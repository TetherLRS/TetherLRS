// How the parameters the firmware lists are shown: readable names, a line of help, and which ones stay hidden.

import type { Param } from './device/cli-parse';

export interface ParamMeta {
  label: string;
  help?: string;
}

export const SECTIONS = [
  { id: 'link', title: 'Radio link', help: 'Changing these restarts the Tx and the receiver.' },
  { id: 'ground', title: 'Ground radio (Tx)' },
  { id: 'air', title: 'Aircraft radio (Rx)', help: 'Read from the receiver while it is connected.' },
  { id: 'advanced', title: 'Advanced', help: 'Two receivers on one Tx, custom plans, the link key, tests.' },
] as const;

const ADVANCED = new Set(['Link Split', 'Radio Scheme', 'Rx Count', 'RF Band 2', 'Rx Downlink', 'Rx Ser Out2', 'Rx Ser Baudrate2', 'Rx Test', 'Rx Slot']);

export type SectionId = (typeof SECTIONS)[number]['id'];

const META: Record<string, ParamMeta> = {
  'Bind Phrase': { label: 'Bind phrase', help: 'Six characters, the same on both radios. SiK programs set it as NETID: sik025 is NETID 25.' },
  Mode: {
    label: 'Air mode',
    help: 'Faster modes carry more data, slower ones reach further. 17 Hz and 8 Hz are narrow, for custom plans below 1 GHz. Auto starts at 19 Hz and steps up while the signal has margin to spare, down when the link gets poor; the Device page shows the mode in use.',
  },
  'RF Band': { label: 'Band' },
  'RF Ortho': { label: 'Orthogonal hopping', help: 'Lets three links share the band without colliding.' },
  Privacy: { label: 'Encryption' },
  Region: {
    label: 'Region',
    help: 'EU follows the European rules: listen before talk, on-time limits per channel, power caps. Free sets no limits: the local rules are up to you. Custom runs the plan set under Advanced.',
  },
  'Dyn Power': {
    label: 'Dynamic power',
    help: 'Both radios turn their power down while the link is strong, never above the transmit power set for each, and back up at once when it weakens.',
  },
  'Tx Power': { label: 'Transmit power' },
  'Tx Diversity': { label: 'Antennas' },
  'Tx Ser Port': { label: 'Serial port' },
  'Tx Ser Baudrate': { label: 'Serial baud rate', help: 'Towards the ground station program. 115200 carries every air mode.' },
  'Tx Snd RadioStat': { label: 'RADIO_STATUS to the GCS' },
  'Tx Mav Component': { label: 'MAVLink component' },
  'Tx Ser Port2': { label: 'Second serial port' },
  'Tx Ser Baudrate2': { label: 'Second serial baud rate' },
  'Tx Buzzer': { label: 'Buzzer' },
  'Tx Downlink': {
    label: 'Aircraft radio transmits',
    help: 'Off: the aircraft radio stays silent, data goes only from the ground to the aircraft (corrections, commands). No telemetry, no link quality on the ground, and the receiver cannot be read or changed until it is back on, which takes effect at once. Receivers without TetherLRS ignore it.',
  },
  'Rx Power': { label: 'Transmit power' },
  'Rx Diversity': { label: 'Antennas' },
  'Rx Ser Port': { label: 'Serial port' },
  'Rx Ser Baudrate': { label: 'Serial baud rate', help: 'Towards the flight controller. 115200 carries every air mode.' },
  'Rx Ser Link Mode': { label: 'Serial protocol', help: 'Transparent passes any data. MAVLink modes add flow control towards the autopilot; mavlinkX also compresses.' },
  'Rx Mav System ID': { label: 'MAVLink system ID' },
  'Rx Snd RadioStat': { label: 'RADIO_STATUS to the autopilot', help: 'ardu_1 for ArduPilot and PX4.' },
  'Link Split': {
    label: 'Link split',
    help: 'How the air time is shared: balanced carries 82 bytes a frame each way, downlink 64 up and 100 down (telemetry), uplink the other way round (corrections, commands).',
  },
  'Radio Scheme': {
    label: 'Radio scheme',
    help: 'With two radios on both ends: dual bonded runs two lanes for twice the data and falls back to one frame on both radios while a channel is poor; dual redundant always sends every frame on two channels; single keeps the second radio for receiving only. Auto is bonded when the pair can.',
  },
  'Rx Count': {
    label: 'Receivers',
    help: 'How many aircraft radios this ground radio serves: each answers in its own slot of the cycle, which grows by a slot for every receiver beyond the first. A bind gives the receiver this count, and Bind to slot its slot too (a plain bind keeps the slot it has); without binding, set both on the Firmware page when flashing it. The Device page lists them.',
  },
  'RF Band 2': {
    label: 'Second band',
    help: 'With Air mode = auto, on radios that run both bands (LR1121, LR2021): the link may move to this band when it is faster for the margin there, and back. It starts on the lower of the two bands, which reaches further, and both radios go back there after 5 s without a link. One receiver only; not with a custom plan or orthogonal hopping. It shows once Air mode = auto is saved.',
  },
  'Rx Slot': {
    label: 'Reply slot',
    help: 'Which slot of the cycle this receiver answers in, 0 up to Receivers minus one. Two receivers in one slot collide; one beyond the count stays silent.',
  },
  'Rx Downlink': {
    label: 'Receiver transmits',
    help: 'Off keeps this receiver silent: a second receiver beside the one that answers, both hearing the same Tx. It cannot be read or changed over the air then.',
  },
  'Rx Ser Out2': {
    label: 'Second serial output',
    help: 'A second TX pin carrying the same data as the serial port: on the port\'s own RX pin (which then reads nothing, transparent mode only), or on the board\'s second serial pad.',
  },
  'Rx Ser Baudrate2': { label: 'Second output baud rate' },
  'Rx Test': { label: 'Test', help: 'Loopback sends what came down back up instead of out of the serial port: the loopback test on the Link page uses it.' },
  'Rx Mav Coalesce': {
    label: 'Freshest telemetry',
    help: 'MAVLink modes: when the link falls behind, an older copy of attitude, position and other state messages still waiting is dropped for the newer one. Parameters, missions and messages are never dropped. GCS link statistics count the dropped copies as lost.',
  },
};

export function paramMeta(name: string): ParamMeta {
  return META[name] ?? { label: name.replace(/^(Tx|Rx) /, '') };
}

// also those the hardware allows one value for: nothing to choose
export function paramHidden(p: Param): boolean {
  return p.unavailable || p.unchangeable;
}

export function paramSection(name: string): SectionId {
  if (ADVANCED.has(name)) return 'advanced';
  if (name.startsWith('Tx ')) return 'ground';
  if (name.startsWith('Rx ')) return 'air';
  return 'link';
}
