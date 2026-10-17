<script lang="ts">
  import { onDestroy, onMount } from 'svelte';
  import { app } from '$lib/app.svelte';
  import { randomPhrase, validPhrase } from '$lib/firmware/bindphrase';
  import { byCommand, byMavlink, radios, type Found, type Radio } from '$lib/firmware/radios.svelte';
  import { hasWebSerial, isDesktop, requestPort, watchPorts } from '$lib/serial/ports';
  import Icon from './Icon.svelte';

  // the radios are looked at while the list loads, not after: an address that does not answer takes its time
  let unwatch = () => {};
  onMount(() => {
    if (!app.manifest) void app.loadFirmwareList().then(() => radios.pickEnv());
    void radios.scan();
    unwatch = watchPorts(() => void radios.portsChanged());
  });
  onDestroy(() => {
    unwatch();
    if (app.page !== 'firmware') void radios.release();
  });

  const sel = $derived(radios.selected);
  // identified on a click, or the Tx this app is connected to; every other port waits below until Identify
  const known = $derived(radios.list.filter((r) => r.found));
  const others = $derived(radios.list.filter((r) => !r.found));
  const f = $derived(sel?.found);
  const envs = $derived(app.manifest ? Object.values(app.manifest).filter((x) => x.env.startsWith(radios.role === 'Tx' ? 'tx-' : 'rx-')) : []);
  const info = $derived(app.manifest?.[radios.env]);
  const update = $derived(f?.family === 'TetherLRS' && f.role === radios.role);
  // a new install needs one; an update may keep the phrase the radio has
  const phraseOk = $derived(radios.phrase ? validPhrase(radios.phrase) : update);
  let editSource = $state(false);
  let ptUart = $state(7);
  // a flight controller is reached only through its passthrough
  const fcClosed = $derived(sel?.entry.kind === 'fc' && sel.entry.passthrough === undefined);
  let logBox = $state<HTMLPreElement>();

  $effect(() => {
    void radios.log.length;
    if (logBox) logBox.scrollTop = logBox.scrollHeight;
  });

  function family(x: Found | undefined): string | undefined {
    if (!x) return undefined;
    if (x.family) return x.family;
    if (x.how === 'mavlink') return 'TetherLRS or mLRS';
    return undefined;
  }

  function describe(r: Radio): string {
    if (r.checking) return 'Asking what it runs…';
    const x = r.found;
    if (!x && r.entry.kind === 'fc') {
      return r.entry.passthrough !== undefined ? `Forwards UART${r.entry.passthrough}: Identify finds out what runs there` : 'A flight controller: its passthrough reaches a radio on one of its UARTs';
    }
    if (!x) return r.entry.kind === 'esp' ? 'Its own USB: Identify reads what it runs' : 'Identify finds out what it runs';
    if (x.how === 'rom') return 'In its bootloader, firmware not recognised';
    if (x.how === 'seen') return `TetherLRS ${x.role === 'Rx' ? 'receiver' : 'ground radio'}, found by the Device page`;
    const role = x.role === 'Tx' ? 'ground radio' : x.role === 'Rx' ? 'receiver' : 'radio';
    return [`${family(x)} ${role}`, x.version].filter(Boolean).join(' · ');
  }

  const identifyHow = $derived(
    sel?.entry.kind === 'esp'
      ? 'On its own USB it is read in its bootloader: it restarts there, nothing to press.'
      : 'Asks it what it runs. A TetherLRS radio keeps running. ExpressLRS restarts to say it; one on an ESP32 can be read only from its bootloader: hold its boot button while powering it, then Identify. The same if nothing answers.',
  );

  const connectHow = $derived(
    !sel
      ? ''
      : sel.entry.kind === 'esp'
        ? 'Back up and flash work in its bootloader. It restarts there over its own USB, nothing to press.'
        : byCommand(f)
          ? 'Back up and flash work in its bootloader. A TetherLRS ground radio goes there by command.'
          : byMavlink(f)
            ? 'Back up and flash work in its bootloader. The receiver goes there by a MAVLink command, through a flight controller too.'
            : 'Back up and flash work in its bootloader. Identify it first, or hold its boot button while powering it.',
  );

  async function add() {
    await requestPort();
    await radios.scan();
  }

  function save(data: Uint8Array, name: string) {
    const url = URL.createObjectURL(new Blob([data], { type: 'application/octet-stream' }));
    const a = Object.assign(document.createElement('a'), { href: url, download: name });
    a.click();
    setTimeout(() => URL.revokeObjectURL(url), 10000);
  }

  async function backup() {
    const data = await radios.backup();
    const name = (f?.name ?? sel?.entry.label ?? 'radio').toLowerCase().replace(/[^a-z0-9]+/g, '-');
    if (data) save(data, `${name}-${f?.family?.toLowerCase() ?? 'flash'}-backup.bin`);
  }

  async function loadList() {
    await app.loadFirmwareList();
    radios.pickEnv();
    editSource = false;
  }
</script>

<div class="fw">
  <section class="card list">
    <header>
      <h2>Radios</h2>
      <div class="actions">
        {#if !isDesktop && hasWebSerial}
          <button class="btn" onclick={add} disabled={radios.busy}><Icon name="usb" size={16} />Add port…</button>
        {/if}
        <button class="btn" onclick={() => radios.scan()} disabled={radios.busy} aria-label="Look again"><Icon name="refresh" size={16} /></button>
      </div>
    </header>
    {#snippet row(r: Radio)}
      <label class="row" class:checked={r.entry.key === radios.key}>
        <input type="radio" name="fw-radio" checked={r.entry.key === radios.key} onchange={() => radios.select(r.entry.key)} disabled={radios.busy} />
        <span class="text">
          <span class="title">
            <span>{r.entry.label}</span>
            {#if family(r.found)}<span class="badge" class:other={r.found?.family !== 'TetherLRS'}>{family(r.found)}</span>{/if}
            {#if r.found?.name}<span class="kind">{r.found.name}</span>{/if}
          </span>
          <span class="caption">{describe(r)} · {r.entry.via}</span>
        </span>
      </label>
    {/snippet}
    {#if radios.busy && sel}
      <p class="caption busy-note">Working on {sel.entry.label}: another radio can be picked when it is done.</p>
    {/if}
    <div class="rows" role="radiogroup" aria-label="Radio">
      {#each known as r (r.entry.key)}
        {@render row(r)}
      {:else}
        <p class="caption empty">
          {others.length ? 'None identified yet: pick a port below and Identify.' : 'No ports. Plug a radio in by its own USB, or on a USB serial adapter: TX to RX, RX to TX, 5 V and GND.'}
        </p>
      {/each}
      {#if others.length}
        <div class="group">
          <span class="group-title">Other ports</span>
          <span class="caption">Not identified yet: TetherLRS, mLRS or ExpressLRS shows up above once Identify reads it.</span>
        </div>
        {#each others as r (r.entry.key)}
          {@render row(r)}
        {/each}
      {/if}
    </div>
  </section>

  <section class="card detail">
    {#if sel}
      <div class="head">
        <h2>{f?.name ?? sel.entry.label}</h2>
        <span class="caption">{describe(sel)}{radios.chip ? ` · ${radios.chip}` : ''}</span>
      </div>

      {#if sel.entry.kind === 'fc'}
        <div class="step">
          <span class="n">0</span>
          <div class="body">
            <span class="step-title">Passthrough</span>
            <span class="caption">
              {sel.entry.passthrough !== undefined
                ? `It forwards UART${sel.entry.passthrough} at 115200 until it is powered off and on: the radio there is reached as on an adapter.`
                : "Its CLI's serialpassthrough makes it a pipe to the UART of the radio, at 115200. Nothing is saved; it forwards until it is powered off and on."}
            </span>
            <div class="line">
              <select class="uart" bind:value={ptUart} disabled={radios.busy || sel.entry.passthrough !== undefined} aria-label="UART">
                {#each [1, 2, 3, 4, 5, 6, 7, 8] as u (u)}<option value={u}>UART{u}</option>{/each}
              </select>
              <button class="btn" class:accent={fcClosed} onclick={() => radios.passthrough(ptUart)} disabled={radios.busy || !fcClosed}>Open passthrough</button>
            </div>
          </div>
        </div>
      {/if}

      <div class="step">
        <span class="n">1</span>
        <div class="body">
          <span class="step-title">Identify</span>
          <span class="caption">{identifyHow}</span>
          <div class="line">
            <button class="btn" class:accent={!f} onclick={() => radios.identify()} disabled={radios.busy || radios.inRom || fcClosed}>Identify</button>
          </div>
        </div>
      </div>

      <div class="step">
        <span class="n">2</span>
        <div class="body">
          <span class="step-title">Bootloader</span>
          <span class="caption">{radios.inRom ? `In its bootloader${radios.chip ? `, ${radios.chip}` : ''}. Restart leaves it without flashing.` : connectHow}</span>
          <div class="line">
            {#if radios.inRom}
              <button class="btn" onclick={() => radios.restart()} disabled={radios.busy}>Restart</button>
            {:else}
              <button class="btn" class:accent={!!f} onclick={() => radios.connect()} disabled={radios.busy || fcClosed}>Connect</button>
            {/if}
          </div>
        </div>
      </div>

      <div class="step" class:off={!radios.inRom}>
        <span class="n">3</span>
        <div class="body">
          <span class="step-title">Back up</span>
          <span class="caption">Everything on it now, to a file, to go back to it. Before the first TetherLRS; an update can skip it.</span>
          <div class="line">
            <button class="btn" onclick={backup} disabled={!radios.inRom || radios.busy}>Back up to a file</button>
          </div>
        </div>
      </div>

      <div class="step" class:off={!radios.inRom}>
        <span class="n">4</span>
        <div class="body">
          <span class="step-title">TetherLRS</span>
          <div class="seg" role="group" aria-label="Use as">
            <button class:on={radios.role === 'Rx'} aria-pressed={radios.role === 'Rx'} onclick={() => radios.setRole('Rx')} disabled={radios.busy}>Aircraft radio (Rx)</button>
            <button class:on={radios.role === 'Tx'} aria-pressed={radios.role === 'Tx'} onclick={() => radios.setRole('Tx')} disabled={radios.busy}>Ground radio (Tx)</button>
          </div>
          <div class="line phrase">
            <label>
              <span class="caption">Bind phrase</span>
              <input
                class="mono"
                type="text"
                maxlength="6"
                value={radios.phrase}
                oninput={(e) => (radios.phrase = e.currentTarget.value.toLowerCase())}
                placeholder={update ? 'keep its own' : '6 characters'}
                spellcheck="false"
                autocomplete="off"
                aria-invalid={!!radios.phrase && !validPhrase(radios.phrase)}
                disabled={radios.busy}
              />
            </label>
            <button class="btn" onclick={() => (radios.phrase = randomPhrase())} disabled={radios.busy}>Random</button>
            <span class="caption hint" class:err={!!radios.phrase && radios.phrase.length === 6 && !validPhrase(radios.phrase)}>
              The same on both radios of a pair: a to z, 0 to 9, # . _ -. Written into this flash only; the app forgets it when it closes.
            </span>
          </div>
          <div class="line">
            <select bind:value={radios.env} disabled={!envs.length || radios.busy} aria-label="Firmware">
              {#if !envs.length}<option value="">{app.manifest ? 'No build for this role' : 'No firmware list'}</option>{/if}
              {#if envs.length && !radios.env}<option value="">Pick the board</option>{/if}
              {#each envs as x (x.env)}
                <option value={x.env}>{x.device ?? x.env}{x.bands?.length ? ` (${x.bands.join(', ')})` : ''}</option>
              {/each}
            </select>
            <button class="btn accent" onclick={() => radios.flash()} disabled={!radios.inRom || !info || radios.busy || !phraseOk}>{update ? 'Update' : 'Flash'}</button>
          </div>
          <span class="caption">
            {#if info}{info.env} · {info.version} · {Math.round(info['full.bin'].size / 1024)} KB{update ? ` · only the application: settings stay${radios.phrase ? ', the bind phrase changes' : ', the bind phrase too'}` : ''}{#if f?.family === 'ExpressLRS' && f.name}<br />the hardware ExpressLRS calls {f.name}{/if}{#if f?.family === 'TetherLRS' && /^GENERIC/i.test(f.name ?? '')}<br />it runs a build for its chip, not for its board: if its board is in the list, pick that one, with the bands it really has{/if}{:else}The board is chosen from what Identify finds.{/if}
          </span>
        </div>
      </div>

      {#if radios.log.length}
        <div class="progress">
          <progress value={radios.progress} max="1"></progress>
          <pre class="log mono" bind:this={logBox}>{radios.log.join('\n')}</pre>
        </div>
      {/if}
    {:else}
      <p class="caption">Pick a radio.</p>
    {/if}
  </section>

  <section class="card source">
    <span class="caption">Firmware from</span>
    {#if editSource}
      <input type="url" bind:value={app.firmwareBase} spellcheck="false" aria-label="Firmware source" />
      <button class="btn" onclick={loadList}>Load</button>
    {:else}
      <span class="url">{app.firmwareBase}</span>
      {#if app.manifest}<span class="caption">{Object.keys(app.manifest).length} builds</span>{/if}
      <button class="btn" onclick={() => (editSource = true)}>Change</button>
      {#if !app.manifest}<span class="caption note" class:err={app.manifestError}>{app.manifestError || 'Loading the list…'}</span>{/if}
    {/if}
  </section>
</div>

<style>
  /* the radios beside the one being flashed when there is room, the firmware source under the radios */
  .fw {
    display: grid;
    grid-template-columns: minmax(0, 1fr);
    grid-template-areas: 'list' 'detail' 'source';
    gap: var(--space-3);
    align-items: start;
  }
  @container content (min-width: 1000px) {
    .fw {
      grid-template-columns: minmax(0, 1fr) minmax(0, 1.2fr);
      grid-template-areas: 'list detail' 'source detail';
      grid-template-rows: auto 1fr;
    }
  }
  .list {
    grid-area: list;
  }
  .card {
    padding: var(--space-4) var(--space-5) var(--space-5);
  }
  header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: var(--space-3);
    margin-bottom: var(--space-3);
  }
  .actions {
    display: flex;
    gap: var(--space-2);
  }
  .rows {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .row {
    display: flex;
    align-items: center;
    gap: var(--space-3);
    padding: var(--space-2) var(--space-3);
    border-radius: var(--r-control);
  }
  .row:hover {
    background: var(--hover);
  }
  .row.checked {
    background: var(--selected);
  }
  .row input {
    accent-color: var(--accent);
    margin: 0;
  }
  .text {
    display: flex;
    flex-direction: column;
    min-width: 0;
  }
  .title {
    display: flex;
    align-items: center;
    gap: var(--space-2);
    flex-wrap: wrap;
  }
  .kind {
    color: var(--text-2);
  }
  .badge {
    padding: 0 6px;
    border-radius: var(--r-control);
    background: var(--accent);
    color: var(--on-accent);
    font-size: 12px;
    line-height: 18px;
    font-weight: 600;
  }
  .badge.other {
    background: var(--surface-strong);
    color: var(--text);
    border: 1px solid var(--stroke-control);
  }
  .empty {
    margin: 0;
    padding: var(--space-2) var(--space-3);
  }
  .group {
    display: flex;
    flex-direction: column;
    gap: 2px;
    margin-top: var(--space-4);
    padding: 0 var(--space-3) var(--space-1);
    border-top: 1px solid var(--stroke);
    padding-top: var(--space-3);
  }
  .group-title {
    font-weight: 600;
  }
  .busy-note {
    margin: 0 0 var(--space-2);
  }
  .detail {
    grid-area: detail;
    display: flex;
    flex-direction: column;
    gap: var(--space-4);
  }
  .head {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .step {
    display: flex;
    gap: var(--space-3);
  }
  .step.off .body {
    opacity: 0.6;
  }
  .n {
    flex: none;
    width: 24px;
    height: 24px;
    border-radius: 50%;
    border: 1px solid var(--stroke-control);
    display: grid;
    place-items: center;
    font-size: 12px;
    font-weight: 600;
    color: var(--text-2);
  }
  .body {
    display: flex;
    flex-direction: column;
    gap: var(--space-2);
    min-width: 0;
    flex: 1;
  }
  .step-title {
    font-weight: 600;
  }
  .line {
    display: flex;
    align-items: center;
    gap: var(--space-3);
    flex-wrap: wrap;
  }
  select.uart {
    min-width: 0;
    flex: none;
    width: 9em;
  }
  select {
    min-width: 240px;
    flex: 1;
    max-width: 420px;
  }
  .phrase label {
    display: flex;
    flex-direction: column;
    gap: var(--space-1);
  }
  .phrase input {
    width: calc(12ch + 28px); /* the placeholder whole */
  }
  .phrase .btn {
    align-self: flex-end;
  }
  .phrase .hint {
    flex-basis: 100%;
  }
  /* two choices side by side, as a Windows segmented control */
  .seg {
    display: inline-flex;
    align-self: flex-start;
    border: 1px solid var(--stroke-control);
    border-radius: var(--r-control);
    overflow: hidden;
  }
  .seg button {
    font: inherit;
    color: var(--text);
    background: var(--surface-strong);
    border: none;
    padding: 5px 14px;
  }
  .seg button + button {
    border-left: 1px solid var(--stroke-control);
  }
  .seg button.on {
    background: var(--accent);
    color: var(--on-accent);
  }
  .progress {
    display: flex;
    flex-direction: column;
    gap: var(--space-2);
  }
  progress {
    width: 100%;
    height: 4px;
    accent-color: var(--accent);
  }
  .log {
    margin: 0;
    font-size: 12px;
    max-height: 180px;
    overflow: auto;
    padding: var(--space-3);
    border-radius: var(--r-control);
    background: var(--bg);
    border: 1px solid var(--stroke);
    white-space: pre-wrap;
  }
  .source {
    grid-area: source;
    display: flex;
    align-items: center;
    gap: var(--space-3);
    padding: var(--space-3) var(--space-5);
    flex-wrap: wrap;
  }
  .url {
    font-family: var(--mono);
    font-size: 12px;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    min-width: 0;
    flex: 1;
  }
  /* on a line of its own, under the address it is about */
  .note {
    flex-basis: 100%;
  }
  .err {
    color: var(--err);
  }
  .source input {
    flex: 1;
    min-width: 240px;
  }
</style>
