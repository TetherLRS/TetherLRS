<script lang="ts">
  import { app } from '$lib/app.svelte';
  import type { LinkStats } from '$lib/device/cli-parse';

  const last = $derived(app.stats.at(-1));
  const eu = $derived(app.param('Region')?.text === 'EU');

  // polyline points for one value over the last minute, 0..100 mapped to the box height
  function spark(values: number[], lo: number, hi: number, w = 240, h = 40): string {
    if (values.length < 2) return '';
    const step = w / 119;
    const x0 = w - (values.length - 1) * step;
    return values.map((v, i) => `${(x0 + i * step).toFixed(1)},${(h - ((Math.min(hi, Math.max(lo, v)) - lo) / (hi - lo)) * h).toFixed(1)}`).join(' ');
  }

  const tiles = $derived(
    [
      { label: 'Link quality', value: last?.lq, unit: '%', series: app.stats.map((s) => s.lq), lo: 0, hi: 100 },
      { label: 'Signal, ground', value: last?.rssi1, unit: 'dBm', series: app.stats.map((s) => s.rssi1), lo: -120, hi: -30 },
      { label: 'Signal, aircraft', value: last?.rxRssi, unit: 'dBm', series: app.stats.map((s) => s.rxRssi), lo: -120, hi: -30 },
      { label: 'SNR, ground', value: last?.snr1, unit: 'dB', series: app.stats.map((s) => s.snr1), lo: -20, hi: 20 },
      { label: 'Data down', value: last?.bytesRx, unit: 'B/s', series: app.stats.map((s) => s.bytesRx), lo: 0, hi: 4200 },
      { label: 'Data up', value: last?.bytesTx, unit: 'B/s', series: app.stats.map((s) => s.bytesTx), lo: 0, hi: 3300 },
    ] as { label: string; value: number | undefined; unit: string; series: number[]; lo: number; hi: number }[],
  );

  const held = (s: LinkStats | undefined) => (s?.ccaBusy ?? 0) + (s?.limited ?? 0);
</script>

<section class="card bar">
  <div>
    <h2>Live statistics</h2>
    <p class="caption">Twice a second from the Tx. The serial port is busy while they run.</p>
  </div>
  {#if app.streaming}
    <button class="btn" onclick={() => app.stopStats()}>Stop</button>
  {:else}
    <button class="btn accent" onclick={() => app.startStats()} disabled={app.state !== 'connected'}>Start</button>
  {/if}
</section>

<section class="tiles">
  {#each tiles as t}
    <div class="card tile">
      <span class="caption">{t.label}</span>
      <span class="value">{t.value ?? '–'}<span class="unit">{t.value !== undefined ? ' ' + t.unit : ''}</span></span>
      <svg viewBox="0 0 240 40" preserveAspectRatio="none" aria-hidden="true">
        <polyline points={spark(t.series, t.lo, t.hi)} />
      </svg>
    </div>
  {/each}
</section>

{#if eu}
  <section class="card eu">
    <h2>EU rules</h2>
    <p class="caption">Frames the Tx held back since it started: channel busy {last?.ccaBusy ?? '–'}, on-time limits {last?.limited ?? '–'}. A few now and then are normal; many mean a crowded channel.</p>
    {#if held(last) > 0 && last}
      <span class="caption">Total {held(last)}</span>
    {/if}
  </section>
{/if}

<style>
  .bar {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: var(--space-4);
    padding: var(--space-4) var(--space-5);
  }
  .bar p,
  .eu p {
    margin: var(--space-1) 0 0;
  }
  .tiles {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
    gap: var(--space-3);
  }
  .tile {
    display: flex;
    flex-direction: column;
    gap: var(--space-1);
    padding: var(--space-3) var(--space-4);
  }
  .value {
    font-family: var(--font-display);
    font-size: 28px;
    line-height: 36px;
    font-weight: 600;
    font-variant-numeric: tabular-nums;
  }
  .unit {
    font-size: 14px;
    font-weight: 400;
    color: var(--text-2);
  }
  svg {
    width: 100%;
    height: 40px;
  }
  polyline {
    fill: none;
    stroke: var(--accent);
    stroke-width: 1.5;
    vector-effect: non-scaling-stroke;
  }
  .eu {
    padding: var(--space-4) var(--space-5);
  }
</style>
