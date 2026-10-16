import { describe, expect, it } from 'vitest';
import { DeviceLink } from './link';

// a port whose stream the test ends, as the desktop bridge does when the adapter is pulled out
function fakePort() {
  let ctrl!: ReadableStreamDefaultController<Uint8Array>;
  const port = {
    readable: null as ReadableStream<Uint8Array> | null,
    writable: new WritableStream<Uint8Array>(),
    async open() {
      port.readable = new ReadableStream<Uint8Array>({ start: (c) => void (ctrl = c) });
    },
    async close() {},
  };
  return { port: port as unknown as SerialPort, unplug: () => ctrl.close() };
}

const tick = () => new Promise((r) => setTimeout(r, 10));

describe('a link whose port goes away', () => {
  it('says so when the stream ends by itself', async () => {
    const { port, unplug } = fakePort();
    const link = new DeviceLink(port);
    let lost = 0;
    link.onLost = () => lost++;
    await link.open(115200);
    unplug();
    await tick();
    expect(lost).toBe(1);
    expect(link.isOpen).toBe(false);
  });

  it('not when it is closed on purpose', async () => {
    const { port } = fakePort();
    const link = new DeviceLink(port);
    let lost = 0;
    link.onLost = () => lost++;
    await link.open(115200);
    await link.close();
    await tick();
    expect(lost).toBe(0);
  });
});
