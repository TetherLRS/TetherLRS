import { describe, expect, it } from 'vitest';
import { DemoSerialPort } from '../serial/demo-port';
import { DeviceLink } from './link';
import { TxSession } from './tx';
import { WordChecker, runLoopback, words } from './benchmark';

describe('the word stream of the loopback test', () => {
  it('counts up in 16 bit words, little endian', () => {
    expect(Array.from(words(0xfffe, 3))).toEqual([0xfe, 0xff, 0xff, 0xff, 0x00, 0x00]);
  });

  it('follows a clean stream, a gap, a byte out of place', () => {
    const c = new WordChecker();
    c.feed(words(0, 100));
    expect([c.lost, c.errors, c.expected]).toEqual([0, 0, 100]);
    c.feed(words(110, 10)); // ten words skipped
    expect([c.lost, c.errors, c.expected]).toEqual([20, 0, 120]);
    c.feed(new Uint8Array([0x99])); // a stray byte, then the stream goes on
    c.feed(words(120, 5));
    expect([c.lost, c.errors, c.expected]).toEqual([20, 1, 125]);
  });
});

describe('the loopback test on the demo Tx', () => {
  it('gets its bytes back from the demo receiver in loopback and lands in the CLI again', async () => {
    const port = new DemoSerialPort();
    const link = new DeviceLink(port as unknown as SerialPort);
    await link.open(57600);
    const tx = new TxSession(link);
    await tx.enterCommandMode();
    expect((await tx.setParam('Rx Test', 1)).ok).toBe(true);
    const seen: number[] = [];
    const r = await runLoopback(link, tx, { seconds: 1, rateBps: 2000 }, (p) => seen.push(p.received));
    expect(r.sent).toBeGreaterThan(1500);
    expect(r.received).toBe(r.sent);
    expect(r.lost + r.errors).toBe(0);
    expect(r.rttMs).toBeGreaterThan(0);
    expect(seen.length).toBeGreaterThan(10);
    expect((await tx.version())?.txName).toBe('DAKEFPV 900 MAX 1W TX'); // the CLI is back
    await link.close();
  }, 15000);
});
