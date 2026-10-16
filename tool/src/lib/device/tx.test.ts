import { describe, expect, it } from 'vitest';
import { DemoSerialPort } from '../serial/demo-port';
import { DeviceLink } from './link';
import { TxSession } from './tx';
import type { LinkStats } from './cli-parse';

async function session() {
  const port = new DemoSerialPort();
  const link = new DeviceLink(port as unknown as SerialPort);
  await link.open(57600);
  return { port, link, tx: new TxSession(link) };
}

describe('a session with the demo Tx', () => {
  it('enters the CLI with +++, also when already there', async () => {
    const { link, tx } = await session();
    expect(await tx.enterCommandMode()).toBe(true);
    expect(await tx.enterCommandMode()).toBe(true);
    await link.close();
  }, 10000);

  it('reads version and parameters, sets and rejects values', async () => {
    const { link, tx } = await session();
    await tx.enterCommandMode();
    expect((await tx.version())?.txName).toBe('DAKEFPV 900 MAX 1W TX');
    const params = await tx.params();
    expect(params.find((p) => p.name === 'Mode')?.text).toBe('31 Hz');
    expect(params.find((p) => p.name === 'RF Ortho')?.unavailable).toBe(true);
    expect((await tx.options('Mode')).list?.map((o) => o.text)).toEqual(['31 Hz', '19 Hz', 'FSK', '19 Hz 7x', '18 Hz', 'LR']);
    const ok = await tx.setParam('Tx Power', 2);
    expect(ok).toMatchObject({ ok: true, param: { text: '100 mW', index: 2 } });
    expect(await tx.setParam('Tx Power', 9)).toMatchObject({ ok: false, message: 'invalid value' });
    expect(await tx.setParam('Bind Phrase', 'abc')).toMatchObject({ ok: false });
    expect((await tx.setParam('Bind Phrase', 'sik025')).param?.text).toBe('sik025');
    await link.close();
  }, 10000);

  it('parameter names as the firmware takes them: blanks dropped, so underscores are needed', async () => {
    const { link, tx } = await session();
    await tx.enterCommandMode();
    expect(await tx.command('p tx power = 2')).toContain('err: invalid parameter name');
    expect(await tx.command('p TX_POWER = 2')).toContain('Tx Power = 100 mW [2]');
    expect(await tx.command('p mode')).toContain('  6 = 18 Hz'); // no "=" lists the options too
    await link.close();
  }, 10000);

  it('EU region narrows the modes', async () => {
    const { link, tx } = await session();
    await tx.enterCommandMode();
    await tx.setParam('Region', 1);
    expect((await tx.options('Mode')).list?.map((o) => o.text)).toEqual(['FSK', '18 Hz', 'LR']);
    await link.close();
  }, 10000);

  it('streams stats until stopped', async () => {
    const { link, tx } = await session();
    await tx.enterCommandMode();
    await tx.setParam('Region', 1);
    const got: LinkStats[] = [];
    await tx.startStats((s) => got.push(s));
    await new Promise((r) => setTimeout(r, 1300));
    await tx.stopStats();
    expect(got.length).toBeGreaterThanOrEqual(2);
    expect(got[0].ccaBusy).toBeDefined();
    const n = got.length;
    await new Promise((r) => setTimeout(r, 700));
    expect(got.length).toBe(n);
    await link.close();
  }, 10000);
});
