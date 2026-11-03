// The loopback test: with Rx Test = loopback the receiver sends back up what came down, so a known stream paced
// into the Tx's data port shows what the link carries: bytes per second each way, what went missing, the round trip.

import { DeviceLink, sleep } from './link';
import type { TxSession } from './tx';

export interface BenchmarkOptions {
  seconds: number;
  rateBps: number; // what the host feeds the Tx, bytes per second
}

export interface BenchmarkProgress {
  elapsedMs: number;
  sent: number;
  received: number;
  lost: number; // bytes the stream skipped, counted from the gaps in the sequence
  errors: number; // bytes out of sequence
  rttMs?: number; // the first byte, out and back
}

export interface BenchmarkResult extends BenchmarkProgress {
  sentBps: number;
  receivedBps: number;
}

export const RATES = [500, 1000, 2000, 4000, 8000];
export const DURATIONS = [10, 30, 60];

// the stream: 16 bit words counting up, little endian, so a gap and a shift both show
export function words(from: number, count: number): Uint8Array {
  const b = new Uint8Array(count * 2);
  for (let i = 0; i < count; i++) {
    const w = (from + i) & 0xffff;
    b[2 * i] = w & 0xff;
    b[2 * i + 1] = w >> 8;
  }
  return b;
}

// follows the words coming back: ok, a gap (lost), or a byte out of place (error, then one byte on to realign)
export class WordChecker {
  expected = 0;
  lost = 0;
  errors = 0;
  private pending: number[] = [];

  feed(bytes: Uint8Array): void {
    this.pending.push(...bytes);
    while (this.pending.length >= 2) {
      const w = this.pending[0] | (this.pending[1] << 8);
      const ahead = (w - this.expected) & 0xffff;
      if (ahead === 0) {
        this.expected = (this.expected + 1) & 0xffff;
        this.pending.splice(0, 2);
      } else if (ahead < 2048) {
        this.lost += ahead * 2;
        this.expected = (w + 1) & 0xffff;
        this.pending.splice(0, 2);
      } else {
        this.errors++;
        this.pending.shift();
      }
    }
  }
}

// the Tx must be in its CLI when this starts, and is back in it when it returns; stop() asked every 50 ms
export async function runLoopback(
  link: DeviceLink,
  tx: TxSession,
  opts: BenchmarkOptions,
  onProgress: (p: BenchmarkProgress) => void,
  stop: () => boolean = () => false,
): Promise<BenchmarkResult> {
  const checker = new WordChecker();
  let received = 0;
  let rttMs: number | undefined;
  let firstSentAt = 0;
  await tx.exitCommandMode(); // its answer is not part of the count
  link.take();
  const stopListen = link.onBytes((b) => {
    if (rttMs === undefined && firstSentAt) rttMs = performance.now() - firstSentAt;
    received += b.length;
    checker.feed(b);
  });
  const start = performance.now();
  let sent = 0;
  let next = 0;
  let feedEnd = start;
  const progress = (): BenchmarkProgress => ({
    elapsedMs: performance.now() - start, sent, received, lost: checker.lost, errors: checker.errors, rttMs,
  });
  try {
    while (performance.now() - start < opts.seconds * 1000 && !stop()) {
      const due = Math.floor(((performance.now() - start) / 1000) * opts.rateBps) - sent; // what the rate owes by now
      const count = Math.min(Math.max(0, Math.floor(due / 2)), 512);
      if (count) {
        if (!firstSentAt) firstSentAt = performance.now();
        await link.writeBytes(words(next, count));
        next = (next + count) & 0xffff;
        sent += count * 2;
      }
      onProgress(progress());
      await sleep(50);
    }
    // the tail comes back within a second or two; the CLI's escape wants a quiet second anyway
    feedEnd = performance.now();
    while (performance.now() - feedEnd < 2000) {
      await sleep(100);
      onProgress(progress());
    }
  } finally {
    stopListen();
    await tx.enterCommandMode();
  }
  const p = progress();
  const seconds = Math.max(0.001, (feedEnd - start) / 1000); // the feeding time: not the tail, not the way back into the CLI
  return { ...p, sentBps: Math.round(sent / seconds), receivedBps: Math.round(received / seconds) };
}
