// A flight controller as a pipe to one of its UARTs: the CLI's "serialpassthrough <id> <baud>" (INAV and Betaflight,
// id 0 for UART1), after which the FC forwards until it is power cycled. The radio behind it is then reached as on a USB
// serial adapter: Identify, Connect, Flash and its settings session work through it. Nothing is ever saved on the FC.

import { DeviceLink, sleep } from '$lib/device/link';
import { romAnswers } from '$lib/firmware/elrs';
import { listen } from '$lib/firmware/mavlink';

// ports with a passthrough open, by key; the FC forgets it only when power cycled, the app when it closes
const open = new Map<string, number>();

export const passthroughUart = (key: string) => open.get(key);

// a flight controller that went away (unplugged, power cycled) forwards no more
export function forgetGonePassthroughs(present: Set<string>): void {
  for (const key of open.keys()) if (!present.has(key)) open.delete(key);
}

export async function openPassthrough(key: string, port: SerialPort, uart: number, baud = 115200): Promise<'opened' | 'already'> {
  // a radio heard on its own, or a bootloader answering: the FC forwards already (opened earlier, by this app or another
  // program), and the CLI's characters would only reach the radio
  if ((await listen(port, baud, 1500).catch(() => [] as number[])).length || (await romAnswers(port, baud).catch(() => false))) {
    open.set(key, uart);
    return 'already';
  }
  const link = new DeviceLink(port);
  await link.open(115200);
  try {
    link.take();
    await link.write('#');
    if ((await link.waitFor(/CLI|# ?$/m, 2000)) === undefined) {
      throw new Error('The flight controller did not open its CLI. If it forwards already to a radio that keeps quiet, power it off and on.');
    }
    await sleep(200);
    link.take();
    await link.write(`serialpassthrough ${uart - 1} ${baud}\r\n`);
    const answer = await link.waitFor(/Forwarding|opened|Invalid|not found|error/i, 3000);
    if (!answer || !/Forwarding|opened/i.test(answer)) {
      throw new Error(`The flight controller did not open UART${uart}: ${answer?.trim().split('\n').pop() ?? 'no answer'}.`);
    }
  } finally {
    await link.close();
  }
  open.set(key, uart);
  return 'opened';
}
