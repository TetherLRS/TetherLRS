<script lang="ts" module>
  let autoChecked = false; // ports are asked once per start; the refresh button asks again
</script>

<script lang="ts">
  import { onMount } from 'svelte';
  import { app } from '$lib/app.svelte';
  import { forgetTx, hasWebSerial, isDesktop, listPorts, probeable, rememberTx, requestPort, type PortEntry } from '$lib/serial/ports';
  import { identifyTx } from '$lib/device/identify';
  import { TX_BAUDS } from '$lib/device/tx';
  import Icon from './Icon.svelte';
  import PortList from './PortList.svelte';

  let ports = $state<PortEntry[]>([]);
  let selected = $state('');
  let checking = $state<string | undefined>(); // the port being asked
  let probing = $state(false);

  async function list() {
    ports = await listPorts();
    if (!ports.some((p) => p.key === selected)) selected = ports[0]?.key ?? ''; // a known Tx sorts first
  }

  // asks the adapters whether a TetherLRS Tx is there; the found ones go to the top with their name
  async function recognise() {
    if (probing || app.state !== 'idle') return;
    probing = true;
    let found: string | undefined;
    for (const p of ports.filter(probeable)) {
      if (app.state !== 'idle') break;
      checking = p.key;
      const id = await identifyTx(p.port, p.known?.baud);
      if (id) {
        rememberTx(p, id.txName, id.baud);
        found ??= p.key;
      } else {
        forgetTx(p);
      }
    }
    checking = undefined;
    probing = false;
    await list();
    if (found) selected = found;
  }

  async function refresh() {
    await list();
    await recognise();
  }

  async function add() {
    const port = await requestPort();
    await refresh();
    if (port) selected = ports.find((x) => x.port === port)?.key ?? selected;
  }

  const current = $derived(ports.find((p) => p.key === selected));

  async function connect() {
    const entry = ports.find((p) => p.key === selected);
    if (entry) await app.connect(entry);
  }

  onMount(async () => {
    await list();
    if (!autoChecked) {
      autoChecked = true;
      await recognise();
    }
  });

  const region = $derived(app.param('Region')?.text);
</script>

<section class="card">
  <header>
    <div>
      <h2>Connection</h2>
      <p class="caption">The ground radio on a USB serial adapter. The tool talks to it through its CLI, as SiK programs do with +++.</p>
    </div>
    <div class="actions">
      {#if !isDesktop && hasWebSerial}
        <button class="btn" onclick={add} disabled={app.state !== 'idle'}><Icon name="usb" size={16} />Add port…</button>
      {/if}
      <button class="btn" onclick={refresh} disabled={app.state !== 'idle' || probing} aria-label="Refresh ports"><Icon name="refresh" size={16} /></button>
    </div>
  </header>

  {#if !isDesktop && !hasWebSerial}
    <p class="note">This browser has no Web Serial. Use Chrome or Edge, or the desktop app. The demo Tx works anyway.</p>
  {/if}

  <div class="list">
    <PortList {ports} bind:selected disabled={app.state !== 'idle' || probing} {checking} />
  </div>

  <footer>
    <label class="baud">
      <span class="caption">Baud rate</span>
      <select bind:value={app.baud} disabled={app.state !== 'idle'}>
        <option value={0}>Automatic</option>
        {#each TX_BAUDS as b}
          <option value={b}>{b}</option>
        {/each}
      </select>
    </label>
    <span class="status caption">{probing ? 'Looking for TetherLRS radios on the USB serial adapters…' : app.status}</span>
    {#if app.connected}
      <button class="btn" onclick={() => app.disconnect()} disabled={app.state === 'busy'}>Disconnect</button>
    {:else}
      <button class="btn accent" onclick={connect} disabled={!current || app.state !== 'idle' || probing}>
        {app.state === 'connecting' ? 'Connecting…' : 'Connect'}
      </button>
    {/if}
  </footer>
</section>

{#if app.connected && app.version}
  <section class="radios">
    <div class="card radio">
      <span class="caption">Ground radio (Tx)</span>
      <span class="name">{app.version.txName}</span>
      <span class="caption">{app.version.txVersion}</span>
    </div>
    <div class="card radio">
      <span class="caption">Aircraft radio (Rx)</span>
      {#if app.version.rxConnected}
        <span class="name">{app.version.rxName}</span>
        <span class="caption">{app.version.rxVersion}</span>
      {:else}
        <span class="name muted">Not connected</span>
        <span class="caption">Power the receiver to read and set its parameters.</span>
      {/if}
    </div>
    <div class="card radio">
      <span class="caption">Link</span>
      <span class="name">{app.param('Mode')?.text ?? '–'} · {app.param('RF Band')?.text ?? '–'}</span>
      <span class="caption">Region {region ?? '–'}{region === 'EU' ? ': European rules apply' : ''}</span>
    </div>
  </section>
{/if}

<style>
  .card {
    padding: var(--space-4) var(--space-5) var(--space-5);
  }
  header {
    display: flex;
    justify-content: space-between;
    align-items: flex-start;
    gap: var(--space-4);
  }
  header p {
    margin: var(--space-1) 0 0;
  }
  .actions {
    display: flex;
    gap: var(--space-2);
  }
  .note {
    margin: var(--space-3) 0 0;
    color: var(--warn);
  }
  .list {
    margin: var(--space-4) 0;
  }
  footer {
    display: flex;
    align-items: flex-end;
    gap: var(--space-4);
  }
  .baud {
    display: flex;
    flex-direction: column;
    gap: var(--space-1);
  }
  .status {
    flex: 1;
    padding-bottom: 6px;
  }
  .radios {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: var(--space-3);
  }
  .radio {
    display: flex;
    flex-direction: column;
    gap: 2px;
    padding: var(--space-4);
  }
  .name {
    font-weight: 600;
    margin-top: var(--space-1);
  }
  .muted {
    color: var(--text-2);
  }
</style>
