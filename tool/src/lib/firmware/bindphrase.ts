// The bind phrase goes into the image just before flashing (mLRS/Common/tlrs_bind.h), so no image on the web carries
// one anyone could link with. The radio takes it at its first start after that flash. The image's checksum, and the
// SHA-256 an ESP32 image carries, are made again for the changed bytes.

const MAGIC = new TextEncoder().encode('TLRSBND\0');

export const validPhrase = (s: string) => /^[a-z0-9#._-]{6}$/.test(s);

export function randomPhrase(): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  return Array.from(crypto.getRandomValues(new Uint8Array(6)), (b) => chars[b % chars.length]).join('');
}

// any value but the two the firmware reads as "nothing written" (0xFFFFFFFF) or a blank setup could hold
export function randomStamp(): number {
  const v = crypto.getRandomValues(new Uint32Array(1))[0];
  return v === 0 || v === 0xffffffff ? 1 : v;
}

interface Layout {
  segments: { at: number; size: number }[];
  checksumAt: number;
  sha: boolean;
}

// the image at start: an ESP8266's has an 8 byte header, an ESP32's 24 with the "SHA-256 appended" flag in the last
function layout(img: Uint8Array, start: number, esp32: boolean): Layout {
  if (img[start] !== 0xe9 || img[start + 1] === 0 || img[start + 1] > 16) throw new Error('not an ESP application image');
  const v = new DataView(img.buffer, img.byteOffset, img.length);
  const segments: Layout['segments'] = [];
  let p = start + (esp32 ? 24 : 8);
  for (let i = 0; i < img[start + 1]; i++) {
    const size = v.getUint32(p + 4, true);
    if (p + 8 + size > img.length) throw new Error('an image segment runs past its end');
    segments.push({ at: p + 8, size });
    p += 8 + size;
  }
  return { segments, checksumAt: ((p + 1 + 15) & ~15) - 1, sha: esp32 && img[start + 23] === 1 };
}

function checksum(img: Uint8Array, l: Layout): number {
  let c = 0xef;
  for (const s of l.segments) for (let i = s.at; i < s.at + s.size; i++) c ^= img[i];
  return c;
}

function find(img: Uint8Array, from: number, to: number): number[] {
  const at: number[] = [];
  for (let i = img.indexOf(MAGIC[0], from); i >= 0 && i + MAGIC.length <= to; i = img.indexOf(MAGIC[0], i + 1)) {
    if (MAGIC.every((b, k) => img[i + k] === b)) at.push(i);
  }
  return at;
}

// CRC-32/MPEG-2, as the ESP8266 Arduino packer (elf2bin.py) and ESP.checkFlashCRC() compute it
function crc8266(data: Uint8Array): number {
  let crc = 0xffffffff;
  for (const byte of data) {
    crc ^= byte << 24;
    for (let k = 0; k < 8; k++) crc = crc & 0x80000000 ? (crc << 1) ^ 0x04c11db7 : crc << 1;
  }
  return crc >>> 0;
}

// start: where the application image begins in this file (0x1000 on an ESP8266, behind its boot loader)
export async function writeBindPhrase(img: Uint8Array, start: number, esp32: boolean, phrase: string, stamp: number): Promise<void> {
  if (!validPhrase(phrase)) throw new Error('The bind phrase is 6 characters: a to z, 0 to 9, # . _ -');
  const v = new DataView(img.buffer, img.byteOffset, img.length);
  // the ESP8266 packer made the checksum with these two words at zero, then wrote the file's length and CRC into them
  const crcAt = start + 0x10;
  const arduino = !esp32 && start === 0x1000 && v.getUint32(crcAt, true) === img.length;
  const crcWas = v.getUint32(crcAt + 4, true);
  const work = arduino ? img.slice() : img;
  if (arduino) work.fill(0, crcAt, crcAt + 8);
  const l = layout(work, start, esp32);
  if (checksum(work, l) !== work[l.checksumAt] || (arduino && crc8266(work) !== crcWas)) {
    throw new Error('the image does not check out before the bind phrase goes in');
  }
  const at = find(img, start, l.checksumAt).filter((i) => l.segments.some((s) => i >= s.at && i + 20 <= s.at + s.size));
  if (at.length !== 1) throw new Error('This build has no place for the bind phrase: it is older than the tool. Pick a newer one.');
  if (arduino) img.fill(0, crcAt, crcAt + 8);
  const p = at[0] + 8;
  img.set(new TextEncoder().encode(phrase), p);
  img[p + 6] = 0;
  v.setUint32(p + 8, stamp, true);
  img[l.checksumAt] = checksum(img, l);
  if (arduino) {
    const crc = crc8266(img);
    v.setUint32(crcAt, img.length, true);
    v.setUint32(crcAt + 4, crc, true);
  }
  if (l.sha) img.set(new Uint8Array(await crypto.subtle.digest('SHA-256', img.subarray(start, l.checksumAt + 1))), l.checksumAt + 1);
}
