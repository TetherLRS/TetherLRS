<script lang="ts">
  import { app, type Page } from '$lib/app.svelte';
  import Icon from './Icon.svelte';

  const items: { page: Page; label: string; icon: string; needsDevice: boolean }[] = [
    { page: 'device', label: 'Device', icon: 'device', needsDevice: false },
    { page: 'params', label: 'Parameters', icon: 'params', needsDevice: true },
    { page: 'link', label: 'Link', icon: 'link', needsDevice: true },
    { page: 'terminal', label: 'Terminal', icon: 'terminal', needsDevice: true },
    { page: 'firmware', label: 'Firmware', icon: 'firmware', needsDevice: false },
  ];

  async function go(page: Page) {
    if (app.streaming && page !== 'link') await app.stopStats();
    app.page = page;
  }
</script>

<nav class="pane">
  <div class="brand">
    <span class="logo" aria-hidden="true"></span>
    <span>TetherLRS</span>
  </div>
  <ul>
    {#each items as item}
      <li>
        <button class="item" class:selected={app.page === item.page} disabled={item.needsDevice && !app.connected} onclick={() => go(item.page)}>
          <Icon name={item.icon} />
          <span>{item.label}</span>
        </button>
      </li>
    {/each}
  </ul>
  <div class="conn">
    <span class="dot" class:on={app.connected} class:pending={app.state === 'connecting'}></span>
    <div class="conn-text">
      {#if app.connected && app.version}
        <span class="conn-name">{app.version.txName}</span>
        <span class="caption">{app.version.rxConnected ? 'Receiver connected' : 'No receiver'}</span>
      {:else if app.state === 'connecting'}
        <span class="conn-name">Connecting…</span>
      {:else}
        <span class="conn-name">Not connected</span>
      {/if}
    </div>
  </div>
</nav>

<style>
  .pane {
    display: flex;
    flex-direction: column;
    width: 240px;
    flex: none;
    padding: var(--space-2) var(--space-1) var(--space-3);
    background: var(--pane);
  }
  .brand {
    display: flex;
    align-items: center;
    gap: var(--space-3);
    height: 48px;
    padding: 0 var(--space-3);
    font-weight: 600;
  }
  .logo {
    width: 18px;
    height: 18px;
    border-radius: 5px;
    background: linear-gradient(135deg, var(--accent), color-mix(in srgb, var(--accent) 55%, #7a3cff));
  }
  ul {
    list-style: none;
    margin: var(--space-1) 0 0;
    padding: 0;
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .item {
    position: relative;
    display: flex;
    align-items: center;
    gap: var(--space-3);
    width: 100%;
    height: 36px;
    padding: 0 var(--space-3);
    border: none;
    border-radius: var(--r-control);
    background: transparent;
    text-align: left;
  }
  .item:hover:not(:disabled) {
    background: var(--hover);
  }
  .item.selected {
    background: var(--selected);
  }
  .item.selected::before {
    content: '';
    position: absolute;
    left: 0;
    top: 10px;
    bottom: 10px;
    width: 3px;
    border-radius: 2px;
    background: var(--accent);
  }
  .item:disabled {
    color: var(--text-3);
  }
  .conn {
    margin-top: auto;
    display: flex;
    align-items: center;
    gap: var(--space-3);
    padding: var(--space-2) var(--space-3);
  }
  .dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--text-3);
    flex: none;
  }
  .dot.on {
    background: var(--ok);
  }
  .dot.pending {
    background: var(--warn);
  }
  .conn-text {
    display: flex;
    flex-direction: column;
    min-width: 0;
  }
  .conn-name {
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
</style>
