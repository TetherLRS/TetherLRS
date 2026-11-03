<script lang="ts">
  import { app } from '$lib/app.svelte';
  import { randomKey, validKey } from '$lib/firmware/bindphrase';
  import { mwText } from '$lib/link-metrics';

  // the two settings that are not parameters: the link key, which never goes over the air, and the custom plan
  let key = $state('');
  let centre = $state(869525);
  let span = $state(250);
  let lbt = $state(true);
  let dbm = $state(27);
  // the usual power levels, and whatever the radio holds if it is none of them
  const LEVELS = [10, 14, 17, 20, 24, 27, 30, 33];
  const levels = $derived([...new Set([...LEVELS, dbm])].sort((a, b) => a - b));

  $effect(() => {
    const cp = app.customPlan;
    if (cp) {
      centre = cp.centreKHz;
      span = cp.spanKHz;
      lbt = cp.lbt;
      dbm = cp.maxDbm;
    }
  });

  const disabled = $derived(app.state !== 'connected' || app.streaming);
  const keyOk = $derived(validKey(key));
  const planChanged = $derived(
    !app.customPlan || centre !== app.customPlan.centreKHz || span !== app.customPlan.spanKHz || lbt !== app.customPlan.lbt || dbm !== app.customPlan.maxDbm,
  );
  const customRegion = $derived(app.param('Region')?.text === 'custom');
</script>

<div class="row">
  <div class="label">
    <span>Link key</span>
    <span class="caption">
      32 bytes both radios hold and never send: with it the encryption does not rest on the bind phrase alone. This radio: {app.linkKeySet === undefined ? '–' : app.linkKeySet ? 'set' : 'not set'}.
      The other radio takes the same key when flashed from the Firmware page, or from its own settings. Save keeps it.
    </span>
  </div>
  <div class="control">
    <input class="mono" type="text" maxlength="64" spellcheck="false" autocomplete="off" bind:value={key} placeholder="64 hex digits" aria-label="Link key" aria-invalid={!!key && !keyOk} {disabled} />
    <button class="btn" onclick={() => (key = randomKey())} {disabled}>New</button>
    <button class="btn" onclick={() => app.setLinkKey(key)} disabled={disabled || !keyOk}>Set</button>
    <button class="btn" onclick={() => app.setLinkKey('none')} disabled={disabled || app.linkKeySet === false}>Clear</button>
  </div>
</div>

{#if !app.version?.onlyRx}
  <div class="row plan">
    <div class="label">
      <span>Custom plan</span>
      <span class="caption">
        What Region = custom runs: the channels are the air mode's bandwidth across the span around the centre; listen before talk and a power cap as the local rules ask.
        {#if app.customPlan?.channels !== undefined}Now {app.customPlan.channels} channel{app.customPlan.channels === 1 ? '' : 's'} of {app.customPlan.bwKHz} kHz.{/if}
        {#if !customRegion}Region is not custom: the plan waits.{/if}
      </span>
    </div>
    <div class="control fields">
      <label><span class="caption">Centre kHz</span><input type="number" min="100000" max="2500000" bind:value={centre} {disabled} /></label>
      <label><span class="caption">Span kHz</span><input type="number" min="25" max="100000" bind:value={span} {disabled} /></label>
      <label><span class="caption">Power cap</span>
        <select bind:value={dbm} {disabled} aria-label="Power cap">
          {#each levels as l (l)}<option value={l}>{mwText(l)} · {l} dBm</option>{/each}
        </select>
      </label>
      <label class="check"><input type="checkbox" bind:checked={lbt} {disabled} /><span class="caption">Listen before talk</span></label>
      <button class="btn" onclick={() => app.setCustomPlan({ centreKHz: centre, spanKHz: span, lbt, maxDbm: dbm })} disabled={disabled || !planChanged}>Set</button>
    </div>
  </div>
{/if}

<style>
  /* these two carry more controls than a parameter row: the text above, the controls in a row below */
  .row {
    display: flex;
    flex-direction: column;
    gap: var(--space-2);
    padding: var(--space-3) var(--space-4);
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
    flex-wrap: wrap;
    align-items: center;
  }
  .fields label {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .fields label.check {
    flex-direction: row;
    align-items: center;
    gap: var(--space-2);
    align-self: end;
    padding-bottom: 6px;
  }
  input[type='text'] {
    flex: 1;
    min-width: 220px;
    font-family: var(--mono);
  }
  input[type='number'] {
    width: 104px;
  }
  input[aria-invalid='true'] {
    border-bottom-color: var(--err);
  }
</style>
