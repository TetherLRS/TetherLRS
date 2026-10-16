import products from './elrs-products.json';

// Firmware published by the CI (.github/workflows/tetherlrs-firmware.yml): manifest.json, <env>.bin, <env>-full.bin.

export interface FirmwareFile {
  size: number;
  sha256: string;
}

export interface FirmwareInfo {
  env: string;
  version: string;
  role?: 'Tx' | 'Rx'; // from the marker in the image
  device?: string; // its DEVICE_NAME, as a running TetherLRS or mLRS radio reports it
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

// the env for a radio found running something, in the role chosen for it: TetherLRS and mLRS report the DEVICE_NAME
// the envs carry; an ExpressLRS product goes by the table made from ExpressLRS's targets (tools/tetherlrs_elrs_products.py):
// its own hal here when there is one, else its hardware layout as a generic env
export function chooseEnv(manifest: Manifest, role: 'Tx' | 'Rx', family: string, name: string | undefined): string | undefined {
  if (!name) return undefined;
  const prefix = role === 'Tx' ? 'tx-' : 'rx-';
  if (family !== 'ExpressLRS') {
    const want = name.replace(/ TX$/i, '').toLowerCase();
    return Object.values(manifest).find((f) => f.env.startsWith(prefix) && f.device?.toLowerCase() === want)?.env;
  }
  const product = (products as unknown as Record<string, [string, string]>)[name];
  return product && manifest[prefix + product[0]] ? prefix + product[0] : undefined;
}
