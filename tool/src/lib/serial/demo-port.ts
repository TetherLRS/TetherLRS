// A pretend TetherLRS Tx (DAKEFPV 915 Pro as ground radio) behind a Web Serial style port. It answers like the
// firmware's CLI (mLRS/CommonTx/cli.h), with the SiK-style escape, so the tool can be tried without hardware.

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

const MODES = ['50 Hz', '31 Hz', '19 Hz', 'FLRC', 'FSK', '19 Hz 7x', '18 Hz', 'LR'];
const BANDS = ['2.4 GHz', '915 MHz FCC', '868 MHz', '433 MHz', '70 cm HAM', '866 MHz IN', '915 MHz + 2.4 GHz', '868 MHz + 2.4 GHz'];
const CH = ['off', '5', '6', '7', '8', '9', '10', '11', '12', '13', '14', '15', '16'];

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
  rxConnected = true;

  constructor() {
    const p = (name: string): DemoParam => this.params.find((x) => x.name === name)!;
    const region = () => p('Region').value as number;
    const band = () => p('RF Band').value as number;
    this.params = [
      { name: 'Bind Phrase', kind: 'phrase', value: 'mlrs.0' },
      {
        name: 'Mode', kind: 'list', options: MODES, value: 1,
        allowed: () => (band() === 2 ? (region() === 1 ? [4, 6, 7] : [1, 2, 4, 5, 6, 7]) : [1, 2, 4, 5]),
      },
      { name: 'RF Band', kind: 'list', options: BANDS, value: 2, allowed: () => (region() === 1 ? [2] : [1, 2]) },
      { name: 'RF Ortho', kind: 'list', options: ['off', '1/3', '2/3', '3/3'], value: 0, allowed: () => (band() === 1 ? [0, 1, 2, 3] : []) },
      { name: 'Privacy', kind: 'list', options: ['off', 'lvl 1', 'lvl 2', 'lvl 3'], value: 0 },
      { name: 'Region', kind: 'list', options: ['free', 'EU'], value: 0 },
      { name: 'Tx Power', kind: 'list', options: ['25 mW', '50 mW', '100 mW', '250 mW', '500 mW'], value: 0 },
      { name: 'Tx Diversity', kind: 'list', options: ['enabled', 'antenna1', 'antenna2', 'r:en. t:ant1', 'r:en. t:ant2'], value: 1, allowed: () => [1] },
      { name: 'Tx Ch Source', kind: 'list', options: ['none', 'crsf', 'in'], value: 0, allowed: () => [0] },
      { name: 'Tx Ch Order', kind: 'list', options: ['AETR', 'TAER', 'ETAR'], value: 0 },
      { name: 'Tx In Mode', kind: 'list', options: ['sbus', 'sbus inv'], value: 0, allowed: () => [] },
      { name: 'Tx Ser Port', kind: 'list', options: ['serial', 'wbridge', 'serial2', 'com', 'crsfbridge'], value: 0, allowed: () => [0] },
      { name: 'Tx Ser Baudrate', kind: 'list', options: ['57600', '115200', '230400'], value: 0 },
      { name: 'Tx Ser Port2', kind: 'list', options: ['none', 'serial', 'wbridge', 'serial2', 'crsfbridge'], value: 0, allowed: () => [] },
      { name: 'Tx Ser Baudrate2', kind: 'list', options: ['57600', '115200', '230400'], value: 1, allowed: () => [] },
      { name: 'Tx Snd RadioStat', kind: 'list', options: ['off', '1 Hz'], value: 0 },
      { name: 'Tx Mav Component', kind: 'list', options: ['off', 'enabled'], value: 0 },
      { name: 'Tx Power Sw Ch', kind: 'list', options: CH, value: 0 },
      { name: 'Tx Buzzer', kind: 'list', options: ['off', 'LP', 'rxLQ'], value: 0, allowed: () => [] },
      { name: 'Rx Power', kind: 'list', options: ['10 mW', '25 mW', '50 mW', '100 mW', '250 mW', '500 mW'], value: 1 },
      { name: 'Rx Diversity', kind: 'list', options: ['enabled', 'antenna1', 'antenna2', 'r:en. t:ant1', 'r:en. t:ant2'], value: 1, allowed: () => [1] },
      { name: 'Rx Ch Order', kind: 'list', options: ['AETR', 'TAER', 'ETAR'], value: 0 },
      { name: 'Rx Out Mode', kind: 'list', options: ['sbus', 'crsf', 'sbus inv'], value: 0, allowed: () => [] },
      { name: 'Rx FailSafe Mode', kind: 'list', options: ['no sig', 'low thr', 'by cnf', 'low thr cnt', 'ch1ch4 cnt'], value: 0 },
      { name: 'Rx Ser Port', kind: 'list', options: ['serial', 'can', 'canfd'], value: 0, allowed: () => [0] },
      { name: 'Rx Ser Baudrate', kind: 'list', options: ['57600', '115200', '230400'], value: 0 },
      { name: 'Rx Ser Link Mode', kind: 'list', options: ['transp.', 'mavlink', 'mavlinkX', 'mspX'], value: 0 },
      { name: 'Rx Mav System ID', kind: 'list', options: ['51', '52', '53', '54', '55'], value: 0 },
      { name: 'Rx Snd RadioStat', kind: 'list', options: ['off', 'ardu_1', 'meth_b'], value: 0 },
      { name: 'Rx Snd RcChannel', kind: 'list', options: ['off', 'rc override', 'rc channels'], value: 0 },
      { name: 'Rx Out Rssi Ch', kind: 'list', options: CH, value: 0 },
      { name: 'Rx Out LQ Ch', kind: 'list', options: CH, value: 0 },
      { name: 'Rx Power Sw Ch', kind: 'list', options: CH, value: 0 },
      ...Array.from({ length: 12 }, (_, i): DemoParam => ({ name: `Rx FS Ch${i + 1}`, kind: 'value', value: 0, min: -120, max: 120, unit: '%' })),
      ...Array.from({ length: 4 }, (_, i): DemoParam => ({ name: `Rx FS Ch${i + 13}`, kind: 'list', options: ['-120 %', '0 %', '120 %'], value: 1 })),
    ];
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

  private fromHost(chunk: Uint8Array) {
    const now = performance.now();
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
    if (this.plus !== 3 || performance.now() - this.lastHostMs < GUARD_MS) return;
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

  // a few of cli.h's SiK answers (CommonTx/sik_at.h), enough to try the Terminal page
  private sikAt(cmd: string) {
    const regs = ['FORMAT=26', 'SERIAL_SPEED=57', 'AIR_SPEED=19', 'NETID=137', 'TXPOWER=14', 'ECC=0', 'MAVLINK=0', 'OPPRESEND=0',
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
      this.putsn('  Tx: DAKEFPV 900 MAX 1W TX, v1.4.05');
      this.putsn(this.rxConnected ? '  Rx: DAKEFPV 900 NANOPRO, v1.4.05' : '  Rx: receiver not connected');
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
    } else if (line === 'listfreqs') {
      [863.325, 865.333, 864.0, 867.667, 868.3].forEach((f, i) => this.putsn(`  ${i}: ${Math.round(f * 1000)} kHz`));
    } else {
      this.putsn('  invalid cmd');
    }
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
    this.putsn(line);
  }
}
