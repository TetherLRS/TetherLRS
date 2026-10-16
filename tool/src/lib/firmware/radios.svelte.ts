// The Firmware page: every radio on this computer, what it runs (ExpressLRS, mLRS or TetherLRS), and TetherLRS onto
// it as the aircraft radio or the ground radio.
//
// Looking at the ports changes nothing: "+++" finds a TetherLRS Tx, a MAVLink question that arms nothing finds a
// TetherLRS or mLRS receiver. Identify then takes the radio to its ROM bootloader (its own USB resets it there; a
// TetherLRS Tx by command, a receiver by MAVLink, ExpressLRS by "bl", anything else held there by its boot button),
// keeps that one esptool session open and reads the start of the firmware to say what it is.

import { app } from '$lib/app.svelte';
import { identifyTx } from '$lib/device/identify';
import { DeviceLink } from '$lib/device/link';
import { TxSession } from '$lib/device/tx';
import { forgetTx, listPorts, type PortEntry } from '$lib/serial/ports';
import type { Running } from './detect';
import { elrsToBootloader, romAnswers } from './elrs';
import { chooseEnv, loadImage } from './manifest';
import { probeReceiver, receiverToBootloader } from './mavlink';
import { RomSession, type Progress } from './rom-session';

export interface Found extends Partial<Running> {
  how: 'connected' | 'cli' | 'mavlink' | 'flash' | 'bl' | 'rom' | 'flashed';
  family?: Running['family'];
}

export interface Radio {
  entry: PortEntry;
  found?: Found;
  checking?: boolean;
}

// how a radio gets to its bootloader without its button
export const byCommand = (f?: Found) => f?.family === 'TetherLRS' && f.role === 'Tx';
export const byMavlink = (f?: Found) => f?.how === 'mavlink' || (f?.role === 'Rx' && (f.family === 'TetherLRS' || f.family === 'mLRS'));

const RX_BAUD = 115200; // the receiver's serial as it leaves the factory, and what a flight controller passes through

const within = <T>(p: Promise<T>, ms: number) =>
  Promise.race([p.catch(() => undefined), new Promise<undefined>((r) => setTimeout(() => r(undefined), ms))]);

class Radios {
  list = $state<Radio[]>([]);
  key = $state('');
  role = $state<'Tx' | 'Rx'>('Rx');
  env = $state('');
  busy = $state(false); // a job on the selected radio (identify, back up, flash): the others wait
  inRom = $state(false); // the selected radio sits in its ROM bootloader, the session open
  chip = $state('');
  log = $state<string[]>([]);
  progress = $state(0);

  private session?: RomSession;

  get selected(): Radio | undefined {
    return this.list.find((r) => r.entry.key === this.key);
  }

  private cb(): Progress {
    return {
      log: (l) => (this.log = [...this.log.slice(-200), l]),
      progress: (done, total) => (this.progress = total ? done / total : 0),
    };
  }

  private stale = false; // ports came or went during a job

  private async run(fn: () => Promise<void>): Promise<void> {
    if (this.busy) return;
    this.busy = true;
    app.error = '';
    try {
      await fn();
    } catch (e) {
      app.error = e instanceof Error ? e.message : String(e);
    } finally {
      this.busy = false;
    }
    if (this.stale) void this.portsChanged();
  }

  private ports = async () => (await listPorts()).filter((p) => p.kind !== 'fc' && p.kind !== 'demo');

  // the ports, nothing asked: what each one runs comes out of Identify, on the user's click
  async scan(): Promise<void> {
    if (this.busy) return;
    await this.release();
    app.error = '';
    this.log = [];
    this.progress = 0;
    this.stale = false;
    this.list = (await this.ports()).map((entry) => this.seed({ entry }));
    if (!this.list.some((r) => r.entry.key === this.key)) this.key = this.list[0]?.entry.key ?? '';
    this.pick();
  }

  // the Tx this app is connected to is known already
  private seed(r: Radio): Radio {
    if (app.connected && app.portKey === r.entry.key && app.version) {
      r.found = { family: 'TetherLRS', role: 'Tx', name: app.version.txName.replace(/ TX$/i, ''), version: app.version.txVersion, how: 'connected' };
    }
    return r;
  }

  // a radio plugged in or pulled out: the ones still there keep what is known of them
  async portsChanged(): Promise<void> {
    if (this.busy) {
      this.stale = true; // after the job
      return;
    }
    this.stale = false;
    const ports = await this.ports();
    const there = new Set(ports.map((p) => p.key));
    if (!there.has(this.key) && this.session) {
      this.cb().log('unplugged');
      await this.release();
    }
    const known = new Map(this.list.map((r) => [r.entry.key, r]));
    this.list = ports.map((entry) => known.get(entry.key) ?? this.seed({ entry }));
    if (!there.has(this.key)) {
      this.key = this.list[0]?.entry.key ?? '';
      this.pick();
    }
  }

  select(key: string): void {
    if (key === this.key || this.busy) return;
    void this.release();
    this.key = key;
    this.pick();
  }

  // to its ROM bootloader, one session open, and what its flash holds
  async identify(): Promise<void> {
    const r = this.selected;
    if (!r) return;
    this.log = [];
    this.progress = 0;
    await this.run(async () => {
      await this.release();
      const e = r.entry;
      const native = e.kind === 'esp';
      let elrsName: string | undefined;
      let romBaud = 115200;
      if (!native) {
        this.cb().log('asking it to restart into its bootloader');
        if (app.connected && app.portKey === e.key) {
          await app.handOverForFlash();
        } else if (byCommand(r.found)) {
          await this.txToBootloader(e);
        } else if (byMavlink(r.found)) {
          if (!(await receiverToBootloader(e.port, RX_BAUD))) throw new Error('The receiver stopped answering. Power it off and on, then Refresh.');
        } else {
          romBaud = await this.discover(r);
          elrsName = r.found?.how === 'bl' ? r.found.name : undefined;
        }
      }
      this.cb().log(native ? 'resetting it into its bootloader over USB' : `in its bootloader at ${romBaud} baud, connecting`);
      try {
        this.session = await RomSession.open(e.port, native, this.cb(), romBaud);
      } catch (err) {
        this.cb().log(err instanceof Error ? err.message : String(err));
        throw new Error(
          native
            ? 'It did not come up in its bootloader. Unplug it, hold its boot button while plugging it back in, then Refresh and Identify.'
            : 'It did not come up in its bootloader. Power it off, hold its boot button while powering it on, then Refresh and Identify.',
        );
      }
      this.inRom = true;
      this.chip = this.session.chip;
      this.cb().log(`${this.chip}, ${this.session.flashSize / 1048576} MB of flash (id ${this.session.flashId.toString(16)}); reading its firmware`);
      const found = await this.session.identify(this.cb());
      this.logRetries(this.session);
      r.found = found ? { ...found, how: 'flash' } : elrsName ? { family: 'ExpressLRS', role: 'Rx', name: elrsName, how: 'bl' } : { how: 'rom' };
      this.progress = 0;
      this.pick();
    });
  }

  // how often the link spoiled something, then reset for the next job
  private logRetries(s: RomSession): void {
    if (s.retries) this.cb().log(`${s.retries} time${s.retries > 1 ? 's' : ''} it arrived spoiled and was done again: the adapter or its wires lose bytes`);
    s.retries = 0;
  }

  // what a port nothing is known of runs, asked on Identify, to its ROM: in it already by its boot button (asked
  // first, as the ROM keeps the rate of the first bytes it hears), a TetherLRS Tx (+++, then systemboot), a TetherLRS or
  // mLRS receiver (MAVLink), ExpressLRS ("bl"). Returns the rate the ROM answers at.
  private async discover(r: Radio): Promise<number> {
    const e = r.entry;
    r.checking = true;
    try {
      const rom = await this.romRate(e.port);
      if (rom) return rom;
      this.cb().log('asking what it runs');
      const tx = await within(identifyTx(e.port, e.known?.baud), 12000);
      if (tx) {
        r.found = { family: 'TetherLRS', role: 'Tx', name: tx.txName.replace(/ TX$/i, ''), how: 'cli' };
        this.cb().log(`a TetherLRS Tx, ${r.found.name}: to its bootloader by command`);
        await this.txToBootloader(e);
        return 115200;
      }
      if (await within(probeReceiver(e.port, RX_BAUD), 5000)) {
        r.found = { role: 'Rx', how: 'mavlink' };
        this.cb().log('a TetherLRS or mLRS receiver: to its bootloader by MAVLink');
        if (!(await receiverToBootloader(e.port, RX_BAUD))) throw new Error('The receiver stopped answering. Power it off and on, then Identify.');
        return 115200;
      }
      const name = (await within(elrsToBootloader(e.port), 4000)) ?? undefined;
      if (name) r.found = { family: 'ExpressLRS', role: 'Rx', name, how: 'bl' };
      const after = await this.romRate(e.port);
      if (after) return after;
      throw new Error(
        name
          ? `It says it is ${name}, but its bootloader did not answer: power it off, hold its boot button while powering it on, then Identify.`
          : "Nothing answered. Hold its boot button while powering it on, then Identify. Check that the adapter's TX goes to its RX and RX to its TX.",
      );
    } finally {
      r.checking = false;
    }
  }

  // the rate its ROM answers at: 115200, or 420000 when an ExpressLRS "bl" reached it first
  private async romRate(port: SerialPort): Promise<number | undefined> {
    for (const baud of [115200, 420000]) if (await within(romAnswers(port, baud), 3000)) return baud;
    return undefined;
  }

  private async txToBootloader(e: PortEntry): Promise<void> {
    const link = new DeviceLink(e.port);
    await link.open(e.known?.baud ?? 115200);
    try {
      const tx = new TxSession(link);
      if (!(await tx.enterCommandMode())) throw new Error('The Tx did not answer. Refresh and try again.');
      await tx.systemBoot();
    } finally {
      await link.close();
    }
  }

  async backup(): Promise<Uint8Array | undefined> {
    const s = this.session;
    if (!s) return undefined;
    let data: Uint8Array | undefined;
    this.progress = 0;
    await this.run(async () => {
      this.cb().log(`reading all ${s.flashSize / 1048576} MB`);
      data = await s.backup(this.cb());
      this.cb().log('backup read, every piece checked by MD5');
      this.logRetries(s);
    });
    return data;
  }

  // TetherLRS onto it; over TetherLRS in the same role only the application, so its settings and its bind stay
  async flash(): Promise<void> {
    const r = this.selected;
    const s = this.session;
    const info = app.manifest?.[this.env];
    if (!r || !s || !info) return;
    this.progress = 0;
    await this.run(async () => {
      const update = r.found?.family === 'TetherLRS' && r.found.role === this.role;
      this.cb().log(`downloading ${info.env} ${info.version}`);
      const image = await loadImage(app.firmwareBase, info, !update);
      this.cb().log(update ? 'writing the application, the settings stay' : 'writing the whole image');
      await s.write(image, update ? info.app_offset : 0, this.cb());
      this.logRetries(s);
      const started = await s.close(true);
      this.session = undefined;
      this.inRom = false;
      forgetTx(r.entry); // what the Device page remembered of this port no longer holds
      this.cb().log(started ? 'done, it starts TetherLRS' : 'done: power it off and on to start TetherLRS');
      r.found = { family: 'TetherLRS', role: this.role, name: info.device, version: info.version, how: 'flashed' };
    });
  }

  setRole(role: 'Tx' | 'Rx'): void {
    this.role = role;
    this.pickEnv();
  }

  // the role it has now, and the firmware for it
  private pick(): void {
    const f = this.selected?.found;
    if (f?.role) this.role = f.role;
    this.pickEnv();
  }

  pickEnv(): void {
    if (!app.manifest) return;
    const f = this.selected?.found;
    if (!f?.family) {
      this.env = ''; // nothing known of this radio yet: Identify chooses, never the board of the radio picked before
      return;
    }
    const chosen = chooseEnv(app.manifest, this.role, f.family, f.name);
    const other = this.env ? `${this.role.toLowerCase()}-${this.env.slice(3)}` : ''; // the same board in the other role
    this.env = chosen ?? (app.manifest[other] ? other : '');
  }

  // leaving the radio in its bootloader: the session goes, the radio stays there until it is powered off and on
  async release(): Promise<void> {
    const s = this.session;
    this.session = undefined;
    this.inRom = false;
    this.chip = '';
    if (s) await s.close(false);
  }
}

export const radios = new Radios();
