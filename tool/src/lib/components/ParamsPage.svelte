<script lang="ts">
  import { app } from '$lib/app.svelte';
  import { SECTIONS, paramSection } from '$lib/param-meta';
  import ParamRow from './ParamRow.svelte';
  import InfoBar from './InfoBar.svelte';
  import Icon from './Icon.svelte';

  const visible = $derived(app.params.filter((p) => !p.unavailable));
  const bySection = $derived(Object.fromEntries(SECTIONS.map((s) => [s.id, visible.filter((p) => paramSection(p.name) === s.id)])));

  // options in the background, so the lists are complete when opened; RC ones only when asked
  $effect(() => {
    const names = visible.filter((p) => p.kind === 'list' && !p.unchangeable && paramSection(p.name) !== 'rc').map((p) => p.name);
    void (async () => {
      for (const n of names) {
        if (!app.connected || app.streaming) return;
        if (!app.options[n]) await app.loadOptions(n);
      }
    })();
  });
</script>

{#if app.unsaved}
  <InfoBar kind="warning" text="Changes are active but not stored. Saving restarts the radios if a link setting changed." />
{/if}

{#each SECTIONS as s}
  {#if bySection[s.id]?.length}
    {#if s.id === 'rc'}
      <details class="card section">
        <summary>
          <h2>{s.title}</h2>
          <span class="caption">{s.help}</span>
        </summary>
        {#each bySection[s.id] as p (p.name)}
          <ParamRow param={p} />
        {/each}
      </details>
    {:else}
      <section class="card section">
        <div class="head">
          <h2>{s.title}</h2>
          {#if 'help' in s}<span class="caption">{s.help}</span>{/if}
        </div>
        {#each bySection[s.id] as p (p.name)}
          <ParamRow param={p} />
        {/each}
      </section>
    {/if}
  {/if}
{/each}

{#if app.version && !app.version.rxConnected}
  <p class="caption">Receiver parameters appear when the receiver is connected.</p>
{/if}

<div class="savebar">
  <button class="btn" onclick={() => app.refresh()} disabled={app.state !== 'connected'}><Icon name="refresh" size={16} />Reload</button>
  <button class="btn accent" onclick={() => app.save()} disabled={!app.unsaved || app.state !== 'connected'}>Save to radios</button>
</div>

<style>
  .section {
    padding: var(--space-1) 0;
  }
  .head,
  summary {
    display: flex;
    flex-direction: column;
    gap: 2px;
    padding: var(--space-3) var(--space-4) var(--space-2);
  }
  summary {
    cursor: default;
    list-style: none;
  }
  summary::-webkit-details-marker {
    display: none;
  }
  summary h2::after {
    content: '  ›';
    color: var(--text-2);
  }
  details[open] summary h2::after {
    content: '  ⌄';
  }
  .savebar {
    position: sticky;
    bottom: 0;
    display: flex;
    justify-content: flex-end;
    gap: var(--space-2);
    padding: var(--space-3) 0;
    background: linear-gradient(to bottom, transparent, var(--bg) 30%);
  }
</style>
