// Asks a port whether a TetherLRS Tx is behind it: the SiK escape at each baud rate, the CLI's "v", and back to
// data mode. Used only on USB serial adapters and ESP32 ports, never on flight controllers.

import { DeviceLink } from './link';
import { TX_BAUDS, TxSession } from './tx';

export interface Identity {
  txName: string;
  txVersion?: string;
  baud: number;
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
