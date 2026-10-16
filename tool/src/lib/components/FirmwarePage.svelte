<script lang="ts">
  import { app } from '$lib/app.svelte';
  import InfoBar from './InfoBar.svelte';

  const txEnvs = $derived(app.manifest ? Object.values(app.manifest).filter((f) => f.env.startsWith('tx-')) : []);
  const info = $derived(app.manifest?.[app.firmwareEnv]);

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
  {#if app.flashLog.length}
    <progress value={app.flashProgress} max="1"></progress>
    <pre class="log mono">{app.flashLog.join('\n')}</pre>
  {/if}
</section>

<InfoBar kind="info" text="Receivers: the first time, upload <env>.bin on the ExpressLRS web page (WiFi). After that they follow the Tx." />

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
