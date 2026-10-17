// The Firmware page: every radio on this computer, what it runs (ExpressLRS, mLRS or TetherLRS), and TetherLRS onto
// it as the aircraft radio or the ground radio.
//
// Looking at the ports changes nothing: listening tells a TetherLRS Tx (its HEARTBEAT) from a receiver (RADIO_STATUS),
// "+++" gives a Tx's name, a MAVLink question that arms nothing confirms a TetherLRS or mLRS receiver. Identify then takes the radio to its ROM bootloader (its own USB resets it there; a
// TetherLRS Tx by command, a receiver by MAVLink, ExpressLRS by "bl", anything else held there by its boot button),
// keeps that one esptool session open and reads the start of the firmware to say what it is.

import { app } from '$lib/app.svelte';
import { identifyTx } from '$lib/device/identify';
import { DeviceLink } from '$lib/device/link';
import { TxSession } from '$lib/device/tx';
import { forget, listPorts, rememberRx, rememberTx, type PortEntry } from '$lib/serial/ports';
import { openPassthrough } from '$lib/serial/passthrough';
import type { Running } from './detect';
import { elrsToBootloader, romAnswers } from './elrs';
import { randomStamp, writeBindPhrase } from './bindphrase';
import { chooseEnv, loadImage } from './manifest';
import { HEARTBEAT, listen, probeReceiver, RADIO_STATUS, receiverToBootloader } from './mavlink';
import { RomSession, type Progress } from './rom-session';

export interface Found extends Partial<Running> {
  how: 'connected' | 'seen' | 'cli' | 'mavlink' | 'flash' | 'bl' | 'rom' | 'flashed';
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

// a flight controller is never asked itself: only the radio behind its passthrough
function closedFc(e: PortEntry): void {
  if (e.kind === 'fc' && e.passthrough === undefined) {
    throw new Error('A flight controller: open the passthrough to the UART of its radio first (step 0).');
  }
}

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
  phrase = $state(''); // the bind phrase for the next flash: kept while the app runs, never stored

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

  private ports = async () => (await listPorts()).filter((p) => p.kind !== 'demo');

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

  // the Tx this app is connected to is known already, and a radio the Device page found answering on this port
  private seed(r: Radio): Radio {
    const { known, receiver } = r.entry;
    if (app.connected && app.portKey === r.entry.key && app.version) {
      r.found = { family: 'TetherLRS', role: 'Tx', name: app.version.txName.replace(/ TX$/i, ''), version: app.version.txVersion, how: 'connected' };
    } else if (known) {
      r.found = { family: 'TetherLRS', role: 'Tx', name: known.txName.replace(/ TX$/i, ''), how: 'seen' };
    } else if (receiver) {
      // MAVLink alone cannot tell TetherLRS from mLRS: only a board read from its flash says it
      r.found = receiver.name ? { family: 'TetherLRS', role: 'Rx', name: receiver.name, how: 'seen' } : { role: 'Rx', how: 'mavlink' };
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

  // what it runs, without restarting it where it can answer: its ROM if it sits there already (boot button), a TetherLRS
  // Tx by +++ (name and version from its CLI), a TetherLRS or mLRS receiver by a MAVLink question that changes nothing.
  // Only ExpressLRS has to restart into its bootloader to say what it is, and a radio on its own USB to be read.
  async identify(): Promise<void> {
    const r = this.selected;
    if (!r) return;
    this.log = [];
    this.progress = 0;
    await this.run(async () => {
      await this.release();
      const e = r.entry;
      closedFc(e);
      if (e.kind === 'esp') return this.openRom(r, 115200);
      if (app.connected && app.portKey === e.key && app.version) {
        this.seed(r);
        this.cb().log('the Tx the Device page is connected to');
        return this.pick();
      }
      r.checking = true;
      try {
        // listening first, which a ROM held by its button does not mind either: a TetherLRS or mLRS radio talks on its
        // own, and the questions at other rates are noise to it
        this.cb().log('listening');
        const heard = (await within(listen(e.port, RX_BAUD, 1500), 4000)) ?? [];
        const sendsHeartbeat = heard.includes(HEARTBEAT);
        const sendsStatus = !sendsHeartbeat && heard.includes(RADIO_STATUS);
        if (!sendsHeartbeat && !sendsStatus) {
          const rom = await this.romRate(e.port); // a ROM keeps the rate of the first bytes it hears
          if (rom) return await this.openRom(r, rom);
        }
        if (!sendsStatus) {
          this.cb().log('asking what it runs');
          const tx = await within(identifyTx(e.port, sendsHeartbeat ? RX_BAUD : e.known?.baud), 12000);
          if (tx) {
            rememberTx(e, tx.txName, tx.baud); // the Device page shows it at once
            r.found = { family: 'TetherLRS', role: 'Tx', name: tx.txName.replace(/ TX$/i, ''), version: tx.txVersion, how: 'cli' };
            this.cb().log(`a TetherLRS Tx, ${r.found.name} ${tx.txVersion ?? ''}: it keeps running`);
            return this.pick();
          }
        }
        if (sendsStatus) this.cb().log('it sends RADIO_STATUS: asking it as a receiver');
        if (await within(probeReceiver(e.port, RX_BAUD), 5000)) {
          rememberRx(e, e.receiver?.name);
          r.found = { role: 'Rx', how: 'mavlink' };
          this.cb().log('a TetherLRS or mLRS receiver: it keeps running, Connect reads which');
          return this.pick();
        }
        const name = (await within(elrsToBootloader(e.port), 4000)) ?? undefined;
        if (name) r.found = { family: 'ExpressLRS', role: 'Rx', name, how: 'bl' };
        const after = await this.romRate(e.port);
        // an ESP8285 restarts into its ROM, which then hears our 115200 first; an ESP32 goes to ExpressLRS's own
        // updater instead, at 420000, which answers like esptool's stub but reads nothing (flash id 0, no read)
        if (name && after === 420000) {
          throw new Error(
            `${name} went to ExpressLRS's own updater, which cannot be read or backed up. For its first TetherLRS: power it off, hold its boot button while powering it on, then Identify. After that TetherLRS restarts by command, no button.`,
          );
        }
        if (after) return await this.openRom(r, after);
        throw new Error(
          name
            ? `It says it is ${name}, but its bootloader did not answer: power it off, hold its boot button while powering it on, then Identify.`
            : "Nothing answered. Hold its boot button while powering it on, then Identify. Check that the adapter's TX goes to its RX and RX to its TX.",
        );
      } finally {
        r.checking = false;
      }
    });
  }

  // to its ROM bootloader by the way it takes, one session open, and what its flash holds: back up and flash need it
  async connect(): Promise<void> {
    const r = this.selected;
    if (!r) return;
    this.log = [];
    this.progress = 0;
    await this.run(async () => {
      await this.release();
      const e = r.entry;
      closedFc(e);
      let romBaud = 115200;
      if (e.kind !== 'esp') {
        if (app.connected && app.portKey === e.key) {
          this.cb().log('asking it to restart into its bootloader');
          await app.handOverForFlash();
        } else if (byCommand(r.found)) {
          this.cb().log('asking it to restart into its bootloader');
          await this.txToBootloader(e);
        } else if (byMavlink(r.found)) {
          this.cb().log('asking it to restart into its bootloader');
          if (!(await receiverToBootloader(e.port, RX_BAUD))) throw new Error('The receiver stopped answering. Power it off and on, then Identify.');
        } else {
          const rom = await this.romRate(e.port);
          if (!rom) throw new Error('It is not in its bootloader: Identify it first, or hold its boot button while powering it on.');
          romBaud = rom;
        }
      }
      await this.openRom(r, romBaud);
    });
  }

  // a flight controller made a pipe to the UART of its radio; it forwards until it is powered off and on
  async passthrough(uart: number): Promise<void> {
    const r = this.selected;
    if (!r) return;
    this.log = [];
    await this.run(async () => {
      this.cb().log(`asking the flight controller to forward UART${uart} at 115200, by its CLI; nothing is saved`);
      const how = await openPassthrough(r.entry.key, r.entry.port, uart);
      this.cb().log(how === 'already' ? 'it forwards already: a radio talks through it' : 'it forwards now, until it is powered off and on');
      const entry = (await this.ports()).find((p) => p.key === r.entry.key);
      if (entry) r.entry = entry;
    });
  }

  // out of its bootloader without flashing: its firmware starts again where the chip allows it without a power cycle
  async restart(): Promise<void> {
    const s = this.session;
    if (!s) return;
    await this.run(async () => {
      this.session = undefined;
      this.inRom = false;
      this.chip = '';
      const started = await s.close(true);
      this.cb().log(started ? 'it runs its firmware again' : 'power it off and on to start its firmware');
    });
  }

  private async openRom(r: Radio, romBaud: number): Promise<void> {
    const e = r.entry;
    const native = e.kind === 'esp';
    this.cb().log(native ? 'resetting it into its bootloader over USB' : `in its bootloader at ${romBaud} baud, connecting`);
    try {
      this.session = await RomSession.open(e.port, native, this.cb(), romBaud);
    } catch (err) {
      this.cb().log(err instanceof Error ? err.message : String(err));
      throw new Error(
        native
          ? 'It did not come up in its bootloader. Unplug it, hold its boot button while plugging it back in, then Connect.'
          : 'It did not come up in its bootloader. Power it off, hold its boot button while powering it on, then Connect.',
      );
    }
    this.inRom = true;
    this.chip = this.session.chip;
    this.cb().log(`${this.chip}, ${this.session.flashSize / 1048576} MB of flash (id ${this.session.flashId.toString(16)}); reading its firmware`);
    let found: Awaited<ReturnType<RomSession['identify']>>;
    try {
      found = await this.session.identify(this.cb());
    } catch (err) {
      await this.release(); // half open is worse than closed: back up and flash would act on what could not be read
      throw new Error(`Its flash could not be read (${err instanceof Error ? err.message : String(err)}). Power it off, hold its boot button while powering it on, then Identify.`);
    }
    this.logRetries(this.session);
    r.found = found ? { ...found, how: 'flash' } : r.found?.how === 'bl' ? r.found : { how: 'rom' };
    if (found?.family === 'TetherLRS' && found.role === 'Rx') rememberRx(e, found.name);
    this.progress = 0;
    this.pick();
  }

  // how often the link spoiled something, then reset for the next job
  private logRetries(s: RomSession): void {
    if (s.retries) this.cb().log(`${s.retries} time${s.retries > 1 ? 's' : ''} it arrived spoiled and was done again: the adapter or its wires lose bytes`);
    s.retries = 0;
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

  // TetherLRS onto it; over TetherLRS in the same role only the application, so its settings stay, and its bind phrase
  // too unless a new one was typed
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
      if (this.phrase) {
        const esp8266 = info.chip === 'esp8266'; // its application sits behind the boot loader in either file
        await writeBindPhrase(image, esp8266 ? 0x1000 : update ? 0 : info.app_offset, !esp8266, this.phrase, randomStamp());
        this.cb().log('bind phrase written into the image: it takes it when it starts');
      } else if (!update) {
        throw new Error('Type the bind phrase first: the same on the ground radio and on the aircraft radio.');
      }
      this.cb().log(update ? `writing the application, the settings stay${this.phrase ? ' but the bind phrase' : ''}` : 'writing the whole image');
      await s.write(image, update ? info.app_offset : 0, this.cb());
      this.logRetries(s);
      const started = await s.close(true);
      this.session = undefined;
      this.inRom = false;
      // what the Device page shows for this port; an update keeps the Tx's serial rate, a new one starts at 115200
      if (this.role === 'Tx') rememberTx(r.entry, info.device ?? info.env, update ? (r.entry.known?.baud ?? 115200) : 115200);
      else rememberRx(r.entry, info.device);
      this.cb().log(started ? 'done, it starts TetherLRS' : 'done: power it off and on to start TetherLRS');
      r.found = { family: 'TetherLRS', role: this.role, name: info.device, version: info.version, how: 'flashed' };
    });
  }

  // a file of the user's onto the radio: a backup of all its flash, or ExpressLRS, mLRS or another image
  async writeFile(data: Uint8Array, address: number): Promise<void> {
    const r = this.selected;
    const s = this.session;
    if (!r || !s) return;
    this.progress = 0;
    await this.run(async () => {
      if (address + data.length > s.flashSize) {
        throw new Error(`The file has ${data.length} bytes: from 0x${address.toString(16)} they do not fit in its ${s.flashSize / 1048576} MB.`);
      }
      this.cb().log(`writing ${data.length} bytes at 0x${address.toString(16)}`);
      await s.write(data, address, this.cb());
      this.logRetries(s);
      const started = await s.close(true);
      this.session = undefined;
      this.inRom = false;
      forget(r.entry); // what runs there now is not known: Identify tells
      r.found = undefined;
      this.cb().log(started ? 'done, it starts what was written' : 'done: power it off and on to start it');
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
