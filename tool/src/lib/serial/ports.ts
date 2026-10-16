// Which ports the user can pick: the desktop app lists them, the browser asks through its own chooser.
// A TetherLRS Tx sits behind a plain USB serial adapter, so USB alone cannot tell it apart: ports where the tool
// once found one are remembered, the others are named after what their USB IDs say they are.

import { DemoSerialPort } from './demo-port';
import { listNativePorts, TauriSerialPort, type NativePortInfo } from './tauri-port';

export type PortKind = 'tlrs' | 'adapter' | 'esp' | 'other' | 'fc' | 'demo';

export interface KnownTx {
  txName: string;
  baud: number;
}

export interface PortEntry {
  key: string; // stable across refreshes, also the memory key
  label: string; // COM17
  kind: PortKind;
  name: string; // what it is, e.g. "USB serial adapter (CP210x)"
  detail: string; // product string and USB IDs
  known?: KnownTx;
  port: SerialPort;
}

export const isDesktop = typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;
export const hasWebSerial = typeof navigator !== 'undefined' && 'serial' in navigator;

const MEMORY = 'tetherlrs.knownPorts';
const ORDER: PortKind[] = ['tlrs', 'adapter', 'esp', 'other', 'fc', 'demo'];

const hex = (n?: number) => (n === undefined ? '' : n.toString(16).padStart(4, '0'));

function classify(vid?: number, pid?: number): { kind: PortKind; name: string } {
  const id = `${hex(vid)}:${hex(pid)}`;
  if (vid === 0x10c4) return { kind: 'adapter', name: 'USB serial adapter (CP210x)' };
  if (vid === 0x1a86) return { kind: 'adapter', name: id === '1a86:7523' ? 'USB serial adapter (CH340)' : 'USB serial adapter (CH9102)' };
  if (vid === 0x0403) return { kind: 'adapter', name: 'USB serial adapter (FTDI)' };
  if (vid === 0x067b) return { kind: 'adapter', name: 'USB serial adapter (PL2303)' };
  if (vid === 0x303a) return { kind: 'esp', name: 'ESP32 with native USB' };
  if (id === '0483:5740' || vid === 0x2e3c) return { kind: 'fc', name: 'Flight controller (USB VCP)' };
  if (vid === undefined) return { kind: 'other', name: 'Serial port' };
  return { kind: 'other', name: 'USB serial device' };
}

function loadMemory(): Record<string, KnownTx> {
  try {
    return JSON.parse(localStorage.getItem(MEMORY) ?? '{}');
  } catch {
    return {};
  }
}

function saveMemory(memory: Record<string, KnownTx>) {
  try {
    localStorage.setItem(MEMORY, JSON.stringify(memory));
  } catch {
    // private window or storage blocked: the tool just won't remember
  }
}

export function rememberTx(entry: PortEntry, txName: string, baud: number): void {
  if (entry.kind !== 'demo') saveMemory({ ...loadMemory(), [entry.key]: { txName, baud } });
}

export function forgetTx(entry: PortEntry): void {
  const memory = loadMemory();
  delete memory[entry.key];
  saveMemory(memory);
}

// ports the tool may ask with +++ on its own: never flight controllers or unknown devices
export const probeable = (entry: PortEntry) => entry.kind === 'adapter' || entry.kind === 'esp' || entry.kind === 'tlrs';

function entry(key: string, label: string, vid: number | undefined, pid: number | undefined, product: string | undefined, port: SerialPort, memory: Record<string, KnownTx>): PortEntry {
  const known = memory[key];
  const c = classify(vid, pid);
  const product_ = product?.replace(/\s*\(COM\d+\)\s*$/, ''); // Windows repeats the port name
  const detail = [product_, vid !== undefined ? `USB ${hex(vid)}:${hex(pid)}` : undefined].filter(Boolean).join(' · ');
  return known ? { key, label, kind: 'tlrs', name: `TetherLRS Tx · ${known.txName}`, detail, known, port } : { key, label, ...c, detail, port };
}

const demo = new DemoSerialPort();
const demoEntry: PortEntry = { key: 'demo', label: 'Demo Tx', kind: 'demo', name: '', detail: 'No hardware: answers like a TetherLRS Tx, to try the tool', port: demo as unknown as SerialPort };

const sorted = (list: PortEntry[]) => list.sort((a, b) => ORDER.indexOf(a.kind) - ORDER.indexOf(b.kind) || a.label.localeCompare(b.label, undefined, { numeric: true }));

export async function listPorts(): Promise<PortEntry[]> {
  const memory = loadMemory();
  if (isDesktop) {
    const native: NativePortInfo[] = await listNativePorts();
    const list = native.map((p) => entry(`${p.path}|${hex(p.vid)}:${hex(p.pid)}|${p.serial ?? ''}`, p.path, p.vid ?? undefined, p.pid ?? undefined, p.product ?? p.manufacturer ?? undefined, new TauriSerialPort(p) as unknown as SerialPort, memory));
    return [...sorted(list), demoEntry];
  }
  const granted = hasWebSerial ? await navigator.serial.getPorts() : [];
  const list = granted.map((port, i) => {
    const info = port.getInfo();
    return entry(`web|${hex(info.usbVendorId)}:${hex(info.usbProductId)}|${i}`, `Port ${i + 1}`, info.usbVendorId, info.usbProductId, undefined, port, memory);
  });
  return [...sorted(list), demoEntry];
}

// browser only: the browser's own chooser, which grants the page access to one more port
export async function requestPort(): Promise<SerialPort | undefined> {
  if (!hasWebSerial) return undefined;
  try {
    return await navigator.serial.requestPort();
  } catch {
    return undefined; // the user closed the chooser
  }
}
