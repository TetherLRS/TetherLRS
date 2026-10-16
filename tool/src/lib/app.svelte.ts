// The tool's state and what the pages ask of the device. One connection at a time.

import { DeviceLink, sleep } from './device/link';
import { TX_BAUDS, TxSession } from './device/tx';
import type { DeviceVersion, LinkStats, Param, ParamOptions } from './device/cli-parse';
import { rememberTx, type PortEntry } from './serial/ports';
import { loadManifest, type Manifest } from './firmware/manifest';

export type Page = 'device' | 'params' | 'link' | 'terminal' | 'firmware';
export type ConnState = 'idle' | 'connecting' | 'connected' | 'busy';

// the web app on GitHub Pages has the firmware next to it; the desktop app and a local dev server fetch it from there
const PAGES_FIRMWARE = 'https://tetherlrs.github.io/TetherLRS/firmware/latest/';
export const DEFAULT_FIRMWARE_BASE =
  typeof location !== 'undefined' && location.protocol === 'https:' && location.hostname.endsWith('github.io')
    ? new URL('firmware/latest/', location.href).href
    : PAGES_FIRMWARE;

// another source (a local build, a fork's site) is remembered once a list has loaded from it
const SOURCE = 'tetherlrs.firmwareSource';

function savedSource(): string {
  try {
    return localStorage.getItem(SOURCE) || DEFAULT_FIRMWARE_BASE;
  } catch {
    return DEFAULT_FIRMWARE_BASE;
  }
}

function saveSource(base: string): void {
  try {
    if (base === DEFAULT_FIRMWARE_BASE) localStorage.removeItem(SOURCE);
    else localStorage.setItem(SOURCE, base);
  } catch {
    // storage blocked: the default comes back next time
  }
}

class App {
  page = $state<Page>('device');
  state = $state<ConnState>('idle');
  status = $state(''); // one line for the footer
  error = $state('');
  portKey = $state('');
  baud = $state(0); // 0 = try 57600, 115200, 230400
  version = $state<DeviceVersion | undefined>();
  params = $state<Param[]>([]);
  options = $state<Record<string, ParamOptions>>({});
  unsaved = $state(false);
  stats = $state<LinkStats[]>([]);
  streaming = $state(false);
  linkMetric = $state('lq'); // the measure the Link page shows large
  terminal = $state<string[]>([]); // the CLI conversation on the Terminal page

  firmwareBase = $state(savedSource());
  manifest = $state<Manifest | undefined>();
  manifestError = $state(''); // why the list did not load, shown where the source is

  private entry?: PortEntry;
  private link?: DeviceLink;
  private tx?: TxSession;
  private queue: Promise<unknown> = Promise.resolve();

  // one CLI conversation at a time: option prefetch and user edits share the port
  private exclusive<T>(fn: () => Promise<T>): Promise<T> {
    const run = this.queue.then(fn, fn);
    this.queue = run.catch(() => undefined);
    return run;
  }

  get connected() {
    return this.state === 'connected' || this.state === 'busy';
  }

  param(name: string) {
    return this.params.find((p) => p.name === name);
  }

  private fail(e: unknown) {
    this.error = e instanceof Error ? e.message : String(e);
  }

  async connect(entry: PortEntry): Promise<void> {
    await this.disconnect();
    this.entry = entry;
    this.portKey = entry.key;
    this.state = 'connecting';
    this.error = '';
    // a remembered baud rate first, then SiK's 57600 and the others
    const bauds = this.baud ? [this.baud] : [...new Set([entry.known?.baud ?? TX_BAUDS[0], ...TX_BAUDS])];
    for (const baud of bauds) {
      this.status = `Trying ${entry.label} at ${baud} baud…`;
      const link = new DeviceLink(entry.port);
      try {
        await link.open(baud);
      } catch (e) {
        this.fail(e);
        this.state = 'idle';
        this.status = '';
        return;
      }
      const tx = new TxSession(link);
      if (await tx.enterCommandMode()) {
        this.link = link;
        this.tx = tx;
        link.onLost = () => this.lost(link);
        this.state = 'busy'; // connected only once version and parameters are read: nothing else may use the CLI before
        this.status = `Connected to ${entry.label} at ${baud} baud`;
        try {
          await this.refresh();
        } finally {
          this.state = 'connected';
        }
        if (this.version) rememberTx(entry, this.version.txName, baud);
        return;
      }
      await link.close();
    }
    this.state = 'idle';
    this.status = '';
    this.error = 'No TetherLRS Tx answered on this port.';
  }

  async disconnect(): Promise<void> {
    if (this.streaming) await this.stopStats();
    if (this.tx && this.link?.isOpen) {
      try {
        await this.tx.exitCommandMode(); // gives the serial back to the data, as SiK's ATO
      } catch {
        // unplugged
      }
    }
    await this.link?.close();
    this.link = undefined;
    this.tx = undefined;
    this.state = 'idle';
    this.version = undefined;
    this.params = [];
    this.options = {};
    this.unsaved = false;
    this.status = '';
  }

  // the Tx was unplugged: nothing more can go to it, so everything goes back without a word to the port
  private lost(link: DeviceLink): void {
    if (this.link !== link) return;
    this.link = undefined;
    this.tx = undefined;
    this.streaming = false;
    this.statsWanted = false;
    this.state = 'idle';
    this.version = undefined;
    this.params = [];
    this.options = {};
    this.unsaved = false;
    this.status = '';
    this.error = `${this.entry?.label ?? 'The radio'} was unplugged.`;
  }

  async refresh(): Promise<void> {
    if (!this.tx) return;
    this.version = await this.tx.version();
    this.params = await this.tx.params();
    this.options = {};
  }

  async loadOptions(name: string): Promise<ParamOptions | undefined> {
    return this.exclusive(async () => {
      if (!this.tx || !this.connected || this.streaming) return undefined;
      if (!this.options[name]) this.options[name] = await this.tx.options(name);
      return this.options[name];
    });
  }

  async setParam(name: string, value: string | number): Promise<boolean> {
    return this.exclusive(() => this.setParamNow(name, value));
  }

  private async setParamNow(name: string, value: string | number): Promise<boolean> {
    if (!this.tx) return false;
    this.state = 'busy';
    try {
      const r = await this.tx.setParam(name, value);
      if (!r.ok || !r.param) {
        this.error = `${name}: ${r.message ?? 'not accepted'}`;
        return false;
      }
      this.error = r.message ? `${name}: ${r.message}` : '';
      const i = this.params.findIndex((p) => p.name === name);
      if (i >= 0) this.params[i] = r.param;
      this.unsaved = true;
      // allowed values of other parameters can depend on this one
      this.options = {};
      return true;
    } finally {
      this.state = 'connected';
    }
  }

  // common parameters restart Tx and receiver; the Tx is then back in data mode, so the tool goes in again
  async save(): Promise<void> {
    if (!this.tx || !this.link || !this.entry) return;
    this.state = 'busy';
    this.status = 'Saving…';
    try {
      await this.tx.store();
      this.unsaved = false;
      await sleep(1500);
      if (!(await this.tx.enterCommandMode())) {
        this.error = 'Saved, but the Tx did not answer after its restart. Connect again.';
        await this.disconnect();
        return;
      }
      await this.refresh();
      this.status = 'Saved';
    } catch (e) {
      this.fail(e);
    } finally {
      if (this.tx) this.state = 'connected';
    }
  }

  // a CLI command typed by the user; the ones that would end the session go through the pages instead
  async runCommand(cmd: string): Promise<void> {
    const c = cmd.trim();
    if (!this.tx || !c || this.streaming) return;
    const say = (...lines: string[]) => (this.terminal = [...this.terminal, ...lines].slice(-1000));
    if (/^(exit|ato|systemboot|stats)$/i.test(c)) {
      say(`> ${c}`, `  ${c === 'stats' ? 'use the Link page' : c === 'systemboot' ? 'use the Firmware page' : 'use Disconnect'} for this`);
      return;
    }
    if (c === 'pstore') {
      say(`> ${c}`);
      await this.save();
      say(this.error ? `  ${this.error}` : '  stored');
      return;
    }
    await this.exclusive(async () => {
      if (!this.tx) return;
      const out = await this.tx.command(c, 300, 6000);
      // the CLI echoes the command and a ">" line first
      const lines = out.replace(/\r/g, '').split('\n').filter((l, i) => !(i === 0 && l.startsWith(c)) && l !== '>');
      say(`> ${c}`, ...lines.filter((l, i, a) => l !== '' || i < a.length - 1));
    });
    if (/^p\s+\S+\s*=\s*[^?\s]/i.test(c) || /^ATS\d+=/i.test(c)) {
      this.unsaved = true;
      await this.exclusive(() => this.refresh());
    }
  }

  // start and stop come from pages opening and closing in any order: one step at a time, towards the last request
  private statsWanted = false;
  private statsMoving?: Promise<void>;

  startStats(): Promise<void> {
    return this.wantStats(true);
  }

  stopStats(): Promise<void> {
    return this.wantStats(false);
  }

  private wantStats(on: boolean): Promise<void> {
    this.statsWanted = on;
    this.statsMoving ??= (async () => {
      while (this.tx && this.streaming !== this.statsWanted) {
        if (this.statsWanted) {
          this.stats = [];
          this.streaming = true;
          await this.tx.startStats((s) => {
            this.stats = [...this.stats.slice(-119), s]; // one minute at 2 per second
          });
        } else {
          this.streaming = false;
          await this.tx.stopStats();
        }
      }
    })().finally(() => (this.statsMoving = undefined));
    return this.statsMoving;
  }

  async loadFirmwareList(): Promise<void> {
    this.manifestError = '';
    try {
      this.manifest = await loadManifest(this.firmwareBase);
      saveSource(this.firmwareBase);
    } catch (e) {
      this.manifest = undefined;
      this.manifestError = e instanceof Error ? e.message : String(e);
    }
  }

  // the connected Tx to its ROM bootloader by command, the port handed over to the Firmware page's esptool session
  async handOverForFlash(): Promise<void> {
    if (!this.tx) return;
    await this.stopStats();
    await this.tx.systemBoot();
    await this.link?.close();
    this.link = undefined;
    this.tx = undefined;
    this.version = undefined;
    this.params = [];
    this.state = 'idle';
    this.status = '';
  }
}

export const app = new App();
