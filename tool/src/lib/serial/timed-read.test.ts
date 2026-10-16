import { describe, expect, it } from 'vitest';
import { timedReader } from './timed-read';

describe('reads with a deadline', () => {
  it('a read given up at its deadline hands its bytes to the next call', async () => {
    let ctrl!: ReadableStreamDefaultController<Uint8Array>;
    const reader = new ReadableStream<Uint8Array>({ start: (c) => void (ctrl = c) }).getReader();
    const read = timedReader(reader);
    expect(await read(20)).toBeUndefined();
    ctrl.enqueue(new Uint8Array([0xc0, 0x01]));
    ctrl.enqueue(new Uint8Array([0x08]));
    expect((await read(20))?.value).toEqual(new Uint8Array([0xc0, 0x01]));
    expect((await read(20))?.value).toEqual(new Uint8Array([0x08]));
    expect(await read(20)).toBeUndefined();
  });
});
