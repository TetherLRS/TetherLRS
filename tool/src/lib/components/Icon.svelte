<script lang="ts">
  // One outline icon set, 20 px grid, 1.5 px strokes, after Fluent System Icons
  let { name, size = 20 }: { name: string; size?: number } = $props();

  const paths: Record<string, string> = {
    device: 'M10 9.5v8 M7.2 7.2a4 4 0 0 0 0 5.6 M12.8 7.2a4 4 0 0 1 0 5.6 M4.6 4.6a7.6 7.6 0 0 0 0 10.8 M15.4 4.6a7.6 7.6 0 0 1 0 10.8',
    params: 'M3.5 6h9 M15.5 6h1 M3.5 14h2 M8.5 14h8',
    link: 'M2.5 10.5h3.5l2-5.5 4 10 2-4.5h3.5',
    firmware: 'M10 3v9.5 M6.5 9l3.5 3.5 3.5-3.5 M4 16h12',
    refresh: 'M15.5 10a5.5 5.5 0 1 1-1.6-3.9 M14.2 3.5v3h-3',
    info: 'M10 9v5 M10 6.5v.5',
    warning: 'M10 7.5v4 M10 14v.5',
    close: 'M5.5 5.5l9 9 M14.5 5.5l-9 9',
    check: 'M4.5 10.5l3.5 3.5 7.5-8',
    usb: 'M10 3v11 M7.5 6.5h5 M6 9.5v2h8v-2 M10 14a1.5 1.5 0 1 0 0.01 0',
  };
  const circles: Record<string, [number, number, number][]> = {
    params: [[14, 6, 1.5], [7, 14, 1.5]],
    device: [[10, 10, 1.2]],
    info: [[10, 10, 7]],
  };
</script>

<svg width={size} height={size} viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
  {#if name === 'warning'}
    <path d="M10 3.2l7.2 12.6H2.8z" />
  {/if}
  <path d={paths[name] ?? ''} />
  {#each circles[name] ?? [] as [cx, cy, r]}
    <circle {cx} {cy} {r} />
  {/each}
</svg>
