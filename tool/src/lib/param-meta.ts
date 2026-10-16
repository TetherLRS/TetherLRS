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
] as const;

export type SectionId = (typeof SECTIONS)[number]['id'];

const META: Record<string, ParamMeta> = {
  'Bind Phrase': { label: 'Bind phrase', help: 'Six characters, the same on both radios. SiK programs set it as NETID: sik025 is NETID 25.' },
  Mode: { label: 'Air mode', help: 'Faster modes carry more data, slower ones reach further.' },
  'RF Band': { label: 'Band' },
  'RF Ortho': { label: 'Orthogonal hopping', help: 'Lets three links share the band without colliding.' },
  Privacy: { label: 'Encryption' },
  Region: {
    label: 'Region',
    help: 'EU follows the European rules: listen before talk, on-time limits per channel, power caps. Free sets no limits: the local rules are up to you.',
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
  'Rx Power': { label: 'Transmit power' },
  'Rx Diversity': { label: 'Antennas' },
  'Rx Ser Port': { label: 'Serial port' },
  'Rx Ser Baudrate': { label: 'Serial baud rate', help: 'Towards the flight controller. 115200 carries every air mode.' },
  'Rx Ser Link Mode': { label: 'Serial protocol', help: 'Transparent passes any data. MAVLink modes add flow control towards the autopilot; mavlinkX also compresses.' },
  'Rx Mav System ID': { label: 'MAVLink system ID' },
  'Rx Snd RadioStat': { label: 'RADIO_STATUS to the autopilot', help: 'ardu_1 for ArduPilot and PX4.' },
  'Rx Mav Coalesce': {
    label: 'Freshest telemetry',
    help: 'MAVLink modes: when the link falls behind, an older copy of attitude, position and other state messages still waiting is dropped for the newer one. Parameters, missions and messages are never dropped. GCS link statistics count the dropped copies as lost.',
  },
};

// RC and failsafe: the firmware leaves them out (TLRS_NO_RC), firmware built without it still lists them
const RC = /^(Tx Ch |Tx In Mode|Tx Power Sw Ch|Rx Ch Order|Rx Out |Rx FailSafe|Rx FS Ch|Rx Power Sw Ch|Rx Snd RcChannel)/;

export function paramMeta(name: string): ParamMeta {
  return META[name] ?? { label: name.replace(/^(Tx|Rx) /, '') };
}

// also those the hardware allows one value for: nothing to choose
export function paramHidden(p: Param): boolean {
  return p.unavailable || p.unchangeable || RC.test(p.name);
}

export function paramSection(name: string): SectionId {
  if (name.startsWith('Tx ')) return 'ground';
  if (name.startsWith('Rx ')) return 'air';
  return 'link';
}
