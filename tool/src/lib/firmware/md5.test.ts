import { createHash } from 'node:crypto';
import { describe, expect, it } from 'vitest';
import { md5 } from './md5';

const enc = (s: string) => new TextEncoder().encode(s);

describe('md5', () => {
  it('the RFC 1321 vectors', () => {
    expect(md5(enc(''))).toBe('d41d8cd98f00b204e9800998ecf8427e');
    expect(md5(enc('abc'))).toBe('900150983cd24fb0d6963f7d28e17f72');
    expect(md5(enc('12345678901234567890123456789012345678901234567890123456789012345678901234567890'))).toBe('57edf4a22be3c955ac49da2e2107b67a');
  });

  it('as node computes it, on every length around the 64 byte block', () => {
    for (const n of [55, 56, 57, 63, 64, 65, 0x4000, 0x10001]) {
      const data = Uint8Array.from({ length: n }, (_, i) => (i * 7 + 3) & 0xff);
      expect(md5(data)).toBe(createHash('md5').update(data).digest('hex'));
    }
  });
});
