// A pretend TetherLRS Tx (DAKEFPV 915 Pro as ground radio) behind a Web Serial style port. It answers like the
// firmware's CLI (src/tx/cli.h), with the SiK-style escape, so the tool can be tried without hardware.

const GUARD_MS = 1000;

type Kind = 'list' | 'value' | 'phrase';

interface DemoParam {
  name: string;
  kind: Kind;
  options?: string[];
  value: number | string;
  allowed?: () => number[]; // list: allowed option indices, all if missing; [] = unavailable
  min?: number;
  max?: number;
  unit?: string;
}

const MODES = ['50 Hz', '31 Hz', '19 Hz', 'FLRC', 'FSK', '19 Hz 7x', '18 Hz', 'LR', '17 Hz', '8 Hz', 'auto'];
const BANDS = ['2.4 GHz', '915 MHz FCC', '868 MHz', '433 MHz', '70 cm HAM', '866 MHz IN', '915 MHz + 2.4 GHz', '868 MHz + 2.4 GHz'];

export class DemoSerialPort {
  readable: ReadableStream<Uint8Array> | null = null;
  writable: WritableStream<Uint8Array> | null = null;

  private out?: ReadableStreamDefaultController<Uint8Array>;
  private enc = new TextEncoder();
  private dec = new TextDecoder();
  private command = false;
  private lastHostMs = -GUARD_MS;
  private plus = 0;
  private plusTimer?: ReturnType<typeof setTimeout>;
  private line = '';
  private statsTimer?: ReturnType<typeof setInterval>;
  private params: DemoParam[];
  private linkKey = false;
  private plan = { centre: 869525, span: 250, lbt: 1, dbm: 27 };
  rxConnected = true;

  constructor() {
    const p = (name: string): DemoParam => this.params.find((x) => x.name === name)!;
    const region = () => p('Region').value as number;
    const band = () => p('RF Band').value as number;
    this.params = [
      { name: 'Bind Phrase', kind: 'phrase', value: 'mlrs.0' },
      {
        name: 'Mode', kind: 'list', options: MODES, value: 1,
        allowed: () => (band() === 2 ? (region() === 1 ? [4, 6, 7, 10] : [1, 2, 4, 5, 6, 7, 8, 9, 10]) : [1, 2, 4, 5, 8, 9, 10]),
      },
      { name: 'RF Band', kind: 'list', options: BANDS, value: 2, allowed: () => (region() === 1 ? [2] : [1, 2]) },
      { name: 'RF Ortho', kind: 'list', options: ['off', '1/3', '2/3', '3/3'], value: 0, allowed: () => (band() === 1 ? [0, 1, 2, 3] : []) },
      { name: 'Privacy', kind: 'list', options: ['off', 'lvl 1', 'lvl 2', 'lvl 3'], value: 0 },
      { name: 'Region', kind: 'list', options: ['free', 'EU', 'custom'], value: 0 },
      { name: 'Dyn Power', kind: 'list', options: ['off', 'on'], value: 1 },
      { name: 'Link Split', kind: 'list', options: ['balanced', 'downlink', 'uplink'], value: 0 },
      { name: 'Radio Scheme', kind: 'list', options: ['auto', 'single', 'dual redund', 'dual bonded', 'dual band'], value: 0, allowed: () => [0, 1, 2, 3] },
      { name: 'Rx Count', kind: 'list', options: ['1', '2', '3', '4'], value: 0 },
      { name: 'RF Band 2', kind: 'list', options: ['off', '2.4', '915 FCC', '868', '433', '70', '866 IN'], value: 0, allowed: () => (p('Mode').value === 10 ? [0, 1] : [0]) },
      { name: 'Tx Power', kind: 'list', options: ['25 mW', '50 mW', '100 mW', '250 mW', '500 mW'], value: 0 },
      { name: 'Tx Diversity', kind: 'list', options: ['enabled', 'antenna1', 'antenna2', 'r:en. t:ant1', 'r:en. t:ant2'], value: 1, allowed: () => [1] },
      { name: 'Tx Ser Port', kind: 'list', options: ['serial', 'wbridge', 'serial2', 'com', 'usb'], value: 0, allowed: () => [0, 4] },
      { name: 'Tx Ser Baudrate', kind: 'list', options: ['57600', '115200', '230400'], value: 1 },
      { name: 'Tx Ser Port2', kind: 'list', options: ['none', 'serial', 'wbridge', 'serial2'], value: 0, allowed: () => [] },
      { name: 'Tx Ser Baudrate2', kind: 'list', options: ['57600', '115200', '230400'], value: 1, allowed: () => [] },
      { name: 'Tx Snd RadioStat', kind: 'list', options: ['off', '1 Hz'], value: 0 },
      { name: 'Tx Mav Component', kind: 'list', options: ['off', 'enabled'], value: 0 },
      { name: 'Tx Buzzer', kind: 'list', options: ['off', 'LP', 'rxLQ'], value: 0, allowed: () => [] },
      { name: 'Tx Downlink', kind: 'list', options: ['on', 'off'], value: 0 },
      { name: 'Rx Power', kind: 'list', options: ['10 mW', '25 mW', '50 mW', '100 mW', '250 mW', '500 mW'], value: 1 },
      { name: 'Rx Diversity', kind: 'list', options: ['enabled', 'antenna1', 'antenna2', 'r:en. t:ant1', 'r:en. t:ant2'], value: 1, allowed: () => [1] },
      { name: 'Rx Ser Port', kind: 'list', options: ['serial', 'can', 'canfd', 'usb'], value: 0, allowed: () => [0, 3] },
      { name: 'Rx Ser Baudrate', kind: 'list', options: ['57600', '115200', '230400'], value: 1 },
      { name: 'Rx Ser Link Mode', kind: 'list', options: ['transp.', 'mavlink', 'mavlinkX', 'mspX'], value: 0 },
      { name: 'Rx Mav System ID', kind: 'list', options: ['51', '52', '53', '54', '55'], value: 0 },
      { name: 'Rx Snd RadioStat', kind: 'list', options: ['off', 'ardu_1', 'meth_b'], value: 0 },
      { name: 'Rx Mav Coalesce', kind: 'list', options: ['off', 'on'], value: 0 },
      { name: 'Rx Downlink', kind: 'list', options: ['as Tx', 'off'], value: 0 },
      { name: 'Rx Ser Out2', kind: 'list', options: ['off', 'tx on rx pin', 'serial 1'], value: 0, allowed: () => [0, 1] },
      { name: 'Rx Ser Baudrate2', kind: 'list', options: ['57600', '115200', '230400'], value: 1 },
      { name: 'Rx Test', kind: 'list', options: ['off', 'loopback'], value: 0 },
      { name: 'Rx Slot', kind: 'list', options: ['0', '1', '2', '3'], value: 0 },
    ];
  }

  // several receivers: the pretend ones beyond the first answer too, up to the count; the selected one is read
  private selectedRx = 0;
  private rxCount() { return (this.find('RX_COUNT')!.value as number) + 1; }
  private rxUp(slot: number) { return slot < Math.min(this.rxCount(), 3); } // the fourth never shows up
  private rxLine(slot: number) {
    if (!this.rxUp(slot)) return `${slot === this.selectedRx ? '  *' : '   '}${slot}: not connected`;
    return `${slot === this.selectedRx ? '  *' : '   '}${slot}: ${slot === 0 ? 'DAKEFPV 900 NANOPRO' : 'GENERIC 900 RX'}, v2.0-dev, LQ ${String(98 - slot).padStart(3, '0')} %, RSSI -0${62 + slot * 4} dBm`;
  }

  getInfo(): SerialPortInfo {
    return { usbVendorId: 0x10c4, usbProductId: 0xea60 };
  }

  async open(_options: SerialOptions): Promise<void> {
    this.readable = new ReadableStream<Uint8Array>({ start: (c) => { this.out = c; } });
    this.writable = new WritableStream<Uint8Array>({ write: (chunk) => this.fromHost(chunk) });
  }

  async close(): Promise<void> {
    clearInterval(this.statsTimer);
    clearTimeout(this.plusTimer);
    try {
      this.out?.close();
    } catch {
      // already cancelled by the reader
    }
    this.readable = null;
    this.writable = null;
  }

  async setSignals(_signals: SerialOutputSignals): Promise<void> {}

  private send(text: string) {
    try {
      this.out?.enqueue(this.enc.encode(text));
    } catch {
      // closed
    }
  }

  private putsn(s: string) {
    this.send(s + '\r\n');
  }

  private sendBytes(b: Uint8Array) {
    try {
      this.out?.enqueue(b);
    } catch {
      // closed
    }
  }

  private fromHost(chunk: Uint8Array) {
    const now = performance.now();
    // the receiver in loopback: what went down the link comes back up, a frame or two later
    // (the firmware holds a possible escape back until its guard is over, so a run of '+' is not echoed)
    if (!this.command && this.find('RX_TEST')!.value === 1 && !/^\+{1,3}$/.test(this.dec.decode(chunk))) setTimeout(() => this.sendBytes(chunk.slice()), 60);
    for (const ch of this.dec.decode(chunk)) {
      if (this.command) {
        this.cliChar(ch);
      } else if (ch === '+' && this.plus < 3 && (this.plus > 0 || now - this.lastHostMs >= GUARD_MS)) {
        this.plus++;
        clearTimeout(this.plusTimer);
        if (this.plus === 3) this.plusTimer = setTimeout(() => this.escape(), GUARD_MS);
      } else {
        this.plus = 0; // data for the link, which the demo drops
        clearTimeout(this.plusTimer);
      }
      this.lastHostMs = now;
    }
  }

  private escape() {
    if (this.plus !== 3) return;
    // as the firmware, which looks again every loop: a timer can fire before performance.now() says the guard is over
    const left = GUARD_MS - (performance.now() - this.lastHostMs);
    if (left > 0) {
      this.plusTimer = setTimeout(() => this.escape(), Math.ceil(left));
      return;
    }
    this.plus = 0;
    this.command = true;
    this.line = '';
    this.send('\r\nOK\r\n');
  }

  private cliChar(ch: string) {
    if (this.statsTimer !== undefined) {
      clearInterval(this.statsTimer);
      this.statsTimer = undefined;
      this.putsn('  streaming stats stopped');
      return;
    }
    if (ch !== '\r' && ch !== '\n' && ch !== ',' && ch !== ';') {
      this.send(ch);
      this.line += ch;
      return;
    }
    const line = this.line;
    this.line = '';
    if (!line) return;
    if (/^(at|rt)/i.test(line)) {
      this.putsn('');
      this.sikAt(line.toUpperCase().replace(/ /g, ''));
      return;
    }
    this.putsn('>');
    this.cli(line);
  }

  // a few of cli.h's SiK answers (src/tx/sik_at.h), enough to try the Terminal page
  private sikAt(cmd: string) {
    const regs = ['FORMAT=26', 'SERIAL_SPEED=115', 'AIR_SPEED=19', 'NETID=137', 'TXPOWER=14', 'ECC=0', 'MAVLINK=0', 'OPPRESEND=0',
      'MIN_FREQ=863275', 'MAX_FREQ=868000', 'NUM_CHANNELS=6', 'DUTY_CYCLE=100', 'LBT_RSSI=0', 'MANCHESTER=0', 'RTSCTS=0', 'MAX_WINDOW=32'];
    if (cmd === 'ATO') this.command = false;
    else if (cmd === 'AT' || cmd === 'AT&W' || /^ATS\d+=\d+$/.test(cmd)) this.putsn('OK');
    else if (cmd === 'ATI') this.putsn('SiK 2.2 on TetherLRS DAKEFPV 900 MAX 1W TX');
    else if (cmd === 'ATI5') regs.forEach((r, i) => this.putsn(`S${i}:${r}`));
    else this.putsn('ERROR');
  }

  private find(cliName: string): DemoParam | undefined {
    return this.params.find((p) => p.name.toUpperCase().replace(/ /g, '_') === cliName.toUpperCase());
  }

  private allowed(p: DemoParam): number[] {
    return p.allowed ? p.allowed() : (p.options ?? []).map((_, i) => i);
  }

  private printParam(p: DemoParam) {
    const allowed = this.allowed(p);
    if (p.kind === 'list' && allowed.length === 0) {
      this.putsn(`  ${p.name} = - (unavailable)`);
    } else if (p.kind === 'list') {
      const i = p.value as number;
      this.putsn(`  ${p.name} = ${p.options![i]} [${String.fromCharCode(48 + i)}]${allowed.length === 1 ? '(unchangeable)' : ''}`);
    } else if (p.kind === 'value') {
      this.putsn(`  ${p.name} = ${p.value} ${p.unit ?? ''}`.trimEnd());
    } else {
      this.putsn(`  ${p.name} = ${p.value}`);
    }
  }

  private cli(line: string) {
    const isRx = (p: DemoParam) => p.name.startsWith('Rx ');
    if (line === 'v') {
      this.putsn('  Tx: DAKEFPV 900 MAX 1W TX, v2.0-dev');
      this.putsn(this.rxConnected ? '  Rx: DAKEFPV 900 NANOPRO, v2.0-dev' : '  Rx: receiver not connected');
    } else if (line === 'pl' || line === 'pl c' || line === 'pl tx' || line === 'pl rx') {
      this.putsn('ConfigId:0');
      if (!this.rxConnected) this.putsn('warn: receiver not connected');
      for (const p of this.params) {
        const g = p.name.startsWith('Tx ') ? 'tx' : isRx(p) ? 'rx' : 'c';
        if (line !== 'pl' && line !== `pl ${g}`) continue;
        if (isRx(p) && !this.rxConnected) continue;
        this.printParam(p);
      }
    } else if (line.startsWith('p ') && /[A-Za-z]/.test(line[2] ?? '')) {
      // as cli.h is_cmd_param_set(): blanks removed, name in upper case, no "=" asks for the value with its options
      const s = line.slice(2).replace(/ /g, '');
      const sep = s.indexOf('=');
      const p = this.find((sep < 0 ? s : s.slice(0, sep)).toUpperCase());
      const v = sep < 0 ? '?' : s.slice(sep + 1);
      if (!p) return this.putsn('err: invalid parameter name');
      if (isRx(p) && !this.rxConnected) return this.putsn('warn: receiver not connected');
      if (v === undefined) return this.printParam(p);
      if (v === '?') {
        this.putsn('ConfigId:0');
        this.printParam(p);
        if (p.kind === 'list') for (const i of this.allowed(p)) this.putsn(`  ${String.fromCharCode(48 + i)} = ${p.options![i]}`);
        else if (p.kind === 'value') { this.putsn(`  min: ${p.min}`); this.putsn(`  max: ${p.max}`); }
        else this.putsn('  [a-z0-9#-._]');
        return;
      }
      if (v === '') return this.putsn('err: no value specified');
      if (p.kind === 'phrase') {
        if (!/^[a-z0-9#\-._]{6}$/.test(v)) return this.putsn('err: invalid value');
        p.value = v;
      } else if (p.kind === 'value') {
        const n = parseInt(v, 10);
        if (isNaN(n) || n < p.min! || n > p.max!) return this.putsn('err: invalid value');
        p.value = n;
      } else {
        const n = parseInt(v, 10);
        if (isNaN(n) || !this.allowed(p).includes(n)) return this.putsn('err: invalid value');
        p.value = n;
      }
      this.putsn('ConfigId:0');
      this.printParam(p);
    } else if (line === 'rxlist') {
      for (let slot = 0; slot < this.rxCount(); slot++) this.putsn(this.rxLine(slot));
    } else if (/^setrx\s*=/.test(line)) {
      const n = parseInt(line.slice(line.indexOf('=') + 1).trim(), 10);
      if (!(n >= 0 && n < this.rxCount())) return this.putsn('err: the slot is 0 up to Rx Count - 1');
      this.selectedRx = n;
      this.putsn(`  receiver ${n} selected, its parameters follow once it answers`);
    } else if (line === 'bind') {
      this.putsn('  Tx entered bind mode');
    } else if (/^bind\s*=/.test(line)) {
      const n = parseInt(line.slice(line.indexOf('=') + 1).trim(), 10);
      if (!(n >= 0 && n < this.rxCount())) return this.putsn('err: the slot is 0 up to Rx Count - 1');
      this.putsn(`  Tx entered bind mode, the receiver gets slot ${n}`);
    } else if (line === 'pstore') {
      this.putsn('ConfigId:0');
      this.putsn(this.rxConnected ? '  parameters stored' : 'warn: receiver not connected');
      setTimeout(() => { this.command = false; }, 300); // restarts into data mode, as with a common change
    } else if (line === 'stats') {
      this.putsn('  starts streaming stats');
      this.putsn('  send any character to stop');
      this.statsTimer = setInterval(() => this.statsLine(), 500);
    } else if (line === 'exit' || line === 'ato') {
      this.putsn('  back to data');
      this.command = false;
    } else if (line === 'systemboot') {
      this.command = false;
    } else if (line === 'h' || line === 'help' || line === '?') {
      for (const l of ['  help, h, ?      -> this help page', '  v               -> print device and version', '  pl              -> list all parameters',
        '  p name          -> get parameter value', '  p name = value  -> set parameter value', '  pstore          -> store parameters',
        '  listfreqs       -> lists frequencies used in fhss scheme', '  ATI5, ATSn=v... -> SiK AT commands, RT... for the receiver']) this.putsn(l);
    } else if (/^linkkey\s*$/.test(line)) {
      this.putsn(this.linkKey ? '  link key: set' : '  link key: not set');
    } else if (/^linkkey\s*=/.test(line)) {
      const v = line.slice(line.indexOf('=') + 1).trim();
      if (v === 'none') {
        this.linkKey = false;
        this.putsn('  link key: cleared, pstore keeps it');
      } else if (/^[0-9a-fA-F]{64}$/.test(v)) {
        this.linkKey = true;
        this.putsn('  link key: set, pstore keeps it, the receiver needs the same');
      } else this.putsn('err: 64 hex digits, or none');
    } else if (/^customplan\s*$/.test(line)) {
      this.printPlan();
    } else if (/^customplan\s*=/.test(line)) {
      const v = line.slice(line.indexOf('=') + 1).split('/').map((x) => x.trim());
      if (v.length !== 4 || v.some((x) => !/^\d+$/.test(x))) return this.putsn('err: customplan = <centre kHz>/<span kHz>/<lbt 0 or 1>/<max dBm>');
      const [centre, span, lbt, dbm] = v.map((x) => parseInt(x, 10));
      if (centre < 863000 || centre > 870000) return this.putsn('err: the centre is outside the band, 863000 to 870000 kHz');
      if (span < 25 || span > 7000) return this.putsn("err: the span is 25 kHz up to the band's width");
      if (lbt > 1) return this.putsn('err: lbt is 0 or 1');
      if (dbm > 33) return this.putsn('err: the power cap is 0 to 33 dBm');
      this.plan = { centre, span, lbt, dbm };
      this.printPlan();
      this.putsn('  pstore keeps it, Region = custom runs it');
    } else if (line === 'listfreqs') {
      [863.325, 865.333, 864.0, 867.667, 868.3].forEach((f, i) => this.putsn(`  ${i}: ${Math.round(f * 1000)} kHz`));
    } else {
      this.putsn('  invalid cmd');
    }
  }

  private printPlan() {
    const { centre, span, lbt, dbm } = this.plan;
    this.putsn(`  custom plan: ${centre} kHz centre, ${span} kHz span, ${lbt ? 'LBT on' : 'no LBT'}, ${dbm} dBm (${Math.round(10 ** (dbm / 10))} mW) max`);
    const bw = [500, 500, 500, 1000, 250, 500, 500, 125, 250, 125][this.find('MODE')!.value as number] ?? 500;
    this.putsn(`  ${Math.min(32, Math.max(1, Math.floor(span / bw)))} channel(s) of ${bw} kHz in the current mode`);
  }

  private statsLine() {
    const r = (a: number, b: number) => Math.round(a + Math.random() * (b - a));
    const p3 = (n: number) => String(n).padStart(3, '0');
    const s8 = (n: number) => (n < 0 ? '-' : ' ') + p3(Math.abs(n));
    const p5 = (n: number) => String(n).padStart(5, '0');
    const p10 = (n: number) => String(n).padStart(10, '0');
    const lq = r(92, 100);
    let line = `${p3(lq)}(${p3(Math.min(100, lq + r(0, 4)))}),${p3(r(90, 100))}, ${s8(r(-75, -60))}/${s8(-127)},${s8(r(-78, -62))}, ${s8(r(5, 9))}/${s8(0)}; ${p5(r(80, 400))}, ${p5(r(900, 1450))};`;
    if (this.find('REGION')!.value === 1) line += ` cca ${p10(r(0, 3))}, lim ${p10(0)};`;
    line += ` pwr ${s8(this.find('DYN_POWER')!.value === 1 ? r(10, 14) : 14)}; rxp ${this.find('RX_POWER')!.options![this.find('RX_POWER')!.value as number]};`;
    const scheme = this.find('RADIO_SCHEME')!.value as number;
    const sch = scheme === 1 ? 1 : scheme === 2 ? 2 : 3; // the demo pretends two radios on both ends: auto is bonded
    line += ` sch ${sch}; split ${this.find('LINK_SPLIT')!.value};`;
    if (this.find('MODE')!.value === 10) { // auto: 19 Hz in use, on the band set
      line += ` mode ${p3(2)};`;
      if (this.find('RF_BAND_2')!.value !== 0) line += ` band ${this.find('RF_BAND')!.value};`;
    }
    if (sch === 3) line += ` lanes 2;`;
    if (sch >= 2) line += ` ant ${p3(r(95, 100))}/${p3(r(90, 100))};`;
    if (this.rxCount() > 1) {
      line += ' mp';
      for (let slot = 0; slot < this.rxCount(); slot++) {
        line += ` ${slot}${slot === this.selectedRx ? '*' : ':'}${this.rxUp(slot) ? `${p3(98 - slot)}/${s8(-62 - slot * 4)}` : '-'}`;
      }
      line += ';';
    }
    this.putsn(line);
  }
}
