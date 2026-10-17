import { describe, expect, it } from 'vitest';
import { parseStatsLine } from './device/cli-parse';
import { linkMetrics, optionDbm, polylines, resumeHistory } from './link-metrics';

const stats = (l: string) => parseStatsLine(l)!;

describe('transmit power', () => {
  it('reads the receiver option names as dBm', () => {
    expect(optionDbm('10 mW')).toBe(10);
    expect(optionDbm('25 mW')).toBe(14);
    expect(optionDbm('1000 mW')).toBe(30);
    expect(optionDbm('0.02 mW')).toBe(-17);
    expect(optionDbm('-')).toBeUndefined();
    expect(optionDbm(undefined)).toBeUndefined();
  });

  it('draws both radios', () => {
    const lines = [
      '100(100),100, -022/ 127,-014,  009/ 127; 00000, 00688; pwr  014; rxp 10 mW;',
      '100(100),100, -022/ 127,-014,  009/ 127; 00000, 00688; pwr  020; rxp 100 mW;',
    ];
    const power = linkMetrics(lines.map(stats)).find((m) => m.key === 'power')!;
    expect(power.series.map((s) => s.name)).toEqual(['Ground', 'Aircraft']);
    expect(power.series[0].values).toEqual([14, 20]);
    expect(power.series[1].values).toEqual([10, 20]);
    expect(power.readout[1].text).toBe('20 dBm · 100 mW');
  });

  it('reads the last sample, not the gap after it', () => {
    const s = stats('100(100),100, -022/ 127,-014,  009/ 127; 00000, 00688; pwr  014; rxp 10 mW;');
    const power = linkMetrics([s, undefined]).find((m) => m.key === 'power')!;
    expect(power.readout[0].text).toBe('14 dBm · 25 mW');
    expect(power.series[0].values).toEqual([14, undefined]);
  });

  it('leaves the aircraft line empty when an mLRS receiver does not tell', () => {
    const power = linkMetrics([stats('100(100),100, -022/ 127,-014,  009/ 127; 00000, 00688; pwr  014; rxp -;')]).find((m) => m.key === 'power')!;
    expect(power.series[1].values).toEqual([undefined]);
    expect(power.readout[1].text).toBe('not told');
  });
});

describe('the last minute across pages', () => {
  it('splits the line where samples are missing', () => {
    expect(polylines([0, 10, undefined, 10, 0], 0, 10, 119, 10)).toEqual(['115.0,10.0 116.0,0.0', '118.0,0.0 119.0,10.0']);
    expect(polylines([undefined, 5, undefined], 0, 10, 119, 10)).toEqual(['118.0,5.0 118.0,5.0']);
    expect(polylines([undefined, undefined], 0, 10, 119, 10)).toEqual([]);
  });

  it('keeps the history and leaves the time without the stream empty', () => {
    const s = stats('100(100),100, -022/ 127,-014,  009/ 127; 00000, 00688; pwr  014; rxp 10 mW;');
    expect(resumeHistory([s], 0, 5000)).toEqual([s]);
    expect(resumeHistory([s], 1000, 3000)).toEqual([s, undefined, undefined, undefined, undefined]);
    expect(resumeHistory(new Array(120).fill(s), 1000, 2000)).toHaveLength(120);
    expect(resumeHistory([s], 1000, 61000)).toEqual([]);
  });
});
