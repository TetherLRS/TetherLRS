// What a radio runs, read from its flash through the ROM bootloader.
//
// The application image's headers say where its segments are and where it ends. ExpressLRS writes its product name
// and hardware layout right at that end; TetherLRS and mLRS are known by strings in the data segments (loaded below
// 0x40000000). Anything after the end is what an earlier, longer firmware left: a TetherLRS flashed over ExpressLRS
// still has the old ExpressLRS name further on.

export type Family = 'TetherLRS' | 'ExpressLRS' | 'mLRS';

export interface Running {
  family: Family;
  role?: 'Tx' | 'Rx';
  name?: string; // as its firmware calls the board
  version?: string;
}

export type Read = (at: number, size: number) => Promise<Uint8Array>;

export interface ImageLayout {
  segments: { addr: number; at: number; size: number }[];
  end: number;
}

// bytes to text with the same offsets, anything unprintable as \0
function text(img: Uint8Array): string {
  let s = '';
  for (let i = 0; i < img.length; i += 0x8000) {
    s += String.fromCharCode(...Array.from(img.subarray(i, i + 0x8000), (c) => (c >= 32 && c < 127 ? c : 0)));
  }
  return s;
}

// the ESP8266's application sits behind its 4 KB boot loader, the ESP32's in its first app partition
const appOffset = (chipName: string) => (chipName === 'ESP8266' ? 0x1000 : 0x10000);

export async function readLayout(read: Read, chipName: string): Promise<ImageLayout | undefined> {
  const start = appOffset(chipName);
  const esp32 = chipName !== 'ESP8266';
  const head = await read(start, esp32 ? 24 : 8);
  if (head[0] !== 0xe9 || head[1] === 0 || head[1] > 16) return undefined;
  const segments: ImageLayout['segments'] = [];
  let p = start + head.length;
  for (let i = 0; i < head[1]; i++) {
    const h = await read(p, 8);
    const v = new DataView(h.buffer, h.byteOffset, 8);
    const size = v.getUint32(4, true);
    if (size > 0x1000000) return undefined;
    segments.push({ addr: v.getUint32(0, true), at: p + 8, size });
    p += 8 + size;
  }
  let end = (p + 1 + 15) & ~15; // the checksum byte, then padding to 16
  if (esp32 && head[23] === 1) end += 32; // its SHA-256
  return { segments, end };
}

// ExpressLRS's product name (128 bytes), lua name (16), options (512), then the hardware layout, which names radio_nss
export function parseTail(tail: Uint8Array): Pick<Running, 'name' | 'role'> | undefined {
  const s = text(tail);
  if (!/^\{"[a-z0-9_]+": [^{}\0]*"radio_nss"/.test(s.slice(0x290))) return undefined;
  const name = s.slice(0, s.indexOf('\0') < 0 ? 128 : Math.min(s.indexOf('\0'), 128)).trim();
  const role = /\bTX\b/.test(name) ? 'Tx' : /\bRX\b/.test(name) ? 'Rx' : undefined;
  return { name: name || undefined, role };
}

export const elrsVersion = (data: Uint8Array) => /\b(\d+\.\d+\.\d+) \([0-9a-f]{6}\)/.exec(text(data))?.[1];

// done: only the TetherLRS marker or an mLRS version line settle it before all the data is read
export function parseStrings(data: Uint8Array, done = true): Running | undefined {
  const s = text(data);
  const t = /TetherLRS:(Tx|Rx):([^:\0]*):(v\d+\.\d+\.\d+):/.exec(s);
  if (t) return { family: 'TetherLRS', role: t[1] as 'Tx' | 'Rx', name: t[2], version: t[3] };
  // its Tx CLI answers "v" with "  Tx: <device>, <version>"
  const m = / {2}(Tx|Rx): ([^,\0]+), (v\d+\.\d+\.\d+)/.exec(s);
  if (m) return { family: 'mLRS', role: m[1] as 'Tx' | 'Rx', name: m[2], version: m[3] };
  if (done && /mLRS/.test(s)) return { family: 'mLRS', version: /\bv1\.\d+\.\d+\b/.exec(s)?.[0] };
  return undefined;
}

export async function whatRuns(read: Read, chipName: string, progress?: (done: number, total: number) => void): Promise<Running | undefined> {
  const layout = await readLayout(read, chipName);
  if (!layout) return undefined;
  const elrs = parseTail(await read(layout.end, 0xc00));
  const data = layout.segments.filter((x) => x.addr < 0x40000000);
  const total = data.reduce((n, x) => n + x.size, 0);
  const chunk = 0x10000;
  let got = new Uint8Array(0);
  for (const seg of data) {
    for (let o = 0; o < seg.size; o += chunk) {
      const part = await read(seg.at + o, Math.min(chunk, seg.size - o));
      const all = new Uint8Array(got.length + part.length);
      all.set(got);
      all.set(part, got.length);
      got = all;
      progress?.(got.length, total);
      if (elrs) {
        const version = elrsVersion(got);
        if (version) return { family: 'ExpressLRS', ...elrs, version };
      } else {
        const r = parseStrings(got, false);
        if (r) return r;
      }
    }
  }
  return elrs ? { family: 'ExpressLRS', ...elrs } : parseStrings(got);
}
