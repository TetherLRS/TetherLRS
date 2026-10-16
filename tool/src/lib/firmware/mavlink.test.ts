import { describe, expect, it } from 'vitest';
import { commandLong, findAcks, HEARTBEAT, RADIO_STATUS, radioMessages, x25 } from './mavlink';

const hex = (b: Uint8Array) => Array.from(b, (x) => x.toString(16).padStart(2, '0')).join('');

describe('MAVLink to the receiver', () => {
  it('the X.25 check value', () => {
    expect(x25(new TextEncoder().encode('123456789'))).toBe(0x6f91);
  });

  it('COMMAND_LONG as the bench script sent it, which the Flywoo receiver accepted', () => {
    expect(hex(commandLong(0, 1))).toBe('fd21000000ffbe4c000000000000000000000000404000008842000000000000000088ac9649f6003344013c14');
  });

  it('a COMMAND_ACK is found in a stream, a broken one is not', () => {
    // COMMAND_ACK 246, result 0, progress 1, result_param2 1234322, as the Flywoo receiver answered
    const p = new Uint8Array(10);
    const v = new DataView(p.buffer);
    v.setUint16(0, 246, true);
    v.setUint8(3, 1);
    v.setInt32(4, 1234322, true);
    const hdr = new Uint8Array([p.length, 0, 0, 7, 51, 68, 77, 0, 0]);
    const body = new Uint8Array([...hdr, ...p]);
    const crc = x25(new Uint8Array([143]), x25(body));
    const frame = new Uint8Array([0xfd, ...body, crc & 0xff, crc >> 8]);
    const stream = new Uint8Array([1, 2, 0xfd, 3, ...frame, 9]);
    expect(findAcks(stream)).toEqual([{ command: 246, result: 0, progress: 1, param2: 1234322 }]);
    const broken = frame.slice();
    broken[12] ^= 1;
    expect(findAcks(broken)).toEqual([]);
  });

  it('a Tx and a receiver told apart by what they send on their own', () => {
    const bytes = (s: string) => new Uint8Array(s.split(' ').map((x) => parseInt(x, 16)));
    // as caught on the bench: the Flywoo Tx's HEARTBEAT, the DAKEFPV 2.4 receiver's RADIO_STATUS
    const heartbeat = bytes('fd 09 00 00 3a 33 44 00 00 00 00 00 00 00 00 08 80 04 03 b7 aa');
    const status = bytes('fd 09 00 00 2d 33 44 6d 00 00 00 00 00 00 12 ff 64 0d ff 6b 01');
    expect(radioMessages(new Uint8Array([0x33, 0xfd, ...heartbeat, 0x44]))).toEqual([HEARTBEAT]);
    expect(radioMessages(new Uint8Array([...status, ...status.subarray(0, 9)]))).toEqual([RADIO_STATUS]);
    const broken = status.slice();
    broken[15] ^= 1;
    expect(radioMessages(broken)).toEqual([]);
  });
});
