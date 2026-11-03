// Parsers for the text the TetherLRS Tx CLI prints (src/tx/cli.h). Pure functions, no I/O.

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
  onlyRx?: boolean; // the receiver's own settings session: no Tx in it
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
  rxPower?: string; // the receiver's power now, as its Rx Power option names it; missing from an mLRS receiver
  scheme?: number; // RADIO_SCHEME_ENUM the pair runs, from a 2.0 Tx
  split?: number; // LINK_SPLIT_ENUM
  mode?: number; // Mode = auto: the mode in use, MODE_ENUM
  band?: [number, number]; // dual band: how much gets through on each band, 0..255
  lanes?: number; // bonded radios: 2 a lane out of each, 1 one frame on both while a channel is poor
  ant?: [number, number]; // valid frames on each antenna, %
  mp?: RxSlotStat[]; // several receivers: each slot's LQ and RSSI as the Tx sees them
}

export interface RxSlotStat {
  slot: number;
  selected: boolean;
  lq?: number; // missing: not connected
  rssi?: number;
}

export const SCHEMES = ['auto', 'single', 'dual redundant', 'dual bonded', 'dual band'];
export const SPLITS = ['balanced', 'downlink', 'uplink'];
export const MODES = ['50 Hz', '31 Hz', '19 Hz', 'FLRC', 'FSK', '19 Hz 7x', '18 Hz', 'LR', '17 Hz', '8 Hz', 'auto'];

// one line of rxlist: "  *0: DAKEFPV 900 NANOPRO, v2.0-dev, LQ 098 %, RSSI -061 dBm", "   1: not connected",
// "  *0: setup pending, LQ ..." for one whose setup data has not come yet; * marks the selected one
export interface RxInfo {
  slot: number;
  selected: boolean;
  connected: boolean;
  name?: string;
  version?: string;
  lq?: number;
  rssi?: number;
}

export function parseRxList(text: string): RxInfo[] {
  const out: RxInfo[] = [];
  for (const line of text.split(/\r?\n/)) {
    const m = /^ {2}([* ])(\d): (.*)$/.exec(line);
    if (!m) continue;
    const r: RxInfo = { slot: parseInt(m[2], 10), selected: m[1] === '*', connected: m[3] !== 'not connected' };
    if (r.connected) {
      const d = /^(?:(.+?), (v\S+)|setup pending), LQ (\d+) %, RSSI (-?\d+) dBm$/.exec(m[3]);
      if (d) {
        if (d[1]) {
          r.name = d[1];
          r.version = d[2];
        }
        r.lq = parseInt(d[3], 10);
        r.rssi = parseInt(d[4], 10);
      }
    }
    out.push(r);
  }
  return out;
}

export interface CustomPlan {
  centreKHz: number;
  spanKHz: number;
  lbt: boolean;
  maxDbm: number;
  channels?: number; // in the current air mode, as the Tx counts them
  bwKHz?: number;
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

// a receiver's own settings session answers "v" with "  Rx: <device>, <version>"
export function parseRxVersion(text: string): DeviceVersion | undefined {
  const rx = /^ {2}Rx: ([^,\r\n]+), (v[^,\r\n]+)/m.exec(text);
  if (!rx) return undefined;
  return { txName: '', txVersion: '', rxName: rx[1].trim(), rxVersion: rx[2].trim(), rxConnected: true, onlyRx: true };
}

// one line of the "stats" stream, e.g. " 98(100), 97,-61/-127,-63, 8/ 0; 120, 410; cca 0, lim 2;"
export function parseStatsLine(line: string): LinkStats | undefined {
  const m = /^\s*(-?\d+)\(\s*(-?\d+)\),\s*(-?\d+),\s*(-?\d+)\/\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\/\s*(-?\d+);\s*(\d+),\s*(\d+);(?:\s*cca\s*(\d+),\s*lim\s*(\d+);)?(?:\s*pwr\s*(-?\d+);)?(?:\s*rxp\s*([^;]*);)?(?:\s*sch\s*(\d);\s*split\s*(\d);)?/.exec(line);
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
  if (m[14] !== undefined && m[14].trim() !== '-') s.rxPower = m[14].trim();
  if (m[15] !== undefined) {
    s.scheme = n(15);
    s.split = n(16);
  }
  // what a 2.0 Tx adds after the split, each only when it applies
  const tail = line.slice(m[0].length);
  const mode = /mode\s*(\d+);/.exec(tail);
  if (mode) s.mode = parseInt(mode[1], 10);
  const band = /band\s*(\d+)\/(\d+);/.exec(tail);
  if (band) s.band = [parseInt(band[1], 10), parseInt(band[2], 10)];
  const lanes = /lanes\s*(\d);/.exec(tail);
  if (lanes) s.lanes = parseInt(lanes[1], 10);
  const ant = /ant\s*(\d+)\/(\d+);/.exec(tail);
  if (ant) s.ant = [parseInt(ant[1], 10), parseInt(ant[2], 10)];
  const mp = /mp((?:\s+\d[*:](?:\d+\/-?\d+|-))+);/.exec(tail);
  if (mp) {
    s.mp = Array.from(mp[1].matchAll(/(\d)([*:])(?:(\d+)\/(-?\d+)|-)/g), (x) => {
      const r: RxSlotStat = { slot: parseInt(x[1], 10), selected: x[2] === '*' };
      if (x[3] !== undefined) {
        r.lq = parseInt(x[3], 10);
        r.rssi = parseInt(x[4], 10);
      }
      return r;
    });
  }
  return s;
}

// "  link key: set", "  link key: not set", after a change "  link key: set, pstore keeps it..." or "cleared, ..."
export function parseLinkKey(text: string): boolean | undefined {
  const m = /link key: (set|not set|cleared)/.exec(text);
  if (!m) return undefined;
  return m[1] === 'set';
}

// "  custom plan: 869525 kHz centre, 250 kHz span, LBT on, 27 dBm max" and "  2 channel(s) of 125 kHz in the current mode"
export function parseCustomPlan(text: string): CustomPlan | undefined {
  const m = /custom plan: (\d+) kHz centre, (\d+) kHz span, (LBT on|no LBT), (\d+) dBm(?: \(\d+ mW\))? max/.exec(text);
  if (!m) return undefined;
  const cp: CustomPlan = { centreKHz: parseInt(m[1], 10), spanKHz: parseInt(m[2], 10), lbt: m[3] === 'LBT on', maxDbm: parseInt(m[4], 10) };
  const c = /(\d+) channel\(s\) of (\d+) kHz/.exec(text);
  if (c) {
    cp.channels = parseInt(c[1], 10);
    cp.bwKHz = parseInt(c[2], 10);
  }
  return cp;
}
