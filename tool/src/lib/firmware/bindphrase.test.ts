import { createHash } from 'node:crypto';
import { existsSync, readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { validPhrase, writeBindPhrase } from './bindphrase';

// an ESP32 application image: 24 byte header with the SHA-256 flag, one data segment holding tlrs_bind, checksum, SHA
function esp32Image(): Uint8Array {
  const data = new Uint8Array(64);
  data.set(new TextEncoder().encode('TLRSBND\0mlrs.0\0\0'), 16);
  new DataView(data.buffer).setUint32(32, 0xffffffff, true);
  const p = 24 + 8 + data.length;
  const end = (p + 1 + 15) & ~15;
  const img = new Uint8Array(end + 32);
  img.set([0xe9, 1]);
  img[23] = 1;
  new DataView(img.buffer).setUint32(24, 0x3c000020, true);
  new DataView(img.buffer).setUint32(28, data.length, true);
  img.set(data, 32);
  let c = 0xef;
  for (const b of data) c ^= b;
  img[end - 1] = c;
  img.set(createHash('sha256').update(img.subarray(0, end)).digest(), end);
  return img;
}

const sha = (img: Uint8Array, end: number) => createHash('sha256').update(img.subarray(0, end)).digest();

describe('the bind phrase in the image', () => {
  it('takes only what mLRS takes', () => {
    expect(validPhrase('abc.9_')).toBe(true);
    expect(validPhrase('ABC.9_')).toBe(false);
    expect(validPhrase('abc')).toBe(false);
    expect(validPhrase('abc 12')).toBe(false);
  });

  it('writes phrase and stamp, then checksum and SHA-256 again', async () => {
    const img = esp32Image();
    const end = img.length - 32;
    await writeBindPhrase(img, 0, true, 'k3y.9x', 0x12345678);
    expect(new TextDecoder().decode(img.subarray(32 + 24, 32 + 31))).toBe('k3y.9x\0');
    expect(new DataView(img.buffer).getUint32(32 + 32, true)).toBe(0x12345678);
    let c = 0xef;
    for (const b of img.subarray(32, 32 + 64)) c ^= b;
    expect(img[end - 1]).toBe(c);
    expect(Buffer.from(img.subarray(end))).toEqual(sha(img, end));
  });

  it('refuses an image without the place, or one already spoiled', async () => {
    const none = esp32Image();
    none[32 + 16] = 0x58; // "XLRSBND"
    none[none.length - 33] ^= 0x58 ^ 0x54;
    await expect(writeBindPhrase(none, 0, true, 'abcdef', 1)).rejects.toThrow(/no place/);
    const spoiled = esp32Image();
    spoiled[40] ^= 1;
    await expect(writeBindPhrase(spoiled, 0, true, 'abcdef', 1)).rejects.toThrow(/does not check out/);
  });

  // the builds of tools/compila_mlrs_wsl.sh: an ESP8285 (app behind the boot loader at 0x1000, no SHA) and a C3
  const pkg = 'D:/personal/Tesi/inav-dev-docs/projects/mlrs/build-wsl/prova/pkg';
  it.skipIf(!existsSync(`${pkg}/tx-generic-2400/full.bin`) || !existsSync(`${pkg}/rx-dakefpv-900-max-1w/full.bin`))('real builds check out before and after', async () => {
    for (const [env, start, esp32] of [['tx-generic-2400', 0x1000, false], ['rx-dakefpv-900-max-1w', 0x10000, true], ['rx-dakefpv-900-max-1w', 0, true]] as const) {
      const img = new Uint8Array(readFileSync(`${pkg}/${env}/${start === 0 && esp32 ? 'firmware' : 'full'}.bin`));
      await writeBindPhrase(img, start, esp32, 'abc123', 0x01020304);
      expect(new TextDecoder().decode(img).includes('TLRSBND\0abc123\0')).toBe(true);
    }
  });
});
