// Parsers for the text the TetherLRS Tx CLI prints (mLRS/CommonTx/cli.h). Pure functions, no I/O.

export type ParamGroup = 'common' | 'tx' | 'rx';
export type ParamKind = 'list' | 'value' | 'phrase';

export interface Param {
  name: string; // as the firmware names it, e.g. "Tx Power"
  group: ParamGroup;
  kind: ParamKind;
  text: string; // the value as shown, e.g. "31 Hz", "0 %", "mlrs.0"
  index?: number; // list parameters: the option index
  unchangeable: boolean;
  unavailable: boolean;
}

export interface ParamOptions {
  list?: { index: number; text: string }[];
  min?: number;
  max?: number;
}

export interface DeviceVersion {
  txName: string;
  txVersion: string;
  rxName?: string;
  rxVersion?: string;
  rxConnected: boolean;
}

export interface LinkStats {
  lq: number; // serial link quality seen by the Tx, %
  lqValid: number;
  rxLq: number; // as the receiver reports it
  rssi1: number;
  rssi2: number;
  rxRssi: number;
  snr1: number;
  snr2: number;
  bytesTx: number; // per second
  bytesRx: number;
  ccaBusy?: number; // EU region: frames held back, channel busy
  limited?: number; // EU region: frames held back, time limits
  txPower?: number; // dBm the Tx transmits with now, lower than set with Dyn Power
}

// the CLI prints list indices as '0' + index, so 10 and above come out as ':' ';' '<' ...
const indexFromChar = (c: string) => c.charCodeAt(0) - 48;

export function paramGroup(name: string): ParamGroup {
  if (name.startsWith('Tx ')) return 'tx';
  if (name.startsWith('Rx ')) return 'rx';
  return 'common';
}

// "p NAME" form the CLI matches: upper case, spaces as underscores
export function cliName(name: string): string {
  return name.toUpperCase().replace(/ /g, '_');
}

// one "  Name = value" line of "pl" or "p name"; undefined for anything else
export function parseParamLine(line: string): Param | undefined {
  const m = /^ {2}(\S.+?) = (.*)$/.exec(line.replace(/\r$/, '')); // option lines "  4 = FSK" have one character
  if (!m) return undefined;
  const name = m[1];
  let rest = m[2];
  const p: Param = { name, group: paramGroup(name), kind: 'value', text: rest, unchangeable: false, unavailable: false };
  if (rest === '- (unavailable)') {
    p.unavailable = true;
    p.text = '';
    return p;
  }
  if (name === 'Bind Phrase') {
    p.kind = 'phrase';
    p.text = rest.split(' ')[0]; // drops the " /e6" note on 2.4 GHz
    return p;
  }
  const li = /^(.*) \[(.)\](\(unchangeable\))?$/.exec(rest);
  if (li) {
    p.kind = 'list';
    p.text = li[1];
    p.index = indexFromChar(li[2]);
    p.unchangeable = li[3] !== undefined;
  }
  return p;
}

export function parseParamList(text: string): Param[] {
  return text.split('\n').map(parseParamLine).filter((p): p is Param => p !== undefined);
}

// output of "p name = ?": the parameter line, then "  i = text" per allowed option, or min and max
export function parseParamOptions(text: string): ParamOptions {
  const o: ParamOptions = {};
  for (const raw of text.split('\n')) {
    const line = raw.replace(/\r$/, '');
    let m = /^ {2}(.) = (.*)$/.exec(line);
    if (m) {
      (o.list ??= []).push({ index: indexFromChar(m[1]), text: m[2] });
      continue;
    }
    m = /^ {2}(min|max): *(-?\d+)$/.exec(line);
    if (m) o[m[1] as 'min' | 'max'] = parseInt(m[2], 10);
  }
  return o;
}

export function parseVersion(text: string): DeviceVersion | undefined {
  const tx = /^ {2}Tx: ([^,\r\n]+), (v[^,\r\n]+)/m.exec(text);
  if (!tx) return undefined;
  const v: DeviceVersion = { txName: tx[1].trim(), txVersion: tx[2].trim(), rxConnected: false };
  const rx = /^ {2}Rx: (.*?)\r?$/m.exec(text);
  if (rx && rx[1] !== 'receiver not connected' && !rx[1].startsWith('-')) {
    const parts = rx[1].split(', ');
    v.rxName = parts[0];
    v.rxVersion = parts[1];
    v.rxConnected = true;
  }
  return v;
}

// one line of the "stats" stream, e.g. " 98(100), 97,-61/-127,-63, 8/ 0; 120, 410; cca 0, lim 2;"
export function parseStatsLine(line: string): LinkStats | undefined {
  const m = /^\s*(-?\d+)\(\s*(-?\d+)\),\s*(-?\d+),\s*(-?\d+)\/\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\/\s*(-?\d+);\s*(\d+),\s*(\d+);(?:\s*cca\s*(\d+),\s*lim\s*(\d+);)?(?:\s*pwr\s*(-?\d+);)?/.exec(line);
  if (!m) return undefined;
  const n = (i: number) => parseInt(m[i], 10);
  const s: LinkStats = {
    lq: n(1), lqValid: n(2), rxLq: n(3), rssi1: n(4), rssi2: n(5), rxRssi: n(6), snr1: n(7), snr2: n(8),
    bytesTx: n(9), bytesRx: n(10),
  };
  if (m[11] !== undefined) {
    s.ccaBusy = n(11);
    s.limited = n(12);
  }
  if (m[13] !== undefined) s.txPower = n(13);
  return s;
}
