<script lang="ts">
  import { onMount } from 'svelte';
  import { app } from '$lib/app.svelte';
  import { hasWebSerial, isDesktop, listPorts, requestPort, type PortEntry } from '$lib/serial/ports';
  import PortList from './PortList.svelte';
  import Icon from './Icon.svelte';

  const txEnvs = $derived(app.manifest ? Object.values(app.manifest).filter((f) => f.env.startsWith('tx-')) : []);
  const rxEnvs = $derived(app.manifest ? Object.values(app.manifest).filter((f) => f.env.startsWith('rx-')) : []);
  const info = $derived(app.manifest?.[app.firmwareEnv]);
  const rxInfo = $derived(app.manifest?.[app.rxEnv]);

  let rxPorts = $state<PortEntry[]>([]);
  let rxPort = $state('');
  const rxEntry = $derived(rxPorts.find((p) => p.key === rxPort));

  async function listRx() {
    rxPorts = (await listPorts()).filter((p) => p.kind !== 'demo' && p.kind !== 'fc');
    if (!rxPorts.some((p) => p.key === rxPort)) rxPort = rxPorts[0]?.key ?? '';
  }
  onMount(listRx);

  async function addRx() {
    const port = await requestPort();
    await listRx();
    if (port) rxPort = rxPorts.find((p) => p.port === port)?.key ?? rxPort;
  }

  async function rxBackup() {
    if (!rxEntry) return;
    const data = await app.rxBackup(rxEntry);
    if (data) save(data, `${(app.rxTarget || 'elrs-rx').toLowerCase().replace(/[^a-z0-9]+/g, '-')}-backup.bin`);
  }

  function save(data: Uint8Array, name: string) {
    const url = URL.createObjectURL(new Blob([data], { type: 'application/octet-stream' }));
    const a = Object.assign(document.createElement('a'), { href: url, download: name });
    a.click();
    setTimeout(() => URL.revokeObjectURL(url), 10000);
  }

  async function backup() {
    const name = app.version?.txName.toLowerCase().replace(/[^a-z0-9]+/g, '-') ?? 'tx';
    const data = await app.backupFlash();
    if (data) save(data, `${name}-backup.bin`);
  }
</script>

<section class="card">
  <h2>Firmware source</h2>
  <p class="caption">Builds made by the project's GitHub Actions. Any folder with a manifest.json from tools/tetherlrs_package.py works too.</p>
  <div class="line">
    <input type="url" bind:value={app.firmwareBase} spellcheck="false" aria-label="Firmware source" />
    <button class="btn" onclick={() => app.loadFirmwareList()} disabled={app.state === 'busy'}>Load list</button>
  </div>
</section>

<section class="card">
  <h2>Ground radio (Tx)</h2>
  <p class="caption">The Tx goes to its bootloader by command, no button needed. Keep a backup of what it runs now before the first flash.</p>
  <div class="line">
    <select bind:value={app.firmwareEnv} disabled={!txEnvs.length || app.state === 'busy'} aria-label="Firmware target">
      {#if !txEnvs.length}<option value="">Load the list first</option>{/if}
      {#each txEnvs as f}
        <option value={f.env}>{f.env}</option>
      {/each}
    </select>
    {#if info}<span class="caption">{info.version} · {info.chip} · {(info['full.bin'].size / 1024).toFixed(0)} KB</span>{/if}
  </div>
  <div class="line">
    <button class="btn" onclick={backup} disabled={!app.connected || app.state === 'busy'}>Back up current firmware</button>
    <button class="btn accent" onclick={() => app.flashFirmware()} disabled={!app.connected || !info || app.state === 'busy'}>Flash</button>
  </div>
  {#if !app.connected}
    <p class="caption">Connect to the Tx on the Device page first.</p>
  {/if}
</section>

<section class="card">
  <div class="head">
    <div>
      <h2>ExpressLRS receiver</h2>
      <p class="caption">To put TetherLRS on a receiver that still runs ExpressLRS, plugged into a USB serial adapter.</p>
    </div>
    <div class="actions">
      {#if !isDesktop && hasWebSerial}
        <button class="btn" onclick={addRx} disabled={app.rxBusy}><Icon name="usb" size={16} />Add port…</button>
      {/if}
      <button class="btn" onclick={listRx} disabled={app.rxBusy} aria-label="Refresh ports"><Icon name="refresh" size={16} /></button>
    </div>
  </div>
  <PortList ports={rxPorts} bind:selected={rxPort} disabled={app.rxBusy} group="rx-port" />
  <div class="line">
    <button class="btn" onclick={() => rxEntry && app.rxIdentify(rxEntry)} disabled={!rxEntry || app.rxBusy}>Identify</button>
    {#if app.rxTarget}<span class="caption">Answered as {app.rxTarget}, now in its bootloader</span>{/if}
  </div>
  <div class="line">
    <select bind:value={app.rxEnv} disabled={!rxEnvs.length || app.rxBusy} aria-label="Receiver firmware">
      {#if !rxEnvs.length}<option value="">Load the list first</option>{/if}
      {#each rxEnvs as f}
        <option value={f.env}>{f.env}</option>
      {/each}
    </select>
    <button class="btn" onclick={rxBackup} disabled={!app.rxTarget || app.rxBusy}>Back up</button>
    <button class="btn accent" onclick={() => rxEntry && app.rxFlash(rxEntry)} disabled={!app.rxTarget || !rxInfo || app.rxBusy}>Flash</button>
  </div>
</section>

{#if app.flashLog.length}
  <section class="card">
    <h2>Progress</h2>
    <progress value={app.flashProgress} max="1"></progress>
    <pre class="log mono">{app.flashLog.join('\n')}</pre>
  </section>
{/if}

<style>
  .card {
    display: flex;
    flex-direction: column;
    gap: var(--space-3);
    padding: var(--space-4) var(--space-5) var(--space-5);
  }
  .card p {
    margin: 0;
  }
  .head {
    display: flex;
    justify-content: space-between;
    align-items: flex-start;
    gap: var(--space-4);
  }
  .head p {
    margin-top: var(--space-1);
  }
  .actions {
    display: flex;
    gap: var(--space-2);
  }
  .line {
    display: flex;
    align-items: center;
    gap: var(--space-3);
    flex-wrap: wrap;
  }
  input[type='url'] {
    flex: 1;
    min-width: 240px;
  }
  select {
    min-width: 260px;
  }
  progress {
    width: 100%;
    height: 4px;
    accent-color: var(--accent);
  }
  .log {
    margin: 0;
    max-height: 200px;
    overflow: auto;
    padding: var(--space-3);
    border-radius: var(--r-control);
    background: var(--bg);
    border: 1px solid var(--stroke);
    white-space: pre-wrap;
  }
</style>
