<script lang="ts">
  import { app } from '$lib/app.svelte';
  import Hint from './Hint.svelte';

  // the receivers of a multipoint ground radio: what the Tx knows of each, the selected one, and binding a slot
  let bindSlot = $state(0);

  const last = $derived(app.stats.findLast((s) => s !== undefined));
  const rows = $derived(
    Array.from({ length: app.rxCount }, (_, slot) => {
      const info = app.rxList.find((r) => r.slot === slot);
      const live = last?.mp?.find((m) => m.slot === slot);
      return {
        slot,
        selected: info?.selected ?? live?.selected ?? slot === 0,
        connected: live ? live.lq !== undefined : (info?.connected ?? false),
        name: info?.name,
        version: info?.version,
        lq: live?.lq ?? info?.lq,
        rssi: live?.rssi ?? info?.rssi,
      };
    }),
  );
  const disabled = $derived(app.state !== 'connected');
</script>

<div class="card rxlist">
  <span class="caption">Receivers<Hint text="One ground radio, up to four aircraft radios, each answering in its own slot of the cycle. The parameters and the stats are of the selected one; Select changes which, Bind binds a receiver to the chosen slot." /></span>
  <table>
    <tbody>
      {#each rows as r (r.slot)}
        <tr class:selected={r.selected}>
          <td class="slot">{r.slot}</td>
          <td class="who">
            {#if r.connected}
              <span class="name">{r.name ?? 'setup pending'}</span>
              {#if r.version}<span class="caption">{r.version}</span>{/if}
            {:else}
              <span class="muted">not connected</span>
            {/if}
          </td>
          <td class="num">{r.lq !== undefined ? `${r.lq} %` : ''}</td>
          <td class="num">{r.rssi !== undefined ? `${r.rssi} dBm` : ''}</td>
          <td class="act">
            {#if r.selected}
              <span class="caption">selected</span>
            {:else}
              <button class="btn" onclick={() => app.selectRx(r.slot)} {disabled}>Select</button>
            {/if}
          </td>
        </tr>
      {/each}
    </tbody>
  </table>
  <div class="bind">
    <label><span class="caption">Bind to slot</span>
      <select bind:value={bindSlot} {disabled} aria-label="Slot to bind">
        {#each rows as r (r.slot)}<option value={r.slot}>{r.slot}</option>{/each}
      </select>
    </label>
    <button class="btn" onclick={() => app.bind(bindSlot)} {disabled}>Bind</button>
  </div>
</div>

<style>
  .rxlist {
    display: flex;
    flex-direction: column;
    gap: var(--space-2);
    padding: var(--space-4);
  }
  table {
    border-collapse: collapse;
    width: 100%;
  }
  td {
    padding: var(--space-1) var(--space-2) var(--space-1) 0;
    vertical-align: middle;
  }
  tr + tr td {
    border-top: 1px solid var(--stroke);
  }
  tr.selected .name {
    font-weight: 600;
  }
  .slot {
    width: 20px;
    color: var(--text-2);
    font-family: var(--mono);
  }
  .who {
    display: flex;
    flex-direction: column;
  }
  .num {
    text-align: right;
    white-space: nowrap;
    font-variant-numeric: tabular-nums;
  }
  .act {
    text-align: right;
    padding-right: 0;
  }
  .muted {
    color: var(--text-2);
  }
  .bind {
    display: flex;
    align-items: end;
    gap: var(--space-2);
  }
  .bind label {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
</style>
