<script lang="ts">
  import { tick } from 'svelte';
  import { app } from '$lib/app.svelte';

  let input = $state('');
  let history: string[] = [];
  let back = 0; // how far up the history the arrows went
  let out: HTMLPreElement | undefined = $state();

  $effect(() => {
    void app.terminal.length;
    tick().then(() => out?.scrollTo({ top: out.scrollHeight }));
  });

  async function send() {
    const cmd = input.trim();
    if (!cmd) return;
    history = [...history.filter((h) => h !== cmd), cmd].slice(-50);
    back = 0;
    input = '';
    await app.runCommand(cmd);
  }

  function key(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      e.preventDefault();
      void send();
    } else if (e.key === 'ArrowUp' && back < history.length) {
      e.preventDefault();
      back++;
      input = history[history.length - back];
    } else if (e.key === 'ArrowDown' && back > 0) {
      e.preventDefault();
      back--;
      input = back ? history[history.length - back] : '';
    }
  }
</script>

<section class="card">
  <div class="head">
    <h2>Tx command line</h2>
    <p class="caption">The Tx's own CLI: <span class="mono">pl</span>, <span class="mono">p tx_power = 2</span>, <span class="mono">v</span>, <span class="mono">listfreqs</span>, SiK's <span class="mono">ATI5</span>… Changes count as unsaved until Save.</p>
  </div>
  <pre class="out mono" bind:this={out} aria-live="polite">{app.terminal.join('\n') || 'Type a command below, h for the list.'}</pre>
  <div class="line">
    <input type="text" class="mono" bind:value={input} onkeydown={key} placeholder="command" spellcheck="false" autocomplete="off" disabled={app.state !== 'connected' || app.streaming} aria-label="Command" />
    <button class="btn accent" onclick={send} disabled={!input.trim() || app.state !== 'connected' || app.streaming}>Send</button>
    <button class="btn" onclick={() => (app.terminal = [])} disabled={!app.terminal.length}>Clear</button>
  </div>
</section>

<style>
  .card {
    display: flex;
    flex-direction: column;
    gap: var(--space-3);
    padding: var(--space-4) var(--space-5) var(--space-5);
  }
  .head p {
    margin: var(--space-1) 0 0;
  }
  .head .mono {
    font-size: 12px;
  }
  .out {
    margin: 0;
    height: 420px;
    overflow: auto;
    padding: var(--space-3);
    border-radius: var(--r-control);
    border: 1px solid var(--stroke);
    background: var(--bg);
    white-space: pre-wrap;
  }
  .line {
    display: flex;
    gap: var(--space-2);
  }
  input {
    flex: 1;
    min-width: 0;
  }
</style>
