<script lang="ts">
  import type { PortEntry } from '$lib/serial/ports';

  let {
    ports,
    selected = $bindable(''),
    disabled = false,
    checking,
    group = 'port',
    hideDemo = false,
  }: { ports: PortEntry[]; selected?: string; disabled?: boolean; checking?: Set<string>; group?: string; hideDemo?: boolean } = $props();

  const shown = $derived(hideDemo ? ports.filter((p) => p.kind !== 'demo') : ports);
</script>

<div class="ports" role="radiogroup" aria-label="Serial port">
  {#each shown as p (p.key)}
    <label class="port" class:checked={selected === p.key}>
      <input type="radio" name={group} value={p.key} bind:group={selected} {disabled} />
      <span class="port-text">
        <span class="port-title">
          <span>{p.label}</span>
          {#if p.kind === 'tlrs'}<span class="badge">TetherLRS</span>{/if}
          {#if p.kind === 'tlrs' || p.name}<span class="kind">{p.kind === 'tlrs' ? p.known?.txName : p.name}</span>{/if}
        </span>
        <span class="caption">
          {checking?.has(p.key) ? 'Checking for a TetherLRS Tx… · ' : p.kind === 'tlrs' ? `${p.known?.baud} baud · ` : ''}{p.detail}
        </span>
      </span>
    </label>
  {:else}
    <p class="caption">No ports found.</p>
  {/each}
</div>

<style>
  .ports {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .port {
    display: flex;
    align-items: center;
    gap: var(--space-3);
    padding: var(--space-2) var(--space-3);
    border-radius: var(--r-control);
  }
  .port:hover {
    background: var(--hover);
  }
  .port.checked {
    background: var(--selected);
  }
  .port input {
    accent-color: var(--accent);
    margin: 0;
  }
  .port-text {
    display: flex;
    flex-direction: column;
    min-width: 0;
  }
  .port-title {
    display: flex;
    align-items: center;
    gap: var(--space-2);
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
</style>
