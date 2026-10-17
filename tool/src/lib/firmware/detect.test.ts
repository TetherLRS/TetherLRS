import { existsSync, readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { parseStrings, parseTail, whatRuns } from './detect';

const enc = (s: string) => new TextEncoder().encode(s);
function bytes(size: number, ...parts: [number, string][]): Uint8Array {
  const img = new Uint8Array(size);
  for (const [at, s] of parts) img.set(enc(s), at);
  return img;
}

// an image as the ESP32's ROM sees it at 0x10000: a header, one data segment, one code segment, the ExpressLRS tail
function esp32Image(data: Uint8Array, code: Uint8Array, tail?: Uint8Array): Uint8Array {
  const flash = new Uint8Array(0x10000 + 24 + 16 + data.length + code.length + 64 + 0x1000);
  const head = flash.subarray(0x10000);
  head.set([0xe9, 2]);
  let p = 0x10000 + 24;
  for (const [addr, seg] of [[0x3c000020, data], [0x42000020, code]] as const) {
    new DataView(flash.buffer).setUint32(p, addr, true);
    new DataView(flash.buffer).setUint32(p + 4, seg.length, true);
    flash.set(seg, p + 8);
    p += 8 + seg.length;
  }
  const end = (p + 1 + 15) & ~15;
  if (tail) flash.set(tail, end);
  return flash;
}
const reader = (flash: Uint8Array) => async (at: number, size: number) => flash.slice(at, at + size);

const elrsTail = bytes(0xc00, [0, 'Flywoo EL24E TCXO 2.4GHz RX'], [0x80, 'Flywoo EL24E'], [0x290, '{"serial_rx": 3, "serial_tx": 1, "radio_nss": 15}']);

describe('what a flash holds', () => {
  it('TetherLRS by its marker', () => {
    expect(parseStrings(bytes(0x400, [0x200, 'TetherLRS:Rx:BAYCK UR500:v1.4.05:']))).toEqual({ family: 'TetherLRS', role: 'Rx', name: 'BAYCK UR500', version: 'v1.4.05' });
  });

  it('mLRS by its CLI version line', () => {
    expect(parseStrings(bytes(0x400, [0x300, '  Tx: RM Ranger 2.4G, v1.3.07']))).toEqual({ family: 'mLRS', role: 'Tx', name: 'RM Ranger 2.4G', version: 'v1.3.07' });
  });

  it('ExpressLRS by what it appends', () => {
    expect(parseTail(elrsTail)).toEqual({ name: 'Flywoo EL24E TCXO 2.4GHz RX', role: 'Rx' });
    expect(parseTail(bytes(0xc00, [0x100, 'hello']))).toBeUndefined();
  });

  it('ExpressLRS only right at the end of the image', async () => {
    const elrs = esp32Image(bytes(0x200, [0x20, '3.5.6 (ee188b)']), new Uint8Array(0x100), elrsTail);
    expect(await whatRuns(reader(elrs), 'ESP32-C3')).toEqual({ family: 'ExpressLRS', role: 'Rx', name: 'Flywoo EL24E TCXO 2.4GHz RX', version: '3.5.6' });
    // a shorter TetherLRS over it: the old name is still there, further on
    const over = esp32Image(bytes(0x80, [0x10, 'TetherLRS:Tx:GENERIC 2400:v1.4.05:']), new Uint8Array(0x40));
    over.set(elrsTail, 0x10000 + 0x400);
    expect(await whatRuns(reader(over), 'ESP32-C3')).toMatchObject({ family: 'TetherLRS', role: 'Tx' });
  });

  it('strings only from the data segments, not the code', async () => {
    const img = esp32Image(new Uint8Array(0x80), bytes(0x80, [0x10, 'TetherLRS:Tx:GENERIC 2400:v1.4.05:']));
    expect(await whatRuns(reader(img), 'ESP32-C3')).toBeUndefined();
  });

  it('no image', async () => {
    expect(await whatRuns(reader(new Uint8Array(0x20000).fill(0xff)), 'ESP32-C3')).toBeUndefined();
  });

  // flash dumps of real radios and packaged builds in $TLRS_REAL_IMAGES, read the way the radio is; skipped without them
  const mlrs = process.env.TLRS_REAL_IMAGES ?? '/nonexistent';
  const dump = (p: string) => reader(new Uint8Array(readFileSync(`${mlrs}/${p}`)));
  it.skipIf(!existsSync(`${mlrs}/backup/flywoo-esp8285-20261016-152542.bin`))('the Flywoo as it came: ExpressLRS on an ESP8285', async () => {
    expect(await whatRuns(dump('backup/flywoo-esp8285-20261016-152542.bin'), 'ESP8266')).toMatchObject({ family: 'ExpressLRS', role: 'Rx', name: 'Flywoo EL24E TCXO 2.4GHz RX' });
  });
  it.skipIf(!existsSync(`${mlrs}/backup/bayck-ur500-20261016-144257.bin`))('the UR500 as it came: ExpressLRS 3.5.6 on a C3', async () => {
    expect(await whatRuns(dump('backup/bayck-ur500-20261016-144257.bin'), 'ESP32-C3')).toEqual({ family: 'ExpressLRS', role: 'Rx', name: 'BAYCKRC UR500 Dual Band RX', version: '3.5.6' });
  });
  it.skipIf(!existsSync(`${mlrs}/backup/flywoo-tlrs-su-resti-elrs-20261016.bin`))('the Flywoo with a build from before the marker, ExpressLRS left behind it', async () => {
    expect(await whatRuns(dump('backup/flywoo-tlrs-su-resti-elrs-20261016.bin'), 'ESP8266')).toMatchObject({ family: 'mLRS', role: 'Tx', name: 'GENERIC 2400' });
  });
  it.skipIf(!existsSync(`${mlrs}/pkg/tx-generic-2400/full.bin`))('TetherLRS builds: Tx on an ESP8285, Rx on a C3', async () => {
    expect(await whatRuns(dump('pkg/tx-generic-2400/full.bin'), 'ESP8266')).toMatchObject({ family: 'TetherLRS', role: 'Tx', name: 'GENERIC 2400' });
    expect(await whatRuns(dump('pkg/rx-bayck-ur500/full.bin'), 'ESP32-C3')).toMatchObject({ family: 'TetherLRS', role: 'Rx' });
  });
});
