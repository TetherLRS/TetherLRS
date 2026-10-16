<script lang="ts">
  import { app, type Page } from '$lib/app.svelte';
  import Icon from './Icon.svelte';
  import logo from '$lib/assets/logo.png';

  const items: { page: Page; label: string; icon: string; needsDevice: boolean }[] = [
    { page: 'device', label: 'Device', icon: 'device', needsDevice: false },
    { page: 'params', label: 'Parameters', icon: 'params', needsDevice: true },
    { page: 'link', label: 'Link', icon: 'link', needsDevice: true },
    { page: 'terminal', label: 'Terminal', icon: 'terminal', needsDevice: true },
    { page: 'firmware', label: 'Firmware', icon: 'firmware', needsDevice: false },
  ];

  // the page changes first: stopping the stats while the Device page still showed made it start them again, and the
  // Parameters page was left with every list disabled; the page that wants the stats starts them again
  async function go(page: Page) {
    if (page === app.page) return;
    app.page = page;
    await app.stopStats();
  }
</script>

<nav class="pane">
  <div class="brand">
    <img class="logo" src={logo} alt="" />
    <span class="label">TetherLRS</span>
  </div>
  <ul>
    {#each items as item}
      <li>
        <button class="item" class:selected={app.page === item.page} disabled={item.needsDevice && !app.connected} onclick={() => go(item.page)} title={item.label}>
          <Icon name={item.icon} />
          <span class="label">{item.label}</span>
        </button>
      </li>
    {/each}
  </ul>
  <div class="conn" title={app.connected && app.version ? app.version.txName : 'Not connected'}>
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
    transition: width 0.15s;
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
    width: 20px;
    height: 20px;
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
  /* a narrow window: icons only, as the Windows 11 navigation pane does in compact mode */
  @media (max-width: 1000px) {
    .pane {
      width: 56px;
    }
    .label,
    .conn-text {
      display: none;
    }
    .brand,
    .item,
    .conn {
      justify-content: center;
      padding: 0;
    }
  }
</style>
