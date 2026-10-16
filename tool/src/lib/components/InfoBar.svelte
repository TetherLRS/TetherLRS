<script lang="ts">
  import Icon from './Icon.svelte';
  let { kind = 'error', text, onclose }: { kind?: 'error' | 'warning' | 'info' | 'success'; text: string; onclose?: () => void } = $props();
  const icon = $derived({ error: 'warning', warning: 'warning', info: 'info', success: 'check' }[kind]);
</script>

<div class="bar {kind}" role={kind === 'error' ? 'alert' : 'status'}>
  <span class="icon"><Icon name={icon} size={16} /></span>
  <span class="text">{text}</span>
  {#if onclose}
    <button class="close" aria-label="Close" onclick={onclose}><Icon name="close" size={16} /></button>
  {/if}
</div>

<style>
  .bar {
    display: flex;
    align-items: flex-start;
    gap: var(--space-3);
    padding: 10px var(--space-3) 10px var(--space-4);
    border: 1px solid var(--stroke);
    border-radius: var(--r-card);
    background: var(--info-bg);
  }
  .error {
    background: var(--err-bg);
  }
  .error .icon {
    color: var(--err);
  }
  .warning {
    background: var(--warn-bg);
  }
  .warning .icon {
    color: var(--warn);
  }
  .success {
    background: var(--ok-bg);
  }
  .success .icon {
    color: var(--ok);
  }
  .info .icon {
    color: var(--accent);
  }
  .icon {
    display: flex;
    padding-top: 2px;
  }
  .text {
    flex: 1;
  }
  .close {
    display: flex;
    padding: 4px;
    border: none;
    border-radius: var(--r-control);
    background: transparent;
    color: var(--text-2);
  }
  .close:hover {
    background: var(--hover);
  }
</style>
