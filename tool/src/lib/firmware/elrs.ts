// An ExpressLRS receiver on a USB-UART adapter: the same request ExpressLRS's own flasher sends, CRSF "bl" at
// 420000 baud. ESP8285 receivers restart into the ROM bootloader and print their target name first. ESP32 based
// ones go to ExpressLRS's own updater instead, so for those the ExpressLRS web page is the way.

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
  const writer = port.writable!.getWriter();
  let text = '';
  try {
    await writer.write(training);
    await new Promise((r) => setTimeout(r, 200));
    await writer.write(frame);
    const end = performance.now() + timeoutMs;
    const dec = new TextDecoder();
    while (performance.now() < end && !text.includes('\n')) {
      const next = await Promise.race([reader.read(), new Promise<undefined>((r) => setTimeout(r, end - performance.now()))]);
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
