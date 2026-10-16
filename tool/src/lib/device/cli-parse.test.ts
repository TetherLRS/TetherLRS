import { describe, expect, it } from 'vitest';
import { cliName, parseParamLine, parseParamList, parseParamOptions, parseStatsLine, parseVersion } from './cli-parse';

describe('parameter lines, as cli.h print_param() writes them', () => {
  it('list value with its index', () => {
    expect(parseParamLine('  Mode = 31 Hz [1]\r')).toMatchObject({ name: 'Mode', group: 'common', kind: 'list', text: '31 Hz', index: 1, unchangeable: false });
  });
  it('indices from 10 up come as the characters after 9', () => {
    expect(parseParamLine('  Tx Power Sw Ch = 14 [:]')?.index).toBe(10);
    expect(parseParamLine('  Rx Out LQ Ch = 16 [<]')?.index).toBe(12);
  });
  it('unchangeable and unavailable', () => {
    expect(parseParamLine('  Tx Diversity = antenna1 [1](unchangeable)')).toMatchObject({ group: 'tx', index: 1, unchangeable: true });
    expect(parseParamLine('  Tx In Mode = - (unavailable)')).toMatchObject({ unavailable: true, text: '' });
  });
  it('bind phrase, with the 2.4 GHz except note dropped', () => {
    expect(parseParamLine('  Bind Phrase = mlrs.0 /e6')).toMatchObject({ kind: 'phrase', text: 'mlrs.0' });
  });
  it('int8 value with unit', () => {
    expect(parseParamLine('  Rx FS Ch3 = -120 %')).toMatchObject({ group: 'rx', kind: 'value', text: '-120 %' });
  });
  it('other lines are not parameters', () => {
    expect(parseParamLine('ConfigId:0')).toBeUndefined();
    expect(parseParamLine('warn: receiver not connected')).toBeUndefined();
    expect(parseParamLine('pl>')).toBeUndefined();
    expect(parseParamLine('  0 = 50 Hz')).toBeUndefined(); // option lines have a one-character "name"
  });
  it('a whole pl output', () => {
    const list = parseParamList('pl>\r\nConfigId:0\r\n  Bind Phrase = mlrs.0\r\n  Mode = FSK [4]\r\n  Region = EU [1]\r\n');
    expect(list.map((p) => p.name)).toEqual(['Bind Phrase', 'Mode', 'Region']);
  });
});

describe('options, versions, stats', () => {
  it('list options with their indices', () => {
    const o = parseParamOptions('ConfigId:0\r\n  Mode = FSK [4]\r\n  4 = FSK\r\n  6 = 18 Hz\r\n  7 = LR\r\n');
    expect(o.list).toEqual([{ index: 4, text: 'FSK' }, { index: 6, text: '18 Hz' }, { index: 7, text: 'LR' }]);
  });
  it('int8 min and max', () => {
    expect(parseParamOptions('  Rx FS Ch1 = 0 %\r\n  min: -120\r\n  max:  120\r\n')).toEqual({ min: -120, max: 120 });
  });
  it('version with and without receiver', () => {
    expect(parseVersion('  Tx: DAKEFPV 900 MAX 1W TX, v1.4.05\r\n  Rx: DAKEFPV 900 NANOPRO, v1.4.05\r\n')).toEqual({
      txName: 'DAKEFPV 900 MAX 1W TX', txVersion: 'v1.4.05', rxName: 'DAKEFPV 900 NANOPRO', rxVersion: 'v1.4.05', rxConnected: true,
    });
    expect(parseVersion('  Tx: X, v1.4.05\r\n  Rx: receiver not connected\r\n')?.rxConnected).toBe(false);
  });
  it('stats line in the firmware number formats, with the EU counters', () => {
    expect(parseStatsLine('098(100),097, -061/-127,-063,  008/ 000; 00120, 00410; cca 0000000003, lim 0000000002;')).toEqual({
      lq: 98, lqValid: 100, rxLq: 97, rssi1: -61, rssi2: -127, rxRssi: -63, snr1: 8, snr2: 0, bytesTx: 120, bytesRx: 410, ccaBusy: 3, limited: 2,
    });
    expect(parseStatsLine('100(100),100, -040/-127,-041,  010/ 000; 00000, 00000;')?.ccaBusy).toBeUndefined();
    expect(parseStatsLine('  starts streaming stats')).toBeUndefined();
  });
  it('CLI names', () => {
    expect(cliName('Tx Ser Baudrate')).toBe('TX_SER_BAUDRATE');
  });
});
