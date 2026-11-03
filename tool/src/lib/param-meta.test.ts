import { describe, expect, it } from 'vitest';
import { parseParamLine } from './device/cli-parse';
import { paramHidden, paramSection } from './param-meta';

const line = (l: string) => parseParamLine(l)!;

describe('which parameters the page shows', () => {
  it('hides what the hardware fixes or lacks, shows the rest', () => {
    expect(paramHidden(line('  Tx Diversity = antenna1 [1](unchangeable)'))).toBe(true);
    expect(paramHidden(line('  Tx Buzzer = - (unavailable)'))).toBe(true);
    expect(paramHidden(line('  Tx Buzzer = LP [1]'))).toBe(false);
    expect(paramHidden(line('  Rx Ser Baudrate = 115200 [1]'))).toBe(false);
    expect(paramHidden(line('  Rx Mav Coalesce = off [0]'))).toBe(false);
  });

  it('puts the rest in link, ground and air', () => {
    expect(paramSection('Region')).toBe('link');
    expect(paramSection('Tx Ser Port2')).toBe('ground');
    expect(paramSection('Rx Snd RadioStat')).toBe('air');
    expect(paramSection('Link Split')).toBe('advanced');
    expect(paramSection('Rx Test')).toBe('advanced');
    expect(paramSection('Rx Count')).toBe('advanced');
    expect(paramSection('Rx Slot')).toBe('advanced');
  });
});
