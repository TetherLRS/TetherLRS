// Asks a port whether a TetherLRS Tx is behind it: the SiK escape at each baud rate, the CLI's "v", and back to
// data mode. Used only on USB serial adapters and ESP32 ports, never on flight controllers.

import { HEARTBEAT, listen, probeReceiver, RADIO_STATUS } from '$lib/firmware/mavlink';
import { DeviceLink } from './link';
import { TX_BAUDS, TxSession } from './tx';

export interface Identity {
  txName: string;
  txVersion?: string;
  baud: number;
}

// a TetherLRS radio of either kind: listening first (its Tx sends a HEARTBEAT every second, a receiver RADIO_STATUS),
// so a receiver gets only the MAVLink question that confirms it and no "+++" at rates it does not use
export async function recogniseRadio(port: SerialPort, firstBaud?: number): Promise<({ role: 'Tx' } & Identity) | { role: 'Rx' } | undefined> {
  const heard = await listen(port, 115200, 1500).catch(() => [] as number[]);
  if (heard.includes(RADIO_STATUS) && !heard.includes(HEARTBEAT) && (await probeReceiver(port, 115200))) return { role: 'Rx' };
  const tx = await identifyTx(port, heard.includes(HEARTBEAT) ? 115200 : firstBaud);
  return tx ? { role: 'Tx', ...tx } : undefined;
}

export async function identifyTx(port: SerialPort, firstBaud?: number): Promise<Identity | undefined> {
  for (const baud of [...new Set([firstBaud ?? TX_BAUDS[0], ...TX_BAUDS])]) {
    const link = new DeviceLink(port);
    try {
      await link.open(baud);
    } catch {
      return undefined; // in use by another program, or gone
    }
    try {
      const tx = new TxSession(link);
      if (await tx.enterCommandMode()) {
        const v = await tx.version();
        if (v) {
          await tx.exitCommandMode();
          return { txName: v.txName, txVersion: v.txVersion, baud };
        }
        await link.write('ATO\r'); // a SiK radio answers +++ as well: give it its data mode back
        return undefined;
      }
    } catch {
      return undefined; // the device took no data (a receiver's own USB the firmware does not read), or went away
    } finally {
      await link.close();
    }
  }
  return undefined;
}
