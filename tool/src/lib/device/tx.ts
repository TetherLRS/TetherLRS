// What the tool does with a TetherLRS Tx: SiK-style escape into the CLI, parameters, stats, bootloader.

import { DeviceLink, sleep } from './link';
import {
  cliName,
  parseParamLine,
  parseParamList,
  parseParamOptions,
  parseStatsLine,
  parseVersion,
  type DeviceVersion,
  type LinkStats,
  type Param,
  type ParamOptions,
} from './cli-parse';

export const TX_BAUDS = [57600, 115200, 230400]; // 57600 first, the SiK-style Tx default
const GUARD_MS = 1100; // the firmware wants 1 s of silence before and after "+++"

export interface SetResult {
  ok: boolean;
  message?: string; // the firmware's err: or warn: text
  param?: Param;
}

export class TxSession {
  private statsStop?: () => void;

  constructor(readonly link: DeviceLink) {}

  // true when the CLI answers, also if it already was in command mode
  async enterCommandMode(): Promise<boolean> {
    await sleep(GUARD_MS);
    this.link.take();
    await this.link.write('+++');
    const answer = await this.link.waitFor(/OK\r?\n|\+\+\+/, GUARD_MS + 800);
    if (answer === undefined) return false;
    if (/OK\r?\n/.test(answer)) return true;
    // the CLI echoed "+++", so it was in command mode already: finish that line
    await this.link.write('\r');
    await this.link.waitQuiet(150, 1000);
    return true;
  }

  async command(cmd: string, quietMs = 250, maxMs = 3000): Promise<string> {
    this.link.take();
    await this.link.write(cmd + '\r');
    return this.link.waitQuiet(quietMs, maxMs);
  }

  async version(): Promise<DeviceVersion | undefined> {
    return parseVersion(await this.command('v'));
  }

  async params(): Promise<Param[]> {
    return parseParamList(await this.command('pl', 300, 6000));
  }

  async options(name: string): Promise<ParamOptions> {
    return parseParamOptions(await this.command(`p ${cliName(name)} = ?`));
  }

  async setParam(name: string, value: string | number): Promise<SetResult> {
    const out = await this.command(`p ${cliName(name)} = ${value}`);
    const err = /err: (.*?)\r?$/m.exec(out);
    if (err) return { ok: false, message: err[1] };
    const param = out.split('\n').map(parseParamLine).find((p) => p?.name === name);
    const warn = /warn: (.*?)\r?$/m.exec(out);
    return { ok: param !== undefined, message: warn?.[1], param };
  }

  // the Tx restarts when a common parameter changed, and is then back in data mode
  async store(): Promise<string> {
    return this.command('pstore', 400, 4000);
  }

  async exitCommandMode(): Promise<void> {
    await this.link.write('exit\r');
    await this.link.waitQuiet(150, 1000);
  }

  // C3 and S3 Tx: restarts into the ROM bootloader, on the same serial pins
  async systemBoot(): Promise<void> {
    await this.link.write('systemboot\r');
    await sleep(300);
  }

  async startStats(onStats: (s: LinkStats) => void): Promise<void> {
    let pending = '';
    this.statsStop = this.link.onText((text) => {
      pending += text;
      const lines = pending.split('\n');
      pending = lines.pop() ?? '';
      for (const line of lines) {
        const s = parseStatsLine(line);
        if (s) onStats(s);
      }
    });
    await this.link.write('stats\r');
  }

  async stopStats(): Promise<void> {
    if (!this.statsStop) return;
    this.statsStop();
    this.statsStop = undefined;
    await this.link.write('x'); // any character ends the stream
    await this.link.waitQuiet(150, 1000);
  }
}
