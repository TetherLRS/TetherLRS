<script lang="ts">
  import { app } from '$lib/app.svelte';
  import NavPane from '$lib/components/NavPane.svelte';
  import InfoBar from '$lib/components/InfoBar.svelte';
  import DevicePage from '$lib/components/DevicePage.svelte';
  import ParamsPage from '$lib/components/ParamsPage.svelte';
  import LinkPage from '$lib/components/LinkPage.svelte';
  import FirmwarePage from '$lib/components/FirmwarePage.svelte';

  const titles = { device: 'Device', params: 'Parameters', link: 'Link', firmware: 'Firmware' };
</script>

<div class="shell">
  <NavPane />
  <main>
    <div class="content">
      <h1>{titles[app.page]}</h1>
      {#if app.error}
        <InfoBar kind="error" text={app.error} onclose={() => (app.error = '')} />
      {/if}
      {#if app.page === 'device'}
        <DevicePage />
      {:else if app.page === 'params'}
        <ParamsPage />
      {:else if app.page === 'link'}
        <LinkPage />
      {:else}
        <FirmwarePage />
      {/if}
    </div>
  </main>
</div>

<style>
  .shell {
    display: flex;
    height: 100vh;
  }
  main {
    flex: 1;
    min-width: 0;
    overflow-y: auto;
    background: var(--bg);
    border-top-left-radius: var(--r-card);
    border-left: 1px solid var(--stroke);
    border-top: 1px solid var(--stroke);
    margin-top: var(--space-1);
  }
  .content {
    display: flex;
    flex-direction: column;
    gap: var(--space-3);
    max-width: 1000px;
    padding: var(--space-5) var(--space-6) 0;
  }
  h1 {
    margin-bottom: var(--space-2);
  }
</style>
