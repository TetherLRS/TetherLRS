<script lang="ts">
  // a small "?" beside a label: the explanation shows on hover or keyboard focus, the row stays clean
  let { text }: { text: string } = $props();
</script>

<button class="hint" type="button" aria-label={text}>
  <span class="dot" aria-hidden="true">?</span>
  <span class="tip" role="tooltip">{text}</span>
</button>

<style>
  .hint {
    position: relative;
    display: inline-flex;
    align-items: center;
    margin-left: var(--space-2);
    padding: 0;
    border: none;
    background: none;
    vertical-align: middle;
    outline: none;
  }
  .dot {
    width: 16px;
    height: 16px;
    border-radius: 50%;
    border: 1px solid var(--stroke-control-bottom);
    color: var(--text-3);
    font-size: 11px;
    line-height: 14px;
    font-weight: 600;
    text-align: center;
    cursor: help;
    user-select: none;
  }
  .hint:hover .dot,
  .hint:focus-visible .dot {
    color: var(--accent);
    border-color: var(--accent);
  }
  .tip {
    display: none;
    position: absolute;
    top: 22px;
    left: -8px;
    z-index: 10;
    width: max-content;
    max-width: min(320px, 70vw);
    padding: var(--space-2) var(--space-3);
    background: var(--surface-strong);
    border: 1px solid var(--stroke);
    border-radius: var(--r-card);
    box-shadow: var(--shadow-flyout);
    font-size: 12px;
    line-height: 16px;
    font-weight: 400;
    color: var(--text);
    text-align: left;
    white-space: normal;
  }
  .hint:hover .tip,
  .hint:focus-visible .tip {
    display: block;
  }
</style>
