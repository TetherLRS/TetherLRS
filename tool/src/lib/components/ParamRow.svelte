<script lang="ts">
  import { app } from '$lib/app.svelte';
  import type { Param } from '$lib/device/cli-parse';
  import { paramMeta } from '$lib/param-meta';

  let { param }: { param: Param } = $props();

  const meta = $derived(paramMeta(param.name));
  const opts = $derived(app.options[param.name]);
  let phrase = $state('');
  let number = $state(0);

  $effect(() => {
    phrase = param.kind === 'phrase' ? param.text : '';
    number = param.kind === 'value' ? parseInt(param.text, 10) || 0 : 0;
  });

  const choices = $derived(opts?.list ?? (param.index !== undefined ? [{ index: param.index, text: param.text }] : []));
  const phraseOk = $derived(/^[a-z0-9#\-._]{6}$/.test(phrase));
  const disabled = $derived(app.state !== 'connected' || app.streaming);

  async function onSelect(e: Event) {
    const v = parseInt((e.target as HTMLSelectElement).value, 10);
    if (v !== param.index && !(await app.setParam(param.name, v))) (e.target as HTMLSelectElement).value = String(param.index);
  }
</script>

<div class="row">
  <div class="label">
    <span>{meta.label}</span>
    {#if meta.help}<span class="caption">{meta.help}</span>{/if}
  </div>
  <div class="control">
    {#if param.unchangeable}
      <span class="fixed">{param.text}</span>
    {:else if param.kind === 'list'}
      <select value={param.index} onchange={onSelect} onfocus={() => app.loadOptions(param.name)} {disabled} aria-label={meta.label}>
        {#each choices as c (c.index)}
          <option value={c.index}>{c.text}</option>
        {/each}
      </select>
    {:else if param.kind === 'phrase'}
      <input type="text" maxlength="6" spellcheck="false" bind:value={phrase} {disabled} aria-label={meta.label} aria-invalid={!phraseOk} />
      <button class="btn" disabled={disabled || !phraseOk || phrase === param.text} onclick={() => app.setParam(param.name, phrase)}>Set</button>
    {:else}
      <input type="number" min={opts?.min ?? -120} max={opts?.max ?? 120} bind:value={number} {disabled} aria-label={meta.label} onfocus={() => app.loadOptions(param.name)} />
      <button class="btn" disabled={disabled || String(number) === param.text.split(' ')[0]} onclick={() => app.setParam(param.name, number)}>Set</button>
    {/if}
  </div>
</div>

<style>
  .row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: var(--space-5);
    min-height: 56px;
    padding: var(--space-2) var(--space-4);
  }
  .row + :global(.row) {
    border-top: 1px solid var(--stroke);
  }
  .label {
    display: flex;
    flex-direction: column;
    min-width: 0;
  }
  .control {
    display: flex;
    gap: var(--space-2);
    flex: none;
  }
  select {
    min-width: 180px;
  }
  input[type='text'] {
    width: 120px;
    font-family: var(--mono);
  }
  input[type='number'] {
    width: 96px;
  }
  input[aria-invalid='true'] {
    border-bottom-color: var(--err);
  }
  .fixed {
    color: var(--text-2);
    padding: 6px 0;
  }
</style>
