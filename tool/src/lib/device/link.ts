// Text conversation with a TetherLRS Tx over a Web Serial style port (browser, Tauri bridge or demo).

const encoder = new TextEncoder();

export class DeviceLink {
  private reader?: ReadableStreamDefaultReader<Uint8Array>;
  private decoder = new TextDecoder();
  private buf = '';
  private lastRxMs = 0;
  private reading?: Promise<void>;
  private listeners = new Set<(text: string) => void>();
  isOpen = false;

  constructor(readonly port: SerialPort) {}

  async open(baudRate: number): Promise<void> {
    await this.port.open({ baudRate });
    this.isOpen = true;
    this.buf = '';
    this.reading = this.readLoop();
  }

  // gives the port back closed, so esptool-js can open it at its own baud rate
  async close(): Promise<void> {
    if (!this.isOpen) return;
    this.isOpen = false;
    try {
      await this.reader?.cancel();
    } catch {
      // already gone, e.g. unplugged
    }
    await this.reading;
    try {
      await this.port.close();
    } catch {
      // as above
    }
  }

  private async readLoop(): Promise<void> {
    const readable = this.port.readable;
    if (!readable) return;
    this.reader = readable.getReader();
    try {
      for (;;) {
        const { value, done } = await this.reader.read();
        if (done) break;
        if (!value?.length) continue;
        const text = this.decoder.decode(value, { stream: true });
        this.buf += text;
        this.lastRxMs = performance.now();
        for (const l of this.listeners) l(text);
      }
    } catch {
      // the port went away; isOpen tells the caller
    } finally {
      this.reader.releaseLock();
      this.reader = undefined;
      this.isOpen = false;
    }
  }

  onText(listener: (text: string) => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  async write(text: string): Promise<void> {
    const writer = this.port.writable?.getWriter();
    if (!writer) throw new Error('port not writable');
    try {
      await writer.write(encoder.encode(text));
    } finally {
      writer.releaseLock();
    }
  }

  take(): string {
    const b = this.buf;
    this.buf = '';
    return b;
  }

  // waits for the text to contain pattern; returns everything received so far, or undefined on timeout
  async waitFor(pattern: RegExp, timeoutMs: number): Promise<string | undefined> {
    const end = performance.now() + timeoutMs;
    while (performance.now() < end) {
      if (pattern.test(this.buf)) return this.take();
      await sleep(10);
    }
    return undefined;
  }

  // waits until nothing arrived for quietMs, at most maxMs; returns what came
  async waitQuiet(quietMs: number, maxMs: number): Promise<string> {
    const start = performance.now();
    const end = start + maxMs;
    for (;;) {
      const now = performance.now();
      const last = Math.max(this.lastRxMs, start);
      if (now - last >= quietMs || now >= end) return this.take();
      await sleep(10);
    }
  }
}

export const sleep = (ms: number) => new Promise<void>((r) => setTimeout(r, ms));
