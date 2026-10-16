<script lang="ts" module>
  let autoChecked = false; // ports are asked once per start; the refresh button asks again
</script>

<script lang="ts">
  import { onDestroy, onMount } from 'svelte';
  import { app } from '$lib/app.svelte';
  import { forgetTx, hasWebSerial, isDesktop, listPorts, probeable, rememberTx, requestPort, watchPorts, type PortEntry } from '$lib/serial/ports';
  import { identifyTx } from '$lib/device/identify';
  import { sleep } from '$lib/device/link';
  import { linkMetrics } from '$lib/link-metrics';
  import LinkGraph from './LinkGraph.svelte';
  import LinkTiles from './LinkTiles.svelte';
  import { TX_BAUDS } from '$lib/device/tx';
  import Icon from './Icon.svelte';
  import PortList from './PortList.svelte';

  let ports = $state<PortEntry[]>([]);
  let selected = $state('');
  let checking = $state(new Set<string>()); // the ports being asked
  let probing = $state(false);

  async function list() {
    ports = await listPorts();
    if (!ports.some((p) => p.key === selected)) selected = ports[0]?.key ?? ''; // a known Tx sorts first
  }

  // asks the adapters whether a TetherLRS Tx is there, all at once (each port is its own conversation); the found ones
  // go to the top with their name as soon as they answer
  async function recognise(only?: Set<string>) {
    if (probing || app.state !== 'idle') return;
    probing = true;
    const todo = ports.filter((x) => probeable(x) && (!only || only.has(x.key)));
    checking = new Set(todo.map((p) => p.key));
    const found = await Promise.all(
      todo.map(async (p) => {
        // a port that never answers must not hold up the others
        const id = await Promise.race([identifyTx(p.port, p.known?.baud).catch(() => undefined), sleep(15000).then(() => undefined)]);
        if (id) rememberTx(p, id.txName, id.baud);
        else forgetTx(p);
        checking.delete(p.key);
        checking = new Set(checking);
        await list();
        return id ? p.key : undefined;
      }),
    );
    probing = false;
    await list();
    const first = found.find(Boolean);
    if (first && !ports.find((p) => p.key === selected && p.kind === 'tlrs')) selected = first;
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

  // a radio plugged in or pulled out: the list follows, and only a new port is asked
  async function portsChanged() {
    const before = new Set(ports.map((p) => p.key));
    await list();
    const fresh = new Set(ports.filter((p) => !before.has(p.key)).map((p) => p.key));
    if (fresh.size) await recognise(fresh);
  }

  let unwatch = () => {};
  onMount(async () => {
    unwatch = watchPorts(() => void portsChanged());
    await list();
    if (!autoChecked) {
      autoChecked = true;
      await recognise();
    }
  });
  onDestroy(() => unwatch());

  const region = $derived(app.param('Region')?.text);

  // the link at a glance, live while this page is open; a measure opens large on the Link page
  const live = $derived(app.connected && app.state === 'connected');
  $effect(() => {
    if (live && !app.streaming) void app.startStats();
  });
  onDestroy(() => {
    if (app.page !== 'device') void app.stopStats();
  });
  const metrics = $derived(linkMetrics(app.stats));
  const shown = $derived(metrics.find((m) => m.key === app.linkMetric) ?? metrics[0]);
</script>

<div class="device">
<section class="card">
  <header>
    <h2>Connection</h2>
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
    <PortList {ports} bind:selected disabled={app.state !== 'idle'} {checking} />
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
      <button class="btn accent" onclick={connect} disabled={!current || app.state !== 'idle' || checking.has(selected)}>
        {app.state === 'connecting' ? 'Connecting…' : 'Connect'}
      </button>
    {/if}
  </footer>
</section>

<section class="radios">
  <div class="card radio">
    <span class="caption">Ground radio (Tx)</span>
    {#if app.connected && app.version}
      <span class="name">{app.version.txName}</span>
      <span class="caption">{app.version.txVersion}</span>
    {:else}
      <span class="name muted">Not connected</span>
      <span class="caption">Pick its port and press Connect.</span>
    {/if}
  </div>
  <div class="card radio">
    <span class="caption">Aircraft radio (Rx)</span>
    {#if app.version?.rxConnected}
      <span class="name">{app.version.rxName}</span>
      <span class="caption">{app.version.rxVersion}</span>
    {:else}
      <span class="name muted">Not connected</span>
      <span class="caption">{app.connected ? 'Power the receiver to read and set its parameters.' : 'Read through the ground radio.'}</span>
    {/if}
  </div>
  <div class="card radio">
    <span class="caption">Link</span>
    {#if app.connected}
      <span class="name">{app.param('Mode')?.text ?? '–'} · {app.param('RF Band')?.text ?? '–'}</span>
      <span class="caption">Region {region ?? '–'}{region === 'EU' ? ': European rules apply' : ''}</span>
    {:else}
      <span class="name muted">–</span>
      <span class="caption">Mode, band and region of the link.</span>
    {/if}
  </div>
</section>
</div>

{#if live}
  <section class="glance">
    <LinkTiles {metrics} selected={shown.key} grid onselect={(k) => (app.linkMetric = k)} />
    <LinkGraph metric={shown} />
  </section>
{/if}

<style>
  .card {
    padding: var(--space-4) var(--space-5) var(--space-5);
  }
  .device > .card {
    display: flex;
    flex-direction: column;
  }
  .device > .card footer {
    margin-top: auto; /* the buttons at the bottom when the card is stretched */
  }
  header {
    display: flex;
    justify-content: space-between;
    align-items: flex-start;
    gap: var(--space-4);
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
  /* the link at a glance fills what is left of the window */
  .glance {
    display: flex;
    flex-direction: column;
    gap: var(--space-3);
    min-height: max(420px, calc(100vh - 470px));
  }
  .glance :global(.graph) {
    flex: 1;
  }
  .device {
    display: grid;
    grid-template-columns: minmax(0, 1fr);
    gap: var(--space-3);
    align-items: stretch; /* the connection as tall as the radios beside it */
  }
  .radios {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: var(--space-3);
  }
  /* wide: the radios in a column beside the connection */
  @container content (min-width: 1000px) {
    .device {
      grid-template-columns: minmax(0, 2fr) minmax(280px, 1fr);
    }
    .radios {
      grid-template-columns: minmax(0, 1fr);
    }
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
