// A TetherLRS or mLRS receiver on its serial port, as a flight controller in passthrough lets the tool reach it:
// COMMAND_LONG MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN to the radio (system 51, component 68), param3 3 = to the
// bootloader, param7 the magic 1234321. Confirmation 1 only arms it and asks for an answer: the receiver says who it
// is (result_param2 1234321, or 1234322 on an ESP) and nothing happens. Confirmation 2 then sends it to the
// bootloader 1 s later. Every wait has a deadline.

import { timedReader } from '$lib/serial/timed-read';

const CRC_EXTRA: Record<number, number> = { 76: 152, 77: 143 };
const MAGIC = 1234321;

export function x25(data: Uint8Array, crc = 0xffff): number {
  for (const b of data) {
    let t = (b ^ (crc & 0xff)) & 0xff;
    t = (t ^ (t << 4)) & 0xff;
    crc = ((crc >> 8) ^ (t << 8) ^ (t << 3) ^ (t >> 4)) & 0xffff;
  }
  return crc;
}

// COMMAND_LONG to the radio component, as a GCS (255, 190) sends it
function commandLongFrame(seq: number, command: number, params: number[], confirmation: number, targetSystem: number): Uint8Array {
  const p = new DataView(new ArrayBuffer(33));
  params.forEach((v, i) => p.setFloat32(i * 4, v, true));
  p.setUint16(28, command, true);
  p.setUint8(30, targetSystem);
  p.setUint8(31, 68); // MAV_COMP_ID_TELEMETRY_RADIO
  p.setUint8(32, confirmation);
  const payload = new Uint8Array(p.buffer);
  const hdr = new Uint8Array([payload.length, 0, 0, seq & 0xff, 255, 190, 76, 0, 0]);
  const body = new Uint8Array([...hdr, ...payload]);
  const crc = x25(new Uint8Array([CRC_EXTRA[76]]), x25(body));
  return new Uint8Array([0xfd, ...body, crc & 0xff, crc >> 8]);
}

export function commandLong(seq: number, confirmation: number, targetSystem = 51): Uint8Array {
  return commandLongFrame(seq, 246, [0, 0, 3, 68, 0, 0, MAGIC], confirmation, targetSystem); // MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN
}

// opens a TetherLRS receiver's settings session (CommonRx/rx_cli.h): MAV_CMD_USER_1, "TLRS" in param1-4, the magic in
// param7; it answers COMMAND_ACK with result_param2 = magic + 1, and only while it hears no Tx
export const RXCLI_CMD = 31010;
export const RXCLI_ANSWER = MAGIC + 1;
export const rxCliUnlock = (seq: number, targetSystem = 51) => commandLongFrame(seq, RXCLI_CMD, [84, 76, 82, 83, 0, 0, MAGIC], 0, targetSystem);

export interface Ack {
  command: number;
  result: number;
  progress: number;
  param2: number;
}

// COMMAND_ACKs in a byte stream, checksums checked
export function findAcks(buf: Uint8Array): Ack[] {
  const out: Ack[] = [];
  for (let i = buf.indexOf(0xfd); i >= 0 && i + 12 <= buf.length; i = buf.indexOf(0xfd, i + 1)) {
    const n = buf[i + 1];
    const msgid = buf[i + 7] | (buf[i + 8] << 8) | (buf[i + 9] << 16);
    const end = i + 10 + n + 2;
    if (msgid !== 77 || end > buf.length) continue;
    const crc = x25(new Uint8Array([CRC_EXTRA[77]]), x25(buf.subarray(i + 1, i + 10 + n)));
    if ((crc & 0xff) !== buf[end - 2] || crc >> 8 !== buf[end - 1]) continue;
    const p = new Uint8Array(12);
    p.set(buf.subarray(i + 10, i + 10 + n));
    const v = new DataView(p.buffer);
    out.push({ command: v.getUint16(0, true), result: v.getUint8(2), progress: v.getUint8(3), param2: v.getInt32(4, true) });
  }
  return out;
}

export const HEARTBEAT = 0;
export const RADIO_STATUS = 109;
const RADIO_EXTRA: Record<number, number> = { [HEARTBEAT]: 50, [RADIO_STATUS]: 185 };

// what the radio component (51, 68) says on its own in a byte stream, checksums checked: a TetherLRS or mLRS Tx sends
// a HEARTBEAT every second, a receiver RADIO_STATUS
export function radioMessages(buf: Uint8Array): number[] {
  const out: number[] = [];
  for (let i = buf.indexOf(0xfd); i >= 0 && i + 12 <= buf.length; i = buf.indexOf(0xfd, i + 1)) {
    const n = buf[i + 1];
    const end = i + 12 + n;
    const msgid = buf[i + 7] | (buf[i + 8] << 8) | (buf[i + 9] << 16);
    if (end > buf.length || buf[i + 5] !== 51 || buf[i + 6] !== 68 || RADIO_EXTRA[msgid] === undefined) continue;
    const crc = x25(new Uint8Array([RADIO_EXTRA[msgid]]), x25(buf.subarray(i + 1, i + 10 + n)));
    if ((crc & 0xff) === buf[end - 2] && crc >> 8 === buf[end - 1]) out.push(msgid);
  }
  return out;
}

// only listens: nothing written, so nothing on the other side can be disturbed
export async function listen(port: SerialPort, baud: number, ms: number): Promise<number[]> {
  await port.open({ baudRate: baud });
  const reader = port.readable!.getReader();
  const read = timedReader(reader);
  const seen = new Set<number>();
  let buf = new Uint8Array(0);
  try {
    const end = performance.now() + ms;
    while (performance.now() < end) {
      const next = await read(end - performance.now());
      if (!next || next.done) break;
      buf = new Uint8Array([...buf.slice(-300), ...next.value]);
      for (const m of radioMessages(buf)) seen.add(m);
    }
  } finally {
    await reader.cancel().catch(() => {});
    reader.releaseLock();
    await port.close();
  }
  return [...seen];
}

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

async function ask(port: SerialPort, baud: number, confirmations: number[]): Promise<Ack | undefined> {
  await port.open({ baudRate: baud });
  const reader = port.readable!.getReader();
  const read = timedReader(reader);
  const writer = port.writable!.getWriter();
  let last: Ack | undefined;
  try {
    // bytes sent at another rate before can leave the receiver's parser inside a made-up frame up to 280 bytes long,
    // which then swallowed all three questions (measured on a DAKEFPV 2.4); zeros end it and start nothing new
    await writer.write(new Uint8Array(300));
    await sleep(50);
    for (const conf of confirmations) {
      let got: Ack | undefined;
      let buf = new Uint8Array(0);
      for (let attempt = 0; attempt < 3 && !got; attempt++) {
        await writer.write(commandLong(conf * 10 + attempt, conf));
        const end = performance.now() + 500;
        while (performance.now() < end && !got) {
          const next = await read(end - performance.now());
          if (!next || next.done) break;
          buf = new Uint8Array([...buf.slice(-600), ...next.value]);
          got = findAcks(buf).find((a) => a.command === 246 && (a.param2 === MAGIC || a.param2 === MAGIC + 1));
        }
      }
      if (!got) return undefined;
      last = got;
    }
    return last;
  } finally {
    writer.releaseLock();
    await reader.cancel().catch(() => {});
    reader.releaseLock();
    await port.close();
  }
}

// a receiver in a MAVLink mode answers; nothing is changed on it
export async function probeReceiver(port: SerialPort, baud: number): Promise<{ esp: boolean } | undefined> {
  const a = await ask(port, baud, [1]).catch(() => undefined);
  return a ? { esp: a.param2 === MAGIC + 1 } : undefined;
}

export async function receiverToBootloader(port: SerialPort, baud: number): Promise<boolean> {
  const a = await ask(port, baud, [1, 2]).catch(() => undefined);
  if (!a) return false;
  await sleep(1500); // it restarts 1 s after the second answer
  return true;
}
