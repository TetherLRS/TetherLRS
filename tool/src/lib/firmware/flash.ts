// Flashing and backup through the ESP ROM bootloader with esptool-js, over any Web Serial style port.

import { ESPLoader, Transport, type IEspLoaderTerminal } from 'esptool-js';

export interface FlashCallbacks {
  log: (line: string) => void;
  progress: (written: number, total: number) => void;
}

// the device must already be in the ROM bootloader: the Tx after "systemboot", an ELRS receiver after "bl"
const BEFORE = 'no_reset';
const FLASH_BAUD = 460800;
const ROM_BAUD = 115200;

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

function binaryString(data: Uint8Array): string {
  let s = '';
  for (let i = 0; i < data.length; i += 0x8000) s += String.fromCharCode(...data.subarray(i, i + 0x8000));
  return s;
}

async function connect(port: SerialPort, cb: FlashCallbacks) {
  const transport = new Transport(port, false);
  const loader = new ESPLoader({ transport, baudrate: FLASH_BAUD, romBaudrate: ROM_BAUD, terminal: terminal(cb.log) });
  const chip = await loader.main(BEFORE);
  cb.log(`chip: ${chip}`);
  return { transport, loader, chip };
}

export async function flashImage(port: SerialPort, image: Uint8Array, address: number, cb: FlashCallbacks): Promise<void> {
  const { transport, loader } = await connect(port, cb);
  try {
    await loader.writeFlash({
      fileArray: [{ data: binaryString(image), address }],
      flashSize: 'keep',
      flashMode: 'keep',
      flashFreq: 'keep',
      eraseAll: false,
      compress: true,
      reportProgress: (_i, written, total) => cb.progress(written, total),
    });
    await loader.after('hard_reset');
  } finally {
    await transport.disconnect();
  }
}

// the whole flash, to keep the original firmware before the first TetherLRS flash
export async function readWholeFlash(port: SerialPort, cb: FlashCallbacks): Promise<Uint8Array> {
  const { transport, loader } = await connect(port, cb);
  try {
    const sizeKb = (await loader.getFlashSize()) ?? 1024;
    const size = sizeKb * 1024;
    let got = 0;
    const data = await loader.readFlash(0, size, (packet) => {
      got += packet.length;
      cb.progress(got, size);
    });
    await loader.after('hard_reset');
    return data;
  } finally {
    await transport.disconnect();
  }
}
