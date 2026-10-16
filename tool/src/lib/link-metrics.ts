// What the Link page and the Device page's overview show from the Tx's stats lines.

import type { LinkStats } from './device/cli-parse';

export type Series = { name: string; values: (number | undefined)[] };
export type Metric = {
  key: string;
  title: string;
  unit: string;
  readout: { name: string; text: string }[];
  series: Series[];
  floor?: number; // the scale never goes past these
  ceil?: number;
  span: number; // the smallest range shown, so noise does not fill the graph
};

const ok = (v: number | undefined) => (v === undefined || v === 127 || v === -127 ? undefined : v);
const fmt = (v: number | undefined, unit: string) => (v === undefined ? '–' : `${v} ${unit}`);
const mw = (dbm: number) => {
  const v = Math.pow(10, dbm / 10);
  return v < 1 ? `${v.toFixed(2)} mW` : `${Math.round(v)} mW`;
};

export function linkMetrics(stats: LinkStats[]): Metric[] {
  const last = stats.at(-1);
  return [
    {
      key: 'lq',
      title: 'Link quality',
      unit: '%',
      readout: [
        { name: 'Ground', text: fmt(last?.lq, '%') },
        { name: 'Aircraft', text: fmt(last?.rxLq, '%') },
      ],
      series: [
        { name: 'Ground', values: stats.map((s) => s.lq) },
        { name: 'Aircraft', values: stats.map((s) => s.rxLq) },
      ],
      floor: 0,
      ceil: 100,
      span: 20,
    },
    {
      key: 'valid',
      title: 'Valid frames',
      unit: '%',
      readout: [{ name: 'Ground', text: fmt(last?.lqValid, '%') }],
      series: [{ name: 'Ground', values: stats.map((s) => s.lqValid) }],
      floor: 0,
      ceil: 100,
      span: 20,
    },
    {
      key: 'signal',
      title: 'Signal',
      unit: 'dBm',
      readout: [
        { name: 'Ground', text: fmt(ok(last?.rssi1), 'dBm') + (ok(last?.rssi2) !== undefined ? ` · ${last?.rssi2}` : '') },
        { name: 'Aircraft', text: fmt(ok(last?.rxRssi), 'dBm') },
      ],
      series: [
        { name: 'Ground', values: stats.map((s) => ok(s.rssi1)) },
        { name: 'Aircraft', values: stats.map((s) => ok(s.rxRssi)) },
      ],
      span: 20,
    },
    {
      key: 'snr',
      title: 'SNR',
      unit: 'dB',
      readout: [{ name: 'Ground', text: fmt(ok(last?.snr1), 'dB') }],
      series: [{ name: 'Ground', values: stats.map((s) => ok(s.snr1)) }],
      span: 10,
    },
    {
      key: 'power',
      title: 'Transmit power',
      unit: 'dBm',
      readout: [
        { name: 'Ground', text: last?.txPower !== undefined ? `${last.txPower} dBm · ${mw(last.txPower)}` : '–' },
        { name: 'Aircraft', text: last?.rxPower ?? (last ? 'not told' : '–') },
      ],
      series: [{ name: 'Ground', values: stats.map((s) => s.txPower) }],
      span: 10,
    },
    {
      key: 'data',
      title: 'Data',
      unit: 'B/s',
      readout: [
        { name: 'Down', text: fmt(last?.bytesRx, 'B/s') },
        { name: 'Up', text: fmt(last?.bytesTx, 'B/s') },
      ],
      series: [
        { name: 'Down', values: stats.map((s) => s.bytesRx) },
        { name: 'Up', values: stats.map((s) => s.bytesTx) },
      ],
      floor: 0,
      span: 200,
    },
  ];
}

// the range a graph shows: the data, at least `span` wide, rounded to tidy steps, inside floor/ceil
export function metricRange(m: Metric): [number, number] {
  const all = m.series.flatMap((s) => s.values).filter((v): v is number => v !== undefined);
  if (!all.length) return [m.floor ?? 0, m.ceil ?? (m.floor ?? 0) + m.span];
  let lo = Math.min(...all);
  let hi = Math.max(...all);
  if (hi - lo < m.span) {
    const mid = (hi + lo) / 2;
    lo = mid - m.span / 2;
    hi = mid + m.span / 2;
  }
  const step = m.span / 4;
  lo = Math.floor(lo / step) * step;
  hi = Math.ceil(hi / step) * step;
  if (m.floor !== undefined && lo < m.floor) [lo, hi] = [m.floor, Math.max(hi, m.floor + m.span)];
  if (m.ceil !== undefined && hi > m.ceil) [lo, hi] = [Math.min(lo, m.ceil - m.span), m.ceil];
  return [lo, hi];
}

// a polyline over the last minute, 2 points a second, newest at the right
export function polyline(values: (number | undefined)[], lo: number, hi: number, w: number, h: number): string {
  const step = w / 119;
  const x0 = w - (values.length - 1) * step;
  return values
    .map((v, i) => (v === undefined ? undefined : `${(x0 + i * step).toFixed(1)},${(h - ((Math.min(hi, Math.max(lo, v)) - lo) / (hi - lo)) * h).toFixed(1)}`))
    .filter(Boolean)
    .join(' ');
}
