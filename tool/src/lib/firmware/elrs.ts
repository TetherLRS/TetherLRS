// An ExpressLRS receiver on a USB-UART adapter: the same request ExpressLRS's own flasher sends, CRSF "bl" at
// 420000 baud. ESP8285 receivers restart into the ROM bootloader and print their target name first. ESP32 based
// ones go to ExpressLRS's own updater instead, so for those the ExpressLRS web page is the way.
// Not every adapter does 420000 (a CP2102 clone here did not): then the boot button and the ROM at 115200.
// Receivers with a USB-C on the C3's or S3's own USB need neither: esptool-js's USB reset puts them in the ROM.

import { timedReader } from '$lib/serial/timed-read';

const crc8 = (data: number[]) => {
  let crc = 0;
  for (const b of data) {
    crc ^= b;
    for (let i = 0; i < 8; i++) crc = crc & 0x80 ? ((crc << 1) ^ 0xd5) & 0xff : (crc << 1) & 0xff;
  }
  return crc;
};

export async function elrsToBootloader(port: SerialPort, timeoutMs = 2000): Promise<string | undefined> {
  const payload = [0x32, 0x62, 0x6c, ...Array.from('ESP82', (c) => c.charCodeAt(0))]; // COMMAND, 'b', 'l', key
  const frame = new Uint8Array([0xec, payload.length + 1, ...payload, crc8(payload)]);
  const training = new Uint8Array([0x07, 0x07, 0x12, 0x20, ...new Array(32).fill(0x55)]); // ROM baud training first

  await port.open({ baudRate: 420000 });
  const reader = port.readable!.getReader();
  const read = timedReader(reader);
  const writer = port.writable!.getWriter();
  let text = '';
  try {
    await writer.write(training);
    await new Promise((r) => setTimeout(r, 200));
    await writer.write(frame);
    const end = performance.now() + timeoutMs;
    const dec = new TextDecoder();
    while (performance.now() < end && !text.includes('\n')) {
      const next = await read(end - performance.now());
      if (!next || next.done) break;
      text += dec.decode(next.value, { stream: true });
    }
  } finally {
    writer.releaseLock();
    await reader.cancel().catch(() => {});
    reader.releaseLock();
    await port.close();
  }
  const name = text.split('\n')[0].replace(/[^\x20-\x7e]/g, '').trim();
  return name || undefined;
}

// one SLIP SYNC: the ROM bootloader answers with its own SYNC response, whatever brought it there (the boot button
// held at power on, "bl", the USB reset); leaves it in the ROM, ready for esptool-js. A C3's ROM keeps the rate of the
// first bytes it heard: 115200 from us, but 420000 if a "bl" got there first (measured on a DAKEFPV 2.4)
export async function romAnswers(port: SerialPort, baud = 115200, tries = 3): Promise<boolean> {
  const sync = new Uint8Array([0xc0, 0x00, 0x08, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x07, 0x12, 0x20, ...new Array(32).fill(0x55), 0xc0]);
  await port.open({ baudRate: baud });
  const reader = port.readable!.getReader();
  const read = timedReader(reader);
  const writer = port.writable!.getWriter();
  let got: number[] = [];
  try {
    for (let i = 0; i < tries; i++) {
      await writer.write(sync);
      const end = performance.now() + 150;
      while (performance.now() < end) {
        const next = await read(end - performance.now());
        if (!next || next.done) break;
        got = got.concat(Array.from(next.value));
      }
      for (let j = 0; j + 2 < got.length; j++) if (got[j] === 0xc0 && got[j + 1] === 0x01 && got[j + 2] === 0x08) return true;
    }
    return false;
  } finally {
    writer.releaseLock();
    await reader.cancel().catch(() => {});
    reader.releaseLock();
    await port.close();
  }
}
