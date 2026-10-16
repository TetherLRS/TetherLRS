import { afterEach, describe, expect, it, vi } from 'vitest';
import { chooseEnv, loadImage, loadManifest, type Manifest } from './manifest';

const image = new Uint8Array([1, 2, 3, 4, 5]);
// sha256 of the five bytes above
const SHA = '74f81fe167d99b4cb41d6d0ccda82278caee9f3e2f25d5e5a3936ff3dcec60d0';

const manifest: Manifest = {
  'tx-dakefpv-900-max-1w': {
    env: 'tx-dakefpv-900-max-1w', version: 'v0.1.0', chip: 'esp32c3', app_offset: 0x10000, role: 'Tx', device: 'DAKEFPV 900 MAX 1W',
    'firmware.bin': { size: 5, sha256: SHA }, 'full.bin': { size: 5, sha256: SHA },
  },
  'rx-dakefpv-900-max-1w': {
    env: 'rx-dakefpv-900-max-1w', version: 'v0.1.0', chip: 'esp32c3', app_offset: 0x10000,
    'firmware.bin': { size: 5, sha256: SHA }, 'full.bin': { size: 5, sha256: SHA },
  },
  'rx-dakefpv-900-nanopro': {
    env: 'rx-dakefpv-900-nanopro', version: 'v0.1.0', chip: 'esp32c3', app_offset: 0x10000, role: 'Rx', device: 'DAKEFPV 900 NANOPRO',
    'firmware.bin': { size: 5, sha256: SHA }, 'full.bin': { size: 5, sha256: SHA },
  },
  'rx-bayck-ur500': {
    env: 'rx-bayck-ur500', version: 'v0.1.0', chip: 'esp32c3', app_offset: 0x10000, role: 'Rx', device: 'BAYCK UR500',
    'firmware.bin': { size: 5, sha256: SHA }, 'full.bin': { size: 5, sha256: SHA },
  },
  'rx-generic-2400': {
    env: 'rx-generic-2400', version: 'v0.1.0', chip: 'esp8266', app_offset: 0, role: 'Rx', device: 'GENERIC 2400',
    'firmware.bin': { size: 5, sha256: SHA }, 'full.bin': { size: 5, sha256: SHA },
  },
};

afterEach(() => vi.unstubAllGlobals());

function serve(files: Record<string, Uint8Array | string>) {
  vi.stubGlobal('fetch', async (url: URL) => {
    const name = String(url).split('/').pop()!;
    const body = files[name];
    if (body === undefined) return new Response('', { status: 404 });
    return new Response(body);
  });
}

describe('firmware list and images', () => {
  it('reads the manifest next to the base URL', async () => {
    serve({ 'manifest.json': JSON.stringify(manifest) });
    expect(Object.keys(await loadManifest('https://x.github.io/T/firmware/latest/'))).toHaveLength(Object.keys(manifest).length);
  });

  it('says where it looked when the site cannot be reached', async () => {
    vi.stubGlobal('fetch', async () => {
      throw new TypeError('Failed to fetch');
    });
    await expect(loadManifest('https://x.github.io/T/firmware/latest/')).rejects.toThrow('No firmware list at https://x.github.io');
  });

  it('takes an image whose size and sha256 match, refuses one that does not', async () => {
    serve({ 'tx-dakefpv-900-max-1w-full.bin': image, 'rx-bayck-ur500.bin': new Uint8Array([1, 2, 3, 4, 6]) });
    expect(await loadImage('https://x/fw/', manifest['tx-dakefpv-900-max-1w'], true)).toEqual(image);
    await expect(loadImage('https://x/fw/', manifest['rx-bayck-ur500'], false)).rejects.toThrow('checksum');
  });

  it('chooses the env for a radio found running something', () => {
    // TetherLRS and mLRS by the DEVICE_NAME they report
    expect(chooseEnv(manifest, 'Tx', 'TetherLRS', 'DAKEFPV 900 MAX 1W')).toBe('tx-dakefpv-900-max-1w');
    expect(chooseEnv(manifest, 'Rx', 'TetherLRS', 'BAYCK UR500')).toBe('rx-bayck-ur500');
    expect(chooseEnv(manifest, 'Rx', 'mLRS', 'GENERIC 2400')).toBe('rx-generic-2400');
    // ExpressLRS products: their own hal, or their layout's generic env
    expect(chooseEnv(manifest, 'Rx', 'ExpressLRS', 'BAYCKRC UR500 Dual Band RX')).toBe('rx-bayck-ur500');
    expect(chooseEnv(manifest, 'Rx', 'ExpressLRS', 'DAKEFPV 900MHz NANOPRO MAX 500mW RX')).toBe('rx-dakefpv-900-nanopro');
    expect(chooseEnv(manifest, 'Rx', 'ExpressLRS', 'Flywoo EL24E TCXO 2.4GHz RX')).toBe('rx-generic-2400');
    // not in this manifest, or not known
    expect(chooseEnv(manifest, 'Tx', 'ExpressLRS', 'Flywoo EL24E TCXO 2.4GHz RX')).toBeUndefined();
    expect(chooseEnv(manifest, 'Rx', 'ExpressLRS', 'Some Future RX')).toBeUndefined();
    expect(chooseEnv(manifest, 'Rx', 'TetherLRS', undefined)).toBeUndefined();
  });
});
