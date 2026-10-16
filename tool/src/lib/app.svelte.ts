// The tool's state and what the pages ask of the device. One connection at a time.

import { DeviceLink, sleep } from './device/link';
import { TX_BAUDS, TxSession } from './device/tx';
import type { DeviceVersion, LinkStats, Param, ParamOptions } from './device/cli-parse';
import { rememberTx, type PortEntry } from './serial/ports';
import { flashImage, readWholeFlash } from './firmware/flash';
import { envForDevice, loadImage, loadManifest, type Manifest } from './firmware/manifest';

export type Page = 'device' | 'params' | 'link' | 'firmware';
export type ConnState = 'idle' | 'connecting' | 'connected' | 'busy';

// the web app on GitHub Pages has the firmware next to it; the desktop app and a local dev server fetch it from there
const PAGES_FIRMWARE = 'https://tetherlrs.github.io/TetherLRS/firmware/latest/';
export const DEFAULT_FIRMWARE_BASE =
  typeof location !== 'undefined' && location.protocol === 'https:' && location.hostname.endsWith('github.io')
    ? new URL('firmware/latest/', location.href).href
    : PAGES_FIRMWARE;

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

  firmwareBase = $state(DEFAULT_FIRMWARE_BASE);
  manifest = $state<Manifest | undefined>();
  firmwareEnv = $state('');
  flashProgress = $state(0); // 0..1
  flashLog = $state<string[]>([]);

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
        this.state = 'connected';
        this.status = `Connected to ${entry.label} at ${baud} baud`;
        await this.refresh();
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

  async startStats(): Promise<void> {
    if (!this.tx || this.streaming) return;
    this.stats = [];
    this.streaming = true;
    await this.tx.startStats((s) => {
      this.stats = [...this.stats.slice(-119), s]; // one minute at 2 per second
    });
  }

  async stopStats(): Promise<void> {
    if (!this.tx || !this.streaming) return;
    this.streaming = false;
    await this.tx.stopStats();
  }

  async loadFirmwareList(): Promise<void> {
    this.error = '';
    try {
      this.manifest = await loadManifest(this.firmwareBase);
      if (this.version) this.firmwareEnv = envForDevice(this.manifest, this.version.txName, true) ?? '';
    } catch (e) {
      this.manifest = undefined;
      this.fail(e);
    }
  }

  // the connected Tx goes to its ROM bootloader by command; then the port belongs to esptool-js
  private async toBootloader(): Promise<PortEntry> {
    if (!this.tx || !this.entry) throw new Error('connect to the Tx first');
    const entry = this.entry;
    await this.tx.systemBoot();
    await this.link?.close();
    this.link = undefined;
    this.tx = undefined;
    this.version = undefined;
    this.params = [];
    return entry;
  }

  async backupFlash(): Promise<Uint8Array | undefined> {
    this.flashLog = [];
    this.flashProgress = 0;
    this.state = 'busy';
    try {
      const entry = await this.toBootloader();
      return await readWholeFlash(entry.port, {
        log: (l) => (this.flashLog = [...this.flashLog, l]),
        progress: (w, t) => (this.flashProgress = w / t),
      });
    } catch (e) {
      this.fail(e);
      return undefined;
    } finally {
      this.state = 'idle';
    }
  }

  async flashFirmware(): Promise<void> {
    if (!this.manifest || !this.firmwareEnv) return;
    const info = this.manifest[this.firmwareEnv];
    this.flashLog = [];
    this.flashProgress = 0;
    this.error = '';
    this.state = 'busy';
    try {
      this.flashLog = [`downloading ${info.env} ${info.version}`];
      const image = await loadImage(this.firmwareBase, info, true);
      const entry = await this.toBootloader();
      await flashImage(entry.port, image, 0, {
        log: (l) => (this.flashLog = [...this.flashLog, l]),
        progress: (w, t) => (this.flashProgress = w / t),
      });
      this.flashLog = [...this.flashLog, 'done, the Tx restarts with the new firmware'];
    } catch (e) {
      this.fail(e);
    } finally {
      this.state = 'idle';
    }
  }
}

export const app = new App();
