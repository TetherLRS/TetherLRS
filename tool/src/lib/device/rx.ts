// A TetherLRS receiver on its own serial: one MAVLink command opens its settings session, which the receiver allows only
// while it hears no Tx (CommonRx/rx_cli.h); after that the same text CLI as the Tx's, on a copy of its setup.

import { findAcks, RXCLI_ANSWER, RXCLI_CMD, rxCliUnlock } from '$lib/firmware/mavlink';
import { parseRxVersion, type DeviceVersion } from './cli-parse';
import { sleep } from './link';
import { TxSession } from './tx';

export const RX_BAUDS = [115200, 57600, 230400]; // its Rx Ser Baudrate, the default first

export class RxSession extends TxSession {
  override async enterCommandMode(): Promise<boolean> {
    // bytes sent before at another rate can leave its MAVLink parser inside a made-up frame: zeros end it
    await this.link.writeBytes(new Uint8Array(300));
    await sleep(50);
    for (let attempt = 0; attempt < 3; attempt++) {
      this.link.take();
      await this.link.writeBytes(rxCliUnlock(attempt));
      const end = performance.now() + 600;
      while (performance.now() < end) {
        if (findAcks(this.link.bytes()).some((a) => a.command === RXCLI_CMD && a.result === 0 && a.param2 === RXCLI_ANSWER)) {
          await sleep(100);
          this.link.take();
          return true;
        }
        await sleep(20);
      }
    }
    return false;
  }

  override async version(): Promise<DeviceVersion | undefined> {
    return parseRxVersion(await this.command('v'));
  }

  // the session ends after 10 s without a byte: an empty line keeps it, and the receiver prints nothing for it
  async keepAlive(): Promise<void> {
    await this.link.write('\r');
  }
}
