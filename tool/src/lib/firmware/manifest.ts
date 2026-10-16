// Firmware published by the CI (.github/workflows/tetherlrs-firmware.yml): manifest.json, <env>.bin, <env>-full.bin.

export interface FirmwareFile {
  size: number;
  sha256: string;
}

export interface FirmwareInfo {
  env: string;
  version: string;
  chip: string; // esp32, esp32c3, esp32s3, esp8266
  app_offset: number;
  'firmware.bin': FirmwareFile;
  'full.bin': FirmwareFile;
}

export type Manifest = Record<string, FirmwareInfo>;

export async function loadManifest(base: string): Promise<Manifest> {
  const url = new URL('manifest.json', base);
  let r: Response;
  try {
    r = await fetch(url, { cache: 'no-cache' });
  } catch {
    throw new Error(`No firmware list at ${url.origin}: the address cannot be reached, or the site does not allow this app to read it.`);
  }
  if (!r.ok) throw new Error(`No firmware list at ${url}: HTTP ${r.status}.`);
  return r.json();
}

async function sha256Hex(data: Uint8Array): Promise<string> {
  const digest = await crypto.subtle.digest('SHA-256', data);
  return Array.from(new Uint8Array(digest), (b) => b.toString(16).padStart(2, '0')).join('');
}

// full: the image for the ROM bootloader at 0x0, else the application alone
export async function loadImage(base: string, info: FirmwareInfo, full: boolean): Promise<Uint8Array> {
  const name = full ? `${info.env}-full.bin` : `${info.env}.bin`;
  const r = await fetch(new URL(name, base), { cache: 'no-cache' });
  if (!r.ok) throw new Error(`${name}: HTTP ${r.status}`);
  const data = new Uint8Array(await r.arrayBuffer());
  const want = info[full ? 'full.bin' : 'firmware.bin'];
  if (data.length !== want.size || (await sha256Hex(data)) !== want.sha256) throw new Error(`${name}: size or checksum does not match the manifest`);
  return data;
}

// the env a running device belongs to, from the name its CLI reports ("DAKEFPV 900 MAX 1W TX")
export function envForDevice(manifest: Manifest, deviceName: string, isTx: boolean): string | undefined {
  const words = deviceName.toLowerCase().replace(/\b(tx|rx)\b/g, '').split(/[^a-z0-9]+/).filter(Boolean);
  const prefix = isTx ? 'tx-' : 'rx-';
  let best: string | undefined;
  let bestScore = 0;
  for (const env of Object.keys(manifest)) {
    if (!env.startsWith(prefix)) continue;
    const score = words.filter((w) => env.includes(w)).length;
    if (score > bestScore) {
      best = env;
      bestScore = score;
    }
  }
  return best;
}
