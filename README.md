<p align="center"><img src="tool/src-tauri/icons/128x128@2x.png" width="112" alt="TetherLRS logo"></p>

<h1 align="center">TetherLRS</h1>

A long-range telemetry and data link for drones and planes, on the radio hardware people already have: ExpressLRS
receivers and modules, and mLRS boards. The core is [mLRS](https://github.com/olliw42/mLRS): its radio link,
protocol and MAVLink handling. TetherLRS turns that into a plain data modem, like a SiK radio, and runs it on any
receiver, which can be the aircraft radio or the ground radio.

**Status: 1.0.** Built for the boards in
[Hardware](#hardware)). Read the [disclaimer](#disclaimer-and-responsibility) before you transmit.

## What it does

- **A serial link, both ways**: MAVLink (ArduPilot, PX4, INAV), MSP (INAV) or any data in transparent mode, between
  a ground station program and the flight controller. No RC channels: fly with your usual radio link, TetherLRS
  carries the telemetry, missions and commands.
- **The air link of mLRS**: LoRa at 50, 31, 19 and 18 Hz, FLRC and FSK, frequency hopping, optional encryption,
  2.4 GHz, 915 MHz (FCC), 868 MHz, 866 MHz (IN) and 433 MHz, on SX1280, SX127x and LR1121 radios.
- **MAVLink that fits the link**: mavlinkX compression, flow control through RADIO_STATUS, and optionally the
  freshest copy of state messages first when the link falls behind.
- **A ground radio that looks like a SiK radio**: `+++` and AT commands on the same serial port, NETID as bind phrase,
  so programs that know SiK work with it.
- **Dynamic power** on both radios: each turns its power down while the link is strong and back up at once when it
  weakens. The power you set is the ceiling.
- **EU region**: the 868 MHz and 2.4 GHz rules (listen before talk, on-time limits per channel, power caps) chosen at
  run time, with the same firmware.
- **Downlink off**: the aircraft radio can stay silent; data then goes one way only, from the ground (RTK corrections,
  commands).
- **The TetherLRS tool**, for Windows, macOS and Linux or in the browser: it finds the radios, backs them up, flashes
  them without pressing buttons (also through a flight controller's serial passthrough), sets the bind phrase into the
  image, and changes the settings of both radios.

## Hardware

Any ESP receiver ExpressLRS supports can run TetherLRS, as aircraft radio or as ground radio, and so can the mLRS
STM32 boards. Builds come from the ExpressLRS hardware descriptions, so the pins and the power tables are the ones the
manufacturers set.

| | Builds |
|---|---|
| ExpressLRS receivers, ESP8285, ESP32, ESP32-C3, ESP32-S3 | 213 of 216 products |
| ExpressLRS Tx modules, external, ESP32, as ground radio on their USB-C | 53 of 55 products |
| mLRS STM32 receivers and Tx boards: FrSky R9, Matek mR900 and mR24, Wio-E5, E77, DIY E22/E28/LR2021 | 49 images, and 4 for R9 boards with the ExpressLRS bootloader |

Not supported yet: two ESP8285 PWM receivers and the HappyModel EPW6 (their layouts lack a radio
pin TetherLRS needs), the CoreWing Sirius Tx module (ESP32-S3) and the DIY BLE joystick (no radio). Modules inside a
radio (TX16S, Zorro, Boxer and alike) are left out on purpose: they talk only to their radio.

A Tx module runs as a ground radio on its USB-C port: the data, and the CLI after `+++`. Its fan turns on from
250 mW; its WiFi backpack and radio bay pins are not used. USB alone may not power the higher levels: power the module
the way its maker says.

A module with a screen shows the link and the aircraft; its joystick moves through the pages (up and down) and the
menus (left and right), and changes settings. The pages: **Link** (RSSI and LQ both ways, power), **Aircraft** (from the
autopilot's MAVLink: autopilot, armed, battery, GPS, height, speed, how fresh the telemetry is), **Session** (time on the
link, links lost, data sent and received), **Radio** (mode, power, diversity), **Data** (antennas, bytes per second
both ways), **About** (names and versions); then the Common, Tx and Rx settings and the actions (store, bind). Screens
on I2C and SPI and the 80x160 TFT are supported; a screen without a joystick shows the pages in turn. On some modules (HappyModel ES24 and ES900, BETAFPV Micro, NamimnoRC OLED,
QuadKopters, AXIS Thor) the WiFi backpack sits on the same serial lines as the USB-C, as mLRS also has it on the BETAFPV
Micro 1W.

## Getting started

1. Get the tool: the desktop app from [Releases](../../releases), or the web app (Chrome or Edge) on this
   repository's GitHub Pages.
2. Connect a radio to the computer: a receiver through a USB serial adapter, a Tx module or a board with USB directly,
   or a receiver already wired to a flight controller, through the flight controller's USB (the tool opens the INAV or
   Betaflight serial passthrough; nothing is saved on the flight controller).
3. On the **Firmware** page: Identify, Back up, choose ground radio (Tx) or aircraft radio (Rx), type a bind phrase
   (six characters, the same on both radios), Flash. Do the other radio the same way.
4. Wire the aircraft radio to a free UART of the flight controller and set that UART to MAVLink telemetry at 115200:
   - INAV: Ports tab, the UART's Telemetry set to MAVLink (in the CLI, function 256), 115200;
   - ArduPilot: `SERIALx_PROTOCOL = 2`, `SERIALx_BAUD = 115`;
   - PX4: `MAV_x_CONFIG` on that port, `SER_TELx_BAUD = 115200`.
5. Plug the ground radio into the computer and connect the ground station program (QGroundControl, Mission
   Planner, Kite, ...) to its serial port at 115200. Tested with INAV and Kite. In QGroundControl, turn off AutoConnect
   for SiK radios (it takes USB serial adapters at 57600) and add a serial link at 115200.

The **Device** and **Parameters** pages show the link and change the settings of both radios through the ground
radio. A receiver on its own (no ground radio near) can also be set directly on its serial port.

## Back to ExpressLRS or mLRS

TetherLRS can always be replaced, and the radio put back as it was:

- **ESP radios.** On the Firmware page, Identify and Connect: TetherLRS puts the radio into its ROM bootloader, no
  button needed. Then either
  - **Write a file** with your backup: the radio is back exactly as it was, ExpressLRS binding and settings included;
  - or close the tool and flash ExpressLRS with the ExpressLRS Configurator (flashing method UART) or mLRS with its
    own instructions: the radio waits in its bootloader. Bind it again afterwards.

  If TetherLRS does not start (power cut during a flash), hold the boot button while powering the radio on: the ROM
  bootloader always answers, and the tool or esptool can write again.
- **STM32 boards** go back the way they came to TetherLRS: an ST-Link, or the bootloader they have (the ExpressLRS
  bootloader on R9 boards), with the mLRS or ExpressLRS image.

## Help and feedback

- **Documentation**: the [wiki](https://github.com/TetherLRS/TetherLRS/wiki), from getting started to every parameter.
- **Questions**: [Discussions](https://github.com/TetherLRS/TetherLRS/discussions/categories/q-a).
- **A board tried**, working or not: a [hardware report](https://github.com/TetherLRS/TetherLRS/issues/new?template=hardware-report.yml).
- **A bug**: a [bug report](https://github.com/TetherLRS/TetherLRS/issues/new?template=bug-report.yml).

## Building

Firmware, in `firmware/` (mLRS's tree, as a git subtree):

```
git submodule update --init --recursive
cd firmware/mLRS/Common/mavlink && python fmav_generate_c_library.py && cd -
cd firmware
pio run -e rx-bayck-ur500                    # an ESP build, every env is in platformio*.ini
(cd tools && python run_copy_st_drivers.py -silent)              # STM32: the ST drivers into each target
(cd mLRS/Common/dronecan && python dronecan_generate_c_library.py -np)  # needs empy 3.3.4, pexpect, dronecan
python tools/run_make_firmwares.py --nopause # every STM32 build, arm-none-eabi-gcc on the PATH
test/run_tests.sh                            # host tests
```

The hardware files of the ExpressLRS receivers and Tx modules are generated from the `firmware/elrs-targets` submodule by
`tools/tetherlrs_gen_elrs.py`; `tools/tetherlrs_tx_envs.py` makes every receiver also a ground radio, and
`tools/tetherlrs_elrs_products.py` tells the tool which build fits which ExpressLRS product. CI checks they are
up to date.

The tool, in `tool/`: `npm ci`, then `npm run tauri dev` (desktop) or `npm run dev` (web), `npx vitest run` (tests).

## Disclaimer and responsibility

**TetherLRS is experimental software, provided as is, without warranty of any kind** (GNU GPL v3, sections 15 and 16).
You use it, and the hardware you flash with it, entirely at your own risk.

- **Radio regulations are your responsibility.** Where you operate decides which frequencies you may use, with how
  much power (antenna gain included), for how long (duty cycle, listen before talk), with or without a licence (433 MHz
  is an amateur band in many countries), and whether a transmitter may fly. The default region is **Free: no limits
  are applied**. The EU region applies the rules of EN 300 220-2 and EN 300 328 as we understand them; it is not a
  certification and does not make a device compliant. Hardware sold for one market (915 MHz FCC, for example) is not
  necessarily legal in another.
- **Not certified.** A reflashed device is no longer the product its maker had certified.
- **Not for flight control and not safety critical.** TetherLRS carries no RC channels; a link can always fail. Keep
  an independent control link, plan for losing telemetry, and follow your aviation authority's rules.
- **A transmitter on the aircraft can disturb it.** We have seen aileron servos jitter with a 500 mW receiver antenna
  next to the servo leads. Check every installation on the ground, at the highest power it will use in the air, and
  keep antennas away from servo and sensor wiring.
- **Flashing can leave a device unusable and voids warranties.** Back it up first; the tool does it in one click.
- **Keep away from transmitting antennas**, especially at high power.
- **Not affiliated** with the mLRS project or with ExpressLRS LLC, nor endorsed by them. Product and brand names belong
  to their owners and are used only to say which hardware works.

## Credits

- **[mLRS](https://github.com/olliw42/mLRS)** by OlliW and the mLRS contributors: the radio link, its protocol and most
  of the firmware. `firmware/` starts from mLRS at commit `1eca7374` (v1.4.05 dev). Thank you.
- **[ExpressLRS](https://github.com/ExpressLRS/ExpressLRS)** and its community: the hardware descriptions in
  [ExpressLRS/targets](https://github.com/ExpressLRS/targets) the builds are generated from, the single-radio antenna
  diversity, and the hardware TetherLRS runs on.
- [fastMAVLink](https://github.com/olliw42/fastmavlink) and [MAVLink](https://mavlink.io), Semtech's radio drivers as
  mLRS uses them, [NeoPixelBus](https://github.com/Makuna/NeoPixelBus), [esptool-js](https://github.com/espressif/esptool-js),
  [Tauri](https://tauri.app) and [Svelte](https://svelte.dev).

## License

GNU General Public License v3.0, the license of mLRS and ExpressLRS, which TetherLRS derives from: see [LICENSE](LICENSE).
