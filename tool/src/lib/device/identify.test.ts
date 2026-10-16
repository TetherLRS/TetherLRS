import { describe, expect, it } from 'vitest';
import { DemoSerialPort } from '../serial/demo-port';
import { identifyTx } from './identify';

// a port where nothing answers, as an adapter with no radio behind it
class SilentPort {
  readable: ReadableStream<Uint8Array> | null = null;
  writable: WritableStream<Uint8Array> | null = null;
  opens: number[] = [];
  async open(o: SerialOptions) {
    this.opens.push(o.baudRate);
    this.readable = new ReadableStream<Uint8Array>();
    this.writable = new WritableStream<Uint8Array>();
  }
  async close() {
    this.readable = this.writable = null;
  }
  getInfo() {
    return {};
  }
}

describe('recognising a TetherLRS Tx on a port', () => {
  it('finds the demo Tx and leaves it in data mode', async () => {
    const demo = new DemoSerialPort();
    expect(await identifyTx(demo as unknown as SerialPort)).toEqual({ txName: 'DAKEFPV 900 MAX 1W TX', baud: 57600 });
    expect((demo as unknown as { command: boolean }).command).toBe(false);
  }, 15000);

  it('tries the remembered baud first, then gives up after all of them', async () => {
    const port = new SilentPort();
    expect(await identifyTx(port as unknown as SerialPort, 115200)).toBeUndefined();
    expect(port.opens).toEqual([115200, 57600, 230400]);
  }, 20000);
});
