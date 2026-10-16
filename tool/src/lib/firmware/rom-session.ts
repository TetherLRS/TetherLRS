// One esptool-js session per trip to the ROM bootloader: identifying, backing up and flashing all go through it, so
// no second handshake ever meets a stub that is already running (on a plain adapter only a power cycle undoes that).

import { ESPLoader, Transport, type IEspLoaderTerminal } from 'esptool-js';
import { whatRuns, type Running } from './detect';

export interface Progress {
  log: (line: string) => void;
  progress: (done: number, total: number) => void;
}

function terminal(log: (line: string) => void): IEspLoaderTerminal {
  let line = '';
  return {
    clean: () => {},
    write: (data: string) => {
      line += data;
    },
    writeLine: (data: string) => {
      log(line + data);
      line = '';
    },
  };
}

const sleep = (ms: number) => new Promise<undefined>((r) => setTimeout(() => r(undefined), ms));

// esptool-js gives up on a read at its timeout but leaves it pending, so the next bytes went to that orphan and the
// ROM's replies arrived without their start: one pending read, kept for the next call
function keepPendingRead(transport: Transport): void {
  const t = transport as unknown as {
    reader?: ReadableStreamDefaultReader<Uint8Array>;
    buffer: Uint8Array;
    readLoop: (timeout: number) => AsyncGenerator<Uint8Array>;
    flushInput: () => Promise<void>;
  };
  let pending: { reader: ReadableStreamDefaultReader<Uint8Array>; read: Promise<ReadableStreamReadResult<Uint8Array>> } | undefined;
  t.readLoop = async function* (timeout: number) {
    const reader = t.reader;
    if (!reader) return;
    try {
      while (true) {
        if (pending?.reader !== reader) pending = { reader, read: reader.read() };
        const r = await Promise.race([pending.read, sleep(timeout)]);
        if (!r) return;
        pending = undefined;
        if (r.done || !r.value) return;
        yield r.value;
      }
    } catch {
      // the port went away
    } finally {
      t.buffer = new Uint8Array(0);
    }
  };
  // the original waits on reader.closed, which never settles while the port is open
  t.flushInput = async () => {
    t.buffer = new Uint8Array(0);
  };
}

function binaryString(data: Uint8Array): string {
  let s = '';
  for (let i = 0; i < data.length; i += 0x8000) s += String.fromCharCode(...data.subarray(i, i + 0x8000));
  return s;
}

// esptool-js's own disconnect waits for the reader's lock, which nothing releases after its cancel: it never returned
async function disconnect(transport: Transport): Promise<void> {
  const t = transport as unknown as { reader?: ReadableStreamDefaultReader<Uint8Array> };
  const reader = t.reader;
  t.reader = undefined;
  if (reader) {
    await reader.cancel().catch(() => {});
    try {
      reader.releaseLock();
    } catch {
      // already released
    }
  }
  await transport.device.close().catch(() => {});
}

export class RomSession {
  private constructor(
    private transport: Transport,
    private loader: ESPLoader,
    readonly chip: string, // as esptool describes it, e.g. ESP8285H16
    readonly chipName: string, // its family: ESP8266, ESP32-C3, ESP32-S3
    readonly flashId: number,
    readonly flashSize: number,
    readonly nativeUsb: boolean,
  ) {}

  // a native USB port: esptool-js resets the chip into the ROM itself; otherwise it has to be there already
  static async open(port: SerialPort, nativeUsb: boolean, cb: Progress, romBaud = 115200): Promise<RomSession> {
    const transport = new Transport(port, false);
    // adapters stay at the ROM's rate all along: some cannot do more, and a stub left at another rate is lost
    const loader = new ESPLoader({ transport, baudrate: nativeUsb ? 921600 : romBaud, romBaudrate: romBaud, terminal: terminal(cb.log) });
    keepPendingRead(transport);
    // main() passes the baud rate as the number of attempts, so a chip that never answers kept it trying for hours
    loader.detectChip = async (mode) => {
      await loader.connect(mode, nativeUsb ? 7 : 4);
      loader.info(`Detecting chip type... ${loader.chip.CHIP_NAME}`);
    };
    try {
      const chip = await loader.main(nativeUsb ? 'default_reset' : 'no_reset');
      const flashId = (await loader.readFlashId()) >>> 0;
      const capacity = (flashId >> 16) & 0xff; // JEDEC: log2 of the size in bytes
      const flashSize = capacity >= 0x12 && capacity <= 0x18 ? 1 << capacity : 0x100000;
      return new RomSession(transport, loader, chip, loader.chip.CHIP_NAME, flashId, flashSize, nativeUsb);
    } catch (e) {
      await disconnect(transport);
      throw e;
    }
  }

  identify(cb: Progress): Promise<Running | undefined> {
    return whatRuns((at, size) => this.loader.readFlash(at, size), this.chipName, cb.progress);
  }

  async backup(cb: Progress): Promise<Uint8Array> {
    let got = 0;
    return this.loader.readFlash(0, this.flashSize, (packet) => {
      got += packet.length;
      cb.progress(got, this.flashSize);
    });
  }

  async write(image: Uint8Array, address: number, cb: Progress): Promise<void> {
    await this.loader.writeFlash({
      fileArray: [{ data: binaryString(image), address }],
      flashSize: 'keep',
      flashMode: 'keep',
      flashFreq: 'keep',
      eraseAll: false,
      compress: true,
      reportProgress: (_i, written, total) => cb.progress(written, total),
    });
  }

  // run: start the firmware. A native USB port resets the chip; an ESP8266 jumps to its application; anything else on
  // a plain adapter needs a power cycle, the caller says so.
  async close(run: boolean): Promise<boolean> {
    let started = false;
    try {
      if (run && this.nativeUsb) {
        await this.loader.after('hard_reset');
        started = true;
      } else if (run && this.chipName === 'ESP8266') {
        await this.loader.after('soft_reset');
        started = true;
      }
    } catch {
      started = false;
    } finally {
      await disconnect(this.transport);
    }
    return started;
  }
}
