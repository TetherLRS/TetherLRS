import { describe, expect, it } from 'vitest';
import { parseStatsLine } from './device/cli-parse';
import { linkMetrics, optionDbm } from './link-metrics';

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

  it('leaves the aircraft line empty when an mLRS receiver does not tell', () => {
    const power = linkMetrics([stats('100(100),100, -022/ 127,-014,  009/ 127; 00000, 00688; pwr  014; rxp -;')]).find((m) => m.key === 'power')!;
    expect(power.series[1].values).toEqual([undefined]);
    expect(power.readout[1].text).toBe('not told');
  });
});
