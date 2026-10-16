<script lang="ts">
  import { metricRange, polyline, type Metric } from '$lib/link-metrics';

  // one measure over the last minute, as large as its box: scale from the data, grid, values on the axis
  let { metric }: { metric: Metric } = $props();

  const W = 1000;
  const H = 300;
  const range = $derived(metricRange(metric));
  const ticks = $derived([0, 1, 2, 3, 4].map((i) => range[1] - ((range[1] - range[0]) * i) / 4));
</script>

<section class="card graph">
  <header>
    <h2>{metric.title}</h2>
    <div class="legend">
      {#each metric.series as s, i}
        <span><span class="swatch" class:second={i === 1}></span>{s.name}</span>
      {/each}
      <span class="caption">last minute, twice a second</span>
    </div>
  </header>
  <div class="plot">
    <div class="axis">
      {#each ticks as t}
        <span class="caption">{Math.round(t)} {metric.unit}</span>
      {/each}
    </div>
    <svg viewBox="0 0 {W} {H}" preserveAspectRatio="none" aria-hidden="true">
      {#each ticks as _, i}
        <line class="grid" x1="0" x2={W} y1={(H * i) / 4} y2={(H * i) / 4} />
      {/each}
      {#each metric.series as s, i}
        <polyline class:second={i === 1} points={polyline(s.values, range[0], range[1], W, H)} />
      {/each}
    </svg>
  </div>
  <div class="time caption"><span>60 s ago</span><span>now</span></div>
</section>

<style>
  .graph {
    display: flex;
    flex-direction: column;
    gap: var(--space-3);
    padding: var(--space-4) var(--space-5);
    min-height: 320px;
  }
  header {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    gap: var(--space-4);
    flex-wrap: wrap;
  }
  .legend {
    display: flex;
    align-items: center;
    gap: var(--space-4);
  }
  .legend > span {
    display: inline-flex;
    align-items: center;
    gap: var(--space-2);
  }
  .swatch {
    display: inline-block;
    width: 8px;
    height: 8px;
    border-radius: 2px;
    background: var(--accent);
  }
  .swatch.second {
    background: var(--series-2);
  }
  .plot {
    flex: 1;
    display: flex;
    gap: var(--space-3);
    min-height: 220px;
  }
  .axis {
    display: flex;
    flex-direction: column;
    justify-content: space-between;
    text-align: right;
    min-width: 64px;
    margin: -0.6em 0;
  }
  svg {
    flex: 1;
    width: 100%;
    height: auto;
    border-left: 1px solid var(--stroke-control);
    border-bottom: 1px solid var(--stroke-control);
  }
  .grid {
    stroke: var(--stroke);
    stroke-width: 1;
    vector-effect: non-scaling-stroke;
  }
  polyline {
    fill: none;
    stroke: var(--accent);
    stroke-width: 2;
    vector-effect: non-scaling-stroke;
  }
  polyline.second {
    stroke: var(--series-2);
  }
  .time {
    display: flex;
    justify-content: space-between;
    padding-left: calc(64px + var(--space-3));
  }
</style>
