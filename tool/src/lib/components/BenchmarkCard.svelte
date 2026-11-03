<script lang="ts">
  import { app } from '$lib/app.svelte';
  import { DURATIONS, RATES } from '$lib/device/benchmark';
  import Hint from './Hint.svelte';

  // the loopback test on the Link page: the receiver sends back up what came down, the tool counts
  let seconds = $state(10);
  let rate = $state(2000);

  const b = $derived(app.bench);
  const p = $derived(b.result ?? b.progress);
  const pct = $derived(b.running && b.progress ? Math.min(100, Math.round((b.progress.elapsedMs / (seconds * 1000)) * 100)) : 0);
  const canRun = $derived(app.state === 'connected' && !!app.version?.rxConnected && !app.version?.onlyRx);
  const fmt = (n: number | undefined, unit: string) => (n === undefined ? '–' : `${n} ${unit}`);
</script>

<section class="card bench">
  <header>
    <h2>Loopback test<Hint text="The receiver's serial port stays quiet while it runs: the data comes back up the link instead, and the tool counts what comes back, what is lost and the round trip." /></h2>
  </header>
  <div class="controls">
    <label><span class="caption">For</span>
      <select bind:value={seconds} disabled={b.running}>
        {#each DURATIONS as d}<option value={d}>{d} s</option>{/each}
      </select>
    </label>
    <label><span class="caption">Feeding</span>
      <select bind:value={rate} disabled={b.running}>
        {#each RATES as r}<option value={r}>{r} B/s</option>{/each}
      </select>
    </label>
    {#if b.running}
      <button class="btn" onclick={() => app.stopBenchmark()}>Stop</button>
    {:else}
      <button class="btn accent" onclick={() => app.startBenchmark({ seconds, rateBps: rate })} disabled={!canRun}>Start</button>
    {/if}
  </div>
  {#if b.running}
    <div class="bar" role="progressbar" aria-valuenow={pct} aria-valuemin="0" aria-valuemax="100"><div style="width: {pct}%"></div></div>
  {/if}
  {#if b.error}
    <p class="caption err">{b.error}</p>
  {/if}
  {#if p}
    <dl>
      <div><dt class="caption">Sent</dt><dd>{p.sent} B{#if b.result}{' · '}{b.result.sentBps} B/s{/if}</dd></div>
      <div><dt class="caption">Back</dt><dd>{p.received} B{#if b.result}{' · '}{b.result.receivedBps} B/s{/if}</dd></div>
      <div><dt class="caption">Lost</dt><dd>{p.lost} B</dd></div>
      <div><dt class="caption">Out of place</dt><dd>{p.errors} B</dd></div>
      <div><dt class="caption">Round trip</dt><dd>{fmt(p.rttMs === undefined ? undefined : Math.round(p.rttMs), 'ms')}</dd></div>
    </dl>
    {#if b.result}
      <p class="caption">
        {#if b.result.received === 0}Nothing came back: is the receiver connected, and on this Tx?{:else if b.result.lost + b.result.errors === 0}Everything came back in order.{:else}The link or the receiver's buffer dropped some: feed less, or use a faster air mode.{/if}
      </p>
    {/if}
  {/if}
  {#if app.version && !app.version.rxConnected && !app.version.onlyRx}
    <p class="caption">Needs a connected receiver.</p>
  {/if}
</section>

<style>
  .bench {
    display: flex;
    flex-direction: column;
    gap: var(--space-3);
    padding: var(--space-4) var(--space-5);
  }
  header {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .controls {
    display: flex;
    align-items: end;
    gap: var(--space-3);
    flex-wrap: wrap;
  }
  .controls label {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .bar {
    height: 4px;
    border-radius: 2px;
    background: var(--stroke);
    overflow: hidden;
  }
  .bar div {
    height: 100%;
    background: var(--accent);
    transition: width 0.2s;
  }
  dl {
    display: flex;
    gap: var(--space-5);
    flex-wrap: wrap;
    margin: 0;
  }
  dl div {
    display: flex;
    flex-direction: column;
  }
  dd {
    margin: 0;
  }
  p {
    margin: 0;
  }
  .err {
    color: var(--err);
  }
</style>
