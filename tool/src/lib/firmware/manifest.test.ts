import { afterEach, describe, expect, it, vi } from 'vitest';
import { envForDevice, loadImage, loadManifest, type Manifest } from './manifest';

const image = new Uint8Array([1, 2, 3, 4, 5]);
// sha256 of the five bytes above
const SHA = '74f81fe167d99b4cb41d6d0ccda82278caee9f3e2f25d5e5a3936ff3dcec60d0';

const manifest: Manifest = {
  'tx-dakefpv-900-max-1w': {
    env: 'tx-dakefpv-900-max-1w', version: 'v0.1.0', chip: 'esp32c3', app_offset: 0x10000,
    'firmware.bin': { size: 5, sha256: SHA }, 'full.bin': { size: 5, sha256: SHA },
  },
  'rx-dakefpv-900-max-1w': {
    env: 'rx-dakefpv-900-max-1w', version: 'v0.1.0', chip: 'esp32c3', app_offset: 0x10000,
    'firmware.bin': { size: 5, sha256: SHA }, 'full.bin': { size: 5, sha256: SHA },
  },
  'rx-dakefpv-900-nanopro': {
    env: 'rx-dakefpv-900-nanopro', version: 'v0.1.0', chip: 'esp32c3', app_offset: 0x10000,
    'firmware.bin': { size: 5, sha256: SHA }, 'full.bin': { size: 5, sha256: SHA },
  },
  'rx-bayck-ur500': {
    env: 'rx-bayck-ur500', version: 'v0.1.0', chip: 'esp32c3', app_offset: 0x10000,
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
    expect(Object.keys(await loadManifest('https://x.github.io/T/firmware/latest/'))).toHaveLength(4);
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

  it('matches device names to envs', () => {
    expect(envForDevice(manifest, 'DAKEFPV 900 MAX 1W TX', true)).toBe('tx-dakefpv-900-max-1w');
    expect(envForDevice(manifest, 'DAKEFPV 900 NANOPRO', false)).toBe('rx-dakefpv-900-nanopro');
    expect(envForDevice(manifest, 'BAYCK UR500', false)).toBe('rx-bayck-ur500');
    expect(envForDevice(manifest, 'SOMETHING ELSE', false)).toBeUndefined();
  });
});
