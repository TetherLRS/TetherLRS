<script lang="ts">
  import { metricRange, polylines, type Metric } from '$lib/link-metrics';

  // the measures as Task Manager lists them: name and values on the left, a small graph on the right
  let { metrics, selected, onselect, grid = false }: { metrics: Metric[]; selected?: string; onselect: (key: string) => void; grid?: boolean } = $props();
</script>

<div class="list" class:grid role="tablist" aria-label="Link statistics">
  {#each metrics as m (m.key)}
    {@const r = metricRange(m)}
    <button class="card tile" class:selected={m.key === selected} role="tab" aria-selected={m.key === selected} onclick={() => onselect(m.key)}>
      <span class="text">
        <span class="tile-title">{m.title}</span>
        {#each m.readout as o, i}
          <span class="readout"><span class="swatch" class:second={i === 1} class:none={i >= m.series.length}></span><span class="caption">{o.name}</span><span class="v">{o.text}</span></span>
        {/each}
      </span>
      <svg viewBox="0 0 120 28" preserveAspectRatio="none" aria-hidden="true">
        {#each m.series as s, i}
          {#each polylines(s.values, r[0], r[1], 120, 28) as points}
            <polyline class:second={i === 1} {points} />
          {/each}
        {/each}
      </svg>
    </button>
  {/each}
</div>

<style>
  .list {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: var(--space-2);
  }
  @container content (min-width: 900px) {
    .list:not(.grid) {
      grid-template-columns: minmax(0, 1fr);
      align-content: start;
    }
    .list.grid {
      grid-template-columns: repeat(3, minmax(0, 1fr));
    }
  }
  @container content (min-width: 1600px) {
    .list.grid {
      grid-template-columns: repeat(6, minmax(0, 1fr));
    }
  }
  .tile {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 72px;
    align-items: start;
    gap: var(--space-3);
    padding: var(--space-2) var(--space-3);
    text-align: left;
    font: inherit;
    color: inherit;
    cursor: default;
    border: 1px solid var(--stroke);
  }
  .tile:hover {
    background: var(--hover);
  }
  .tile.selected {
    background: var(--selected);
    border-color: var(--accent);
  }
  .text {
    display: flex;
    flex-direction: column;
    gap: 1px;
    min-width: 0;
  }
  .tile-title {
    font-weight: 600;
  }
  .readout {
    display: flex;
    align-items: baseline;
    gap: var(--space-2);
  }
  .readout .caption {
    min-width: 56px;
  }
  .v {
    font-variant-numeric: tabular-nums;
    white-space: nowrap;
  }
  .swatch {
    flex: none;
    width: 8px;
    height: 8px;
    border-radius: 2px;
    background: var(--accent);
    align-self: center;
  }
  .swatch.second {
    background: var(--series-2);
  }
  .swatch.none {
    background: none; /* a reading without a line in the graph */
  }
  svg {
    width: 72px;
    height: 32px;
    align-self: center;
  }
  polyline {
    fill: none;
    stroke: var(--accent);
    stroke-width: 1.5;
    stroke-linecap: round;
    vector-effect: non-scaling-stroke;
  }
  polyline.second {
    stroke: var(--series-2);
  }
</style>
