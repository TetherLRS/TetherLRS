<script lang="ts">
  import { onDestroy, onMount } from 'svelte';
  import { app, statsPage } from '$lib/app.svelte';
  import type { LinkStats } from '$lib/device/cli-parse';
  import { linkMetrics } from '$lib/link-metrics';
  import LinkGraph from './LinkGraph.svelte';
  import LinkTiles from './LinkTiles.svelte';

  // live from the moment the page opens; leaving the page gives the serial port back
  onMount(() => {
    void app.startStats();
  });
  onDestroy(() => {
    if (!statsPage(app.page)) void app.stopStats(); // only when leaving for a page without them, not on a rebuild
  });

  const last = $derived(app.stats.findLast((s) => s !== undefined));
  const eu = $derived(app.param('Region')?.text === 'EU');
  const metrics = $derived(linkMetrics(app.stats));
  const current = $derived(metrics.find((m) => m.key === app.linkMetric) ?? metrics[0]);

  const held = (s: LinkStats | undefined) => (s?.ccaBusy ?? 0) + (s?.limited ?? 0);
</script>

<div class="link">
  <LinkTiles {metrics} selected={current.key} onselect={(k) => (app.linkMetric = k)} />

  <LinkGraph metric={current} />
</div>

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
  /* as Task Manager's Performance page: the measures on the left, the chosen one large on the right */
  .link {
    display: grid;
    grid-template-columns: minmax(0, 1fr);
    gap: var(--space-3);
  }
  @container content (min-width: 900px) {
    .link {
      grid-template-columns: minmax(260px, 320px) minmax(0, 1fr);
      min-height: calc(100vh - 150px);
    }
  }
  .eu {
    padding: var(--space-4) var(--space-5);
  }
  .eu p {
    margin: var(--space-1) 0 0;
  }
</style>
