// A Web Serial style port over the desktop app's Rust bridge (src-tauri/src/serial.rs), for the DeviceLink and
// for esptool-js alike.

import { Channel, invoke } from '@tauri-apps/api/core';

export interface NativePortInfo {
  path: string;
  vid?: number;
  pid?: number;
  product?: string;
  manufacturer?: string;
  serial?: string;
}

export class TauriSerialPort {
  readable: ReadableStream<Uint8Array> | null = null;
  writable: WritableStream<Uint8Array> | null = null;
  private id?: number;

  constructor(readonly info: NativePortInfo) {}

  getInfo(): SerialPortInfo {
    return { usbVendorId: this.info.vid ?? undefined, usbProductId: this.info.pid ?? undefined };
  }

  async open(options: SerialOptions): Promise<void> {
    let controller!: ReadableStreamDefaultController<Uint8Array>;
    this.readable = new ReadableStream<Uint8Array>({
      start: (c) => {
        controller = c;
      },
      cancel: () => this.closeNative(),
    });
    const onData = new Channel<ArrayBuffer>();
    onData.onmessage = (data) => {
      try {
        if (data.byteLength === 0) controller.close(); // the bridge's word that the port is gone (unplugged)
        else controller.enqueue(new Uint8Array(data));
      } catch {
        // reader gone
      }
    };
    this.id = await invoke<number>('serial_open', { path: this.info.path, baud: options.baudRate, onData });
    const id = this.id;
    this.writable = new WritableStream<Uint8Array>({
      write: (chunk) => invoke('serial_write', chunk, { headers: { 'x-port-id': String(id) } }),
    });
  }

  private async closeNative(): Promise<void> {
    if (this.id === undefined) return;
    const id = this.id;
    this.id = undefined;
    await invoke('serial_close', { id });
  }

  async close(): Promise<void> {
    await this.closeNative();
    this.readable = null;
    this.writable = null;
  }

  async setSignals(signals: SerialOutputSignals): Promise<void> {
    if (this.id === undefined) return;
    await invoke('serial_set_signals', { id: this.id, dtr: signals.dataTerminalReady, rts: signals.requestToSend });
  }
}

export async function listNativePorts(): Promise<NativePortInfo[]> {
  return invoke<NativePortInfo[]>('serial_list');
}
