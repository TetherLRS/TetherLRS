// One esptool-js session per trip to the ROM bootloader: identifying, backing up and flashing all go through it, so
// no second handshake ever meets a stub that is already running (on a plain adapter only a power cycle undoes that).

import { ESPLoader, Transport, type IEspLoaderTerminal } from 'esptool-js';
import { whatRuns, type Running } from './detect';
import { md5 } from './md5';

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

const le32 = (n: number) => new Uint8Array([n & 0xff, (n >> 8) & 0xff, (n >> 16) & 0xff, (n >>> 24) & 0xff]);
const bytesOf = (s: string) => Uint8Array.from(s, (c) => c.charCodeAt(0));
const PIECE = 0x4000; // a bad piece costs this much again

// Plain adapters and their wires lose and flip bytes (an FT232R with a DAKEFPV did, several times a megabyte): every
// piece read is checked against the MD5 the stub computes on the chip, a bad one read again after the stub is let
// finish and given the ack it waits for; a write is checked by MD5 and done again if it does not match.
export class RomSession {
  retries = 0; // pieces read again and writes done again, for the log

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
    (loader as unknown as { FLASH_READ_TIMEOUT: number }).FLASH_READ_TIMEOUT = 3000; // 100 s a packet by default
    // main() passes the baud rate as the number of attempts, so a chip that never answers kept it trying for hours
    loader.detectChip = async (mode) => {
      await loader.connect(mode, nativeUsb ? 7 : 4);
      loader.info(`Detecting chip type... ${loader.chip.CHIP_NAME}`);
    };
    try {
      const chip = await loader.main(nativeUsb ? 'default_reset' : 'no_reset');
      // a stub left running by an earlier session: esptool-js skips its upload but never says it talks to a stub, so
      // its soft reset sent the ROM's sequence and the ESP8266 never left the stub (Restart did nothing)
      if ((loader as unknown as { syncStubDetected: boolean }).syncStubDetected) loader.IS_STUB = true;
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
    return whatRuns((at, size) => this.readChecked(at, size), this.chipName, cb.progress);
  }

  backup(cb: Progress): Promise<Uint8Array> {
    return this.readChecked(0, this.flashSize, (done) => cb.progress(done, this.flashSize));
  }

  async write(image: Uint8Array, address: number, cb: Progress): Promise<void> {
    for (let attempt = 0; ; attempt++) {
      try {
        await this.loader.writeFlash({
          fileArray: [{ data: binaryString(image), address }],
          flashSize: 'keep',
          flashMode: 'keep',
          flashFreq: 'keep',
          eraseAll: false,
          compress: true,
          reportProgress: (_i, written, total) => cb.progress(written, total),
          calculateMD5Hash: (s: string) => md5(bytesOf(s)),
        });
        return;
      } catch (e) {
        if (attempt >= 2) throw e;
        this.retries++;
        cb.log(`it did not arrive clean (${e instanceof Error ? e.message : String(e)}), writing it again`);
        await this.recover();
      }
    }
  }

  private async readChecked(at: number, size: number, done?: (n: number) => void): Promise<Uint8Array> {
    const out = new Uint8Array(size);
    for (let o = 0; o < size; o += PIECE) {
      const len = Math.min(PIECE, size - o);
      for (let attempt = 0; ; attempt++) {
        try {
          const piece = await this.loader.readFlash(at + o, len);
          await this.drain(60, 1000); // the stub's own MD5 of what it sent, which esptool-js leaves unread
          if (piece.length < len || md5(piece.subarray(0, len)) !== (await this.loader.flashMd5sum(at + o, len))) {
            throw new Error('changed on the way');
          }
          out.set(piece.subarray(0, len), o);
          break;
        } catch (e) {
          if (attempt >= 7) throw new Error(`0x${(at + o).toString(16)} never arrived clean: ${e instanceof Error ? e.message : String(e)}`);
          this.retries++;
          await this.recover(len);
        }
      }
      done?.(o + len);
    }
    return out;
  }

  // until nothing more arrives for quietMs
  private async drain(quietMs: number, limitMs = 10000): Promise<void> {
    const t = this.transport as unknown as { readLoop: (ms: number) => AsyncGenerator<Uint8Array>; buffer: Uint8Array };
    const end = performance.now() + limitMs;
    while (performance.now() < end && !(await t.readLoop(quietMs).next()).done);
    t.buffer = new Uint8Array(0);
  }

  // back in step after a bad packet: the rest of an interrupted read let through and acked (the stub waits for it and
  // hears nothing else until then), then SYNC until it answers
  private async recover(readLen = 0): Promise<void> {
    await this.drain(500, readLen / 11 + 3000);
    if (readLen) await this.transport.write(le32(readLen));
    await this.drain(500);
    for (let i = 0; i < 6; i++) {
      try {
        await this.loader.sync();
        return;
      } catch {
        await this.drain(300);
      }
    }
    throw new Error('it stopped answering');
  }

  // a C3 or S3 on a plain adapter restarts by its RTC watchdog, as esptool's watchdog_reset; the forced download boot
  // TetherLRS's systemboot set is cleared first, or it would come back to the ROM (registers from ESP-IDF's
  // rtc_cntl_reg.h: OPTION1 0x600080F4 on the C3, 0x6000812C on the S3)
  private async watchdogReset(): Promise<void> {
    const s3 = this.chipName === 'ESP32-S3';
    const rtc = 0x60008000;
    await this.loader.writeReg(s3 ? 0x6000812c : 0x600080f4, 0, 0x1);
    await this.loader.writeReg(rtc + (s3 ? 0xb0 : 0xa8), 0x50d83aa1); // unlock
    await this.loader.writeReg(rtc + (s3 ? 0x9c : 0x94), 2000); // timeout
    await this.loader.writeReg(rtc + (s3 ? 0x98 : 0x90), (1 << 31) | (5 << 28) | (1 << 8) | 2).catch(() => {}); // it may reset before answering
  }

  // run: start the firmware. A native USB port resets the chip; an ESP8266 jumps to its application; a C3 or S3 on a
  // plain adapter by its watchdog; anything else needs a power cycle, the caller says so.
  async close(run: boolean): Promise<boolean> {
    let started = false;
    try {
      if (run && this.nativeUsb) {
        await this.loader.after('hard_reset');
        started = true;
      } else if (run && this.chipName === 'ESP8266') {
        await this.loader.after('soft_reset');
        started = true;
      } else if (run && (this.chipName === 'ESP32-C3' || this.chipName === 'ESP32-S3')) {
        await this.watchdogReset();
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
