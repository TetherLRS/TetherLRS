#!/usr/bin/env python
'''
TetherLRS: generates a hal and a PlatformIO env for every ExpressLRS ESP receiver no hal here covers, and for the
external ExpressLRS Tx modules on ESP32
    python tools/tetherlrs_gen_elrs.py [<ExpressLRS targets clone>] [--check]
From each product its hardware layout with the product's overlay, keeping only what a data link uses (radio pins and
options, power tables, PA and antenna switches, serial, button, LED). Products with the same such hardware share one
build. A Tx module becomes a ground radio (TLRS_RX_AS_TX) with the data and the CLI on UART0, the port its USB-C
reaches; its screen, joystick, fan and power control are kept, its backpack is not. Writes:
    mLRS/Common/hal/esp/gen/{rx,tx}-hal-elrs-<name>.h  one per build
    mLRS/Common/hal/esp/gen/elrs-device_conf.h       DEVICE_NAME, radio, bands
    mLRS/Common/hal/esp/gen/elrs-hal.h               includes the hal of the build
    platformio-tetherlrs-elrs.ini                    the [env:rx-elrs-<name>] and [env:tx-elrs-mod-<name>] sections
    tools/elrs-gen.json                              product name -> env name, and what was left out and why
The power tables are ExpressLRS's: its levels (10 mW to 2 W) with the radio values of the layout, which the
manufacturers set. --check only says whether the files are up to date (for CI).
'''
import json
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
TARGETS = next((a for a in sys.argv[1:] if not a.startswith('--')), os.path.join(ROOT, 'elrs-targets'))
GEN = os.path.join(ROOT, 'mLRS', 'Common', 'hal', 'esp', 'gen')
INI = os.path.join(ROOT, 'platformio-tetherlrs-elrs.ini')
MAP = os.path.join(ROOT, 'tools', 'elrs-gen.json')

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# what a data link does not use: PWM, battery and voltage sense, VTX, gyro, I2C, IR, backpack; of misc_ only the FAN
IGNORED = re.compile(r'^(//|vbat|vsrc|pwm_outputs|i2c_|vtx_|gyro_|ir_|adc_|misc_|ledidx_rgb_vtx|ledidx_rgb_boot|'
                     r'spi_|serial1_|ant_group|use_backpack|debug_|backpack|screen_|joystick|five_way|fan_|power_pdet)')
# on a Tx module also the radio bay's serial pin, ELRS's unused power_high, the fan's speeds and tachometer
TX_IGNORED = re.compile(r'^(serial_|power_high|passthrough_baud|button2|button_led_index|thermal_|radio_dio2|misc_fan_speeds|misc_fan_tacho)')
FAN = ('misc_fan_en', 'misc_fan_pwm')
# handheld radios with the module inside, flashed over their own USB: not modules for a ground station
HANDHELDS = ('RadioMaster AX12 2.4/900 TX', 'HDZero Radio 2.4GHz TX')
LEVEL_MW = [10, 25, 50, 100, 250, 500, 1000, 2000]
LEVEL_DBM = [10, 14, 17, 20, 24, 27, 30, 33]
FAMILY = {
    'esp8285':  {'common': 'env_common_rx_esp82xx', 'board': 'esp8285', 'chip': 'ESP8285'},
    'esp32':    {'common': 'env_common_rx_esp32', 'board': 'pico32', 'chip': 'ESP32'},
    'esp32-c3': {'common': 'env_common_rx_esp32c3', 'board': 'esp32-c3-devkitm-1', 'chip': 'ESP32C3'},
    'esp32-s3': {'common': 'env_common_rx_esp32s3', 'board': 'esp32-s3-devkitc-1', 'chip': 'ESP32S3'},
}
TX_FAMILY = {'esp32': {'common': 'env_common_tx_esp32', 'board': 'esp32dev', 'chip': 'ESP32'}}
RADIO = {
    'SX1280': {'src': 'sx128x.cpp', 'has': 'DEVICE_HAS_SX128x', 'calc': 'sx128x_rfpower_calc', 'spi': '18000000L'},
    'SX127x': {'src': 'sx127x.cpp', 'has': 'DEVICE_HAS_SX127x', 'calc': 'sx1276_rfpower_calc', 'spi': '10000000L'},
    'LR1121': {'src': 'lr11xx.cpp', 'has': 'DEVICE_HAS_LR11xx', 'calc': 'lr11xx_rfpower_calc', 'spi': '16000000L'},
}


def define(name, value):
    return f'#define {name:<25} {value}'


def io(pin):
    return f'IO_P{pin}'


def radio_of(product, hw):
    fw = product.get('firmware', '')
    if 'LR1121' in fw or 'radio_rfsw_ctrl' in hw or 'power_values_dual' in hw:
        return 'LR1121'
    if '900' in fw:
        return 'SX127x'
    if '2400' in fw:
        return 'SX1280'
    return None


def hardware(product, kind):
    with open(os.path.join(TARGETS, kind.upper(), product['layout_file']), encoding='utf-8') as f:
        hw = json.load(f)
    hw.update(product.get('overlay', {}))

    def used(k):
        if kind == 'tx' and k.startswith(SCREEN_KEYS):
            return True
        return k in FAN or not (IGNORED.match(k) or kind == 'tx' and TX_IGNORED.match(k))
    hw = {k: v for k, v in hw.items() if v is not None and v != -1 and used(k)}  # -1: ELRS's no pin
    if kind == 'tx':
        if 'led' not in hw and 'led_rgb' not in hw and ('led_red' in hw or 'led_green' in hw):
            hw['led'] = hw.get('led_red', hw.get('led_green'))  # the red one is ELRS's status LED
            if 'led_red' in hw and hw.get('led_red_invert'):
                hw['led_invert'] = True
        for k in ('led_red', 'led_green', 'led_blue', 'led_red_invert'):
            hw.pop(k, None)
    return hw


def tx_module(d):
    # an external Tx module, flashed over its own USB: not inside a radio (EdgeTX passthrough, "Internal", handhelds)
    return 'etx' not in d.get('upload_methods', []) and 'Internal' not in d['product_name'] and d['product_name'] not in HANDHELDS


def why_not(product, hw, radio):
    if radio is None:
        return 'radio not recognised'
    pc = hw.get('power_control', 0)
    if pc == 3:  # ELRS's DAC on power_apc2, which only the ESP32 has
        if product['platform'] != 'esp32' or hw.get('power_apc2') is None:
            return 'power set by a DAC the chip or the layout does not have'
        if hw.get('power_values2') is None and (radio != 'SX127x' or hw.get('radio_rfo_hf')):
            return 'power set by a DAC without the radio power table'
        if hw.get('power_values2') is not None and len(hw['power_values2']) != len(hw.get('power_values') or []):
            return 'DAC and radio power tables of different length'
    elif pc != 0:
        return f'power set by power_control {pc}, not by the radio'
    if product['platform'] == 'esp8285' and (hw.get('radio_sck'), hw.get('radio_miso'), hw.get('radio_mosi')) not in ((14, 12, 13), (None, None, None)):
        return 'ESP8285 radio not on the HSPI pins'
    if hw.get('radio_nss') is None or hw.get('radio_rst') is None:  # GPIO 0 is a pin too
        return 'no radio chip select or reset'
    if radio != 'SX127x' and hw.get('radio_busy') is None:
        return 'no radio busy pin'
    if (hw.get('radio_dio1') if radio != 'SX127x' else dio0(hw, '')) is None:
        return 'no radio interrupt pin'
    if 'radio_nss_2' in hw and (hw.get('radio_busy_2') is None and radio != 'SX127x' or
                                (hw.get('radio_dio1_2') if radio != 'SX127x' else dio0(hw, '_2')) is None):
        return 'second radio without busy or interrupt pin'
    if radio == 'LR1121' and hw.get('power_values') is None and hw.get('power_values_dual') is None:
        return 'no power table'
    if radio != 'LR1121' and not hw.get('power_values'):
        return 'no power table'
    return None


def dio0(hw, sfx):
    # an SX127x interrupts on DIO0, some layouts only name DIO1
    return hw.get('radio_dio0' + sfx) if hw.get('radio_dio0' + sfx) is not None else hw.get('radio_dio1' + sfx)


def levels(hw, values):
    lo, hi = hw.get('power_min', 0), hw.get('power_max', 0)
    n = hi - lo + 1
    if len(values) == 1 and n > 1:
        hi, n = lo, 1
    out = []
    for i in range(min(n, len(values))):
        out.append((LEVEL_DBM[lo + i], LEVEL_MW[lo + i], values[i]))
    return out


def power_block(radio, hw):
    lines = ['//-- POWER', '']
    dac, sub = None, 'power_values'
    if hw.get('power_control', 0) == 3:  # ELRS: power_values to the PA's DAC, power_values2 to the radio
        dac, sub = {d: v for d, _, v in levels(hw, hw['power_values'])}, 'power_values2'
        lines += [define('SX_PA_DAC_IO', io(hw['power_apc2'])) + ' // the PA gain (ELRS power_apc2)', '']
    if radio == 'LR1121':
        lf = distinct(levels(hw, hw[sub]), dac) if hw.get(sub) is not None else []
        hf = distinct(levels(hw, hw['power_values_dual']), dac) if hw.get('power_values_dual') is not None else []
        lines += ['#include "../../../setup_types.h" // needed for frequency band condition in rfpower calc', '',
                  'void lr11xx_rfpower_calc(const int8_t power_dbm, int8_t* sx_power, int8_t* actual_power_dbm, const uint8_t frequency_band)', '{']
        if hf and lf:
            lines += ['    if (frequency_band == SX_FHSS_FREQUENCY_BAND_2P4_GHZ) {'] + calc_chain(hf, '        ', dac)
            lines += ['    } else {'] + calc_chain(lf, '        ', dac) + ['    }']
        else:  # one band only, the device conf allows no other
            lines += ['    (void)frequency_band;'] + calc_chain(hf or lf, '    ', dac)
        lines.append('}')
        table = sorted(set((d, m) for d, m, _ in hf + lf))
    else:
        if hw.get(sub) is not None:
            t = distinct(levels(hw, hw[sub]), dac)
        else:  # an SX127x behind a DAC with no radio table: ELRS leaves PA_BOOST at OutputPower 0
            t = distinct([(d, m, 0) for d, m, _ in levels(hw, hw['power_values'])], dac)
        lines += [f'void {RADIO[radio]["calc"]}(const int8_t power_dbm, int8_t* sx_power, int8_t* actual_power_dbm)', '{']
        lines += calc_chain(t, '    ', dac)
        lines.append('}')
        table = sorted(set((d, m) for d, m, _ in t))
    want = LEVEL_DBM[hw.get('power_default', 0)]
    default = max([i for i, (d, _) in enumerate(table) if d <= want] or [0])
    lines += ['', f'#define RFPOWER_DEFAULT           {default} // index into rfpower_list array', '',
              'const rfpower_t rfpower_list[] = {']
    lines += [f'    {{ .dbm = POWER_{d}_DBM, .mW = {m} }},' for d, m in table]
    lines += ['};']
    return lines


def distinct(table, dac=None):
    # a level whose radio value (and DAC value) repeats the one below gives no more power: the lower name stays
    out = []
    for d, m, v in table:
        if out and v == out[-1][2] and (dac is None or dac[d] == dac[out[-1][0]]):
            continue
        out.append((d, m, v))
    return out


def calc_chain(table, ind, dac=None):
    # the highest level at or below the request, the lowest below them all
    out = []
    rev = sorted(table, reverse=True)
    for i, (dbm, mw, v) in enumerate(rev):
        body = [f'{ind}    *sx_power = {v};', f'{ind}    *actual_power_dbm = {dbm};']
        if dac is not None:
            body.append(f'{ind}    dacWrite(SX_PA_DAC_IO, {dac[dbm]});')
        if i == len(rev) - 1:
            out += ([f'{ind}}} else {{'] if i else [f'{ind}{{']) + body + [f'{ind}}}']
        else:
            out += [f'{ind}{"} else " if i else ""}if (power_dbm >= POWER_{dbm}_DBM) {{'] + body
    return out


def radio_options(radio, hw, sx='SX'):
    o = []
    if hw.get('radio_dcdc'):
        o.append(f'#define {sx}_USE_REGULATOR_MODE_DCDC')
    if sx == 'SX' and radio == 'LR1121':
        if hw.get('radio_rfsw_ctrl'):
            o.append('#define SX_USE_RFSW_CTRL          {' + ', '.join(str(v) for v in hw['radio_rfsw_ctrl']) + '}')
        if hw.get('radio_rfo_hf'):
            o.append('#define SX_USE_LP_PA              // sub-GHz from the low power PA (ELRS radio_rfo_hf)')
    if sx == 'SX' and radio == 'SX127x' and hw.get('radio_rfo_hf'):
        o.append('#define SX_USE_RFO                // RFO pin, not PA_BOOST (ELRS radio_rfo_hf)')
    if sx == 'SX' and hw.get('power_lna_gain'):
        o.append(f'#define SX_LNA_GAIN_DB            {hw["power_lna_gain"]} // ELRS power_lna_gain, for the EU listen before talk thresholds')
    return o


def radio_block(plat, radio, hw, n):
    # n = '' for the first radio, '2' for the second of a true diversity pair
    sx, sfx = ('SX', '') if not n else ('SX2', '_2')
    esp8285 = plat == 'esp8285'
    dio = hw.get('radio_dio1' + sfx) if radio != 'SX127x' else dio0(hw, sfx)
    rst = hw.get('radio_rst' + sfx, hw.get('radio_rst'))  # a pair can share one reset line
    tx, rx = hw.get('power_txen' + sfx), hw.get('power_rxen' + sfx)
    L = [f'//-- {sx}: {radio}{" & SPI" if not n else ""}', '']
    if not n:
        L.append(f'#define SPI_CS_IO                 {io(hw["radio_nss"])}')
        if not esp8285:
            L += [f'#define SPI_MISO                  {io(hw["radio_miso"])}', f'#define SPI_MOSI                  {io(hw["radio_mosi"])}',
                  f'#define SPI_SCK                   {io(hw["radio_sck"])}']
        L.append(f'#define SPI_FREQUENCY             {RADIO[radio]["spi"] if not (esp8285 and radio == "SX1280") else "16000000L"}')
    else:
        L.append(f'#define SX2_CS_IO                 {io(hw["radio_nss_2"])}')
    L += [define(f'{sx}_RESET', io(rst)) + ('' if rst != hw.get('radio_rst') or not n else ' // shared with the first radio'),
          define(f'{sx}_DIO', io(dio))]
    if radio != 'SX127x':
        L.append(define(f'{sx}_BUSY', io(hw['radio_busy' + sfx])))
    if tx is not None:
        L.append(define(f'{sx}_TX_EN', io(tx)))
    if rx is not None:
        L.append(define(f'{sx}_RX_EN', io(rx)))
    pa_en = hw.get('power_enable') if not n else None
    if pa_en is not None:
        L.append(define('SX_PA_EN', io(pa_en)) + ' // ELRS power_enable: high while sending or receiving')
    L += radio_options(radio, hw, sx)
    ant = not n and hw.get('ant_ctrl') is not None and 'radio_nss_2' not in hw
    if ant:
        L += ['', '// antenna switch, driven by Common/antenna_switch.h (ELRS single radio diversity); antenna 0 is low',
              '#define DEVICE_HAS_ANTENNA_SWITCH', f'#define SX_ANT_CTRL               {io(hw["ant_ctrl"])}']
    low = sx.lower()
    L += ['', f'IRQHANDLER(void {sx}_DIO_EXTI_IRQHandler(void);)', '', f'void {low}_init_gpio(void)', '{']
    if n:
        L.append('    gpio_init(SX2_CS_IO, IO_MODE_OUTPUT_PP_HIGH);')
    L.append(f'    gpio_init({sx}_RESET, IO_MODE_OUTPUT_PP_{"HIGH" if esp8285 else "LOW"});')
    L.append(f'    gpio_init({sx}_DIO, IO_MODE_INPUT_{"PU" if radio == "SX127x" else "ANALOG"});')
    if radio != 'SX127x':
        L.append(f'    gpio_init({sx}_BUSY, IO_MODE_INPUT_{"PU" if radio == "SX1280" else "ANALOG"});')
    if tx is not None:
        L.append(f'    gpio_init({sx}_TX_EN, IO_MODE_OUTPUT_PP_LOW);')
    if rx is not None:
        L.append(f'    gpio_init({sx}_RX_EN, IO_MODE_OUTPUT_PP_LOW);')
    if pa_en is not None:
        L.append('    gpio_init(SX_PA_EN, IO_MODE_OUTPUT_PP_HIGH);')
    if ant:
        L.append('    gpio_init(SX_ANT_CTRL, IO_MODE_OUTPUT_PP_LOW);')
    L += ['}', '']
    if n:
        L += ['IRAM_ATTR void spib_select(void) { gpio_low(SX2_CS_IO); }', 'IRAM_ATTR void spib_deselect(void) { gpio_high(SX2_CS_IO); }', '']
    if radio != 'SX127x':
        L += [f'IRAM_ATTR bool {low}_busy_read(void) {{ return (gpio_read_activehigh({sx}_BUSY)) ? true : false; }}', '']
    if ant:
        L += ['IRAM_ATTR void sx_antenna_switch_set(uint8_t antenna) { if (antenna) { gpio_high(SX_ANT_CTRL); } else { gpio_low(SX_ANT_CTRL); } }', '']
    for what, on, off in (('transmit', tx, rx), ('receive', rx, tx)):
        body = []
        if off is not None:
            body.append(f'    gpio_low({sx}_{"RX" if what == "transmit" else "TX"}_EN);')
        if on is not None:
            body.append(f'    gpio_high({sx}_{"TX" if what == "transmit" else "RX"}_EN);')
        L += ([f'IRAM_ATTR void {low}_amp_{what}(void)', '{'] + body + ['}']) if body else [f'IRAM_ATTR void {low}_amp_{what}(void) {{}}']
    L += ['', f'void {low}_dio_init_exti_isroff(void) {{{"" if esp8285 else f" detachInterrupt({sx}_DIO); "}}}',
          f'void {low}_dio_enable_exti_isr(void) {{ attachInterrupt({sx}_DIO, {sx}_DIO_EXTI_IRQHandler, RISING); }}',
          f'IRAM_ATTR void {low}_dio_exti_isr_clearflag(void) {{}}', '', '']
    return L


SCREEN_KEYS = ('screen_', 'joystick', 'five_way')
SCREEN_I2C, SCREEN_SPI, SCREEN_TFT = 1, 2, 4  # ExpressLRS screen_type: SSD1306 on I2C, SSD1306 on SPI, ST7735 80x160


def screen(hw):
    # the screen a Tx module has and its pins are complete for, else 0
    t = hw.get('screen_type', 0)
    if t == SCREEN_I2C and hw.get('screen_sck') is not None and hw.get('screen_sda') is not None:
        return t
    if t in (SCREEN_SPI, SCREEN_TFT) and all(hw.get(k) is not None for k in ('screen_sck', 'screen_mosi', 'screen_dc')):
        return t
    return 0


def fiveway(hw):
    # 'adc', 'digital' or None, as ExpressLRS reads them
    if hw.get('joystick') is not None and len(hw.get('joystick_values') or []) == 6:
        return 'adc'
    if all(hw.get(k) is not None for k in ('five_way1', 'five_way2', 'five_way3')):
        return 'digital'
    return None


def display_defines(hw):
    t = screen(hw)
    if not t:
        return []
    rot = bool(hw.get('screen_reversed'))
    if t == SCREEN_I2C:
        d = ['#define DEVICE_HAS_I2C_DISPLAY_ROT180' if rot else '#define DEVICE_HAS_I2C_DISPLAY']
    else:
        d = ['#define DEVICE_HAS_TFT_DISPLAY' if t == SCREEN_TFT else '#define DEVICE_HAS_SPI_DISPLAY']
        if rot:
            d.append('#define DEVICE_HAS_DISPLAY_ROT180')
    if not fiveway(hw):
        d.append('#define DEVICE_HAS_DISPLAY_NO_KEYS')
    return d


def display_block(hw):
    t = screen(hw)
    if not t:
        return []
    L = ['', '', '//-- Display', '']
    if t == SCREEN_I2C:
        L += [define('I2C_SDA_IO', io(hw['screen_sda'])), define('I2C_SCL_IO', io(hw['screen_sck'])),
              define('I2C_CLOCKSPEED', '1000000L') + ' // as mLRS on the Ranger']
    else:
        L += [define('DISP_SPI_SCK_IO', io(hw['screen_sck'])), define('DISP_SPI_MOSI_IO', io(hw['screen_mosi'])),
              define('DISP_DC_IO', io(hw['screen_dc']))]
        for key, name in (('screen_cs', 'DISP_CS_IO'), ('screen_rst', 'DISP_RST_IO'), ('screen_bl', 'DISP_BL_IO')):
            if hw.get(key) is not None:
                L.append(define(name, io(hw[key])))
    rst = hw.get('screen_rst') if t == SCREEN_I2C else None  # the SPI driver resets its display itself
    if rst is not None:
        L.append(define('DISP_RST_IO', io(rst)))
    L += ['', '', '//-- 5 Way Switch', '']
    init = ['void fiveway_init(void)', '{']
    if rst is not None:
        init += ['    gpio_init(DISP_RST_IO, IO_MODE_OUTPUT_PP_LOW); // the OLED out of reset before the display starts',
                 '    delay(10);', '    gpio_high(DISP_RST_IO);']
    kind = fiveway(hw)
    if kind == 'adc':
        # ExpressLRS joystick_values: up, down, left, right, centre, idle; a key is the value within half the distance
        # to its nearest neighbour
        v = hw['joystick_values']
        fuzz = [min(abs(a - b) for j, b in enumerate(v) if j != i) // 2 for i, a in enumerate(v)]
        L += [define('FIVEWAY_ADC_IO', io(hw['joystick'])), ''] + init + ['}', '', 'IRAM_ATTR uint8_t fiveway_read(void)', '{',
              '    int16_t adc = analogRead(FIVEWAY_ADC_IO); // as ExpressLRS: the value within half the distance to the next']
        for i, key in enumerate(('KEY_UP', 'KEY_DOWN', 'KEY_LEFT', 'KEY_RIGHT', 'KEY_CENTER')):
            L.append(f'    if (adc > {v[i] - fuzz[i]} && adc < {v[i] + fuzz[i]}) return (1 << {key});')
        L += ['    return 0;', '}']
    elif kind == 'digital':
        L += [define('FIVEWAY_IN1_IO', io(hw['five_way1'])), define('FIVEWAY_IN2_IO', io(hw['five_way2'])),
              define('FIVEWAY_IN3_IO', io(hw['five_way3'])), ''] + init + [
              '    gpio_init(FIVEWAY_IN1_IO, IO_MODE_INPUT_PU);', '    gpio_init(FIVEWAY_IN2_IO, IO_MODE_INPUT_PU);',
              '    gpio_init(FIVEWAY_IN3_IO, IO_MODE_INPUT_PU);', '}', '', 'IRAM_ATTR uint8_t fiveway_read(void)', '{',
              '    // as ExpressLRS: the three inputs make a number, 7 is no key',
              '    uint8_t v = (gpio_read_activehigh(FIVEWAY_IN1_IO) ? 4 : 0) | (gpio_read_activehigh(FIVEWAY_IN2_IO) ? 2 : 0) |',
              '                (gpio_read_activehigh(FIVEWAY_IN3_IO) ? 1 : 0);',
              '    switch (v) {', '        case 2: return (1 << KEY_LEFT);', '        case 3: return (1 << KEY_UP);',
              '        case 4: return (1 << KEY_RIGHT);', '        case 5: return (1 << KEY_DOWN);',
              '        case 6: return (1 << KEY_CENTER);', '    }', '    return 0;', '}']
    else:
        L += init + ['}', '', 'IRAM_ATTR uint8_t fiveway_read(void) { return 0; } // no keys: the main pages take turns']
    return L


def hal(name, kind, plat, radio, hw, products, layout, overlay):
    td = 'radio_nss_2' in hw
    rgb = hw.get('led_rgb') is not None
    L = ['//*******************************************************',
         '// TetherLRS, generated by tools/tetherlrs_gen_elrs.py from ExpressLRS/targets: do not edit',
         '// GPL3, https://www.gnu.org/licenses/gpl-3.0.de.html',
         '//*******************************************************',
         f'// {FAMILY[plat]["chip"]}, {radio}{" true diversity" if td else ""}: ExpressLRS {"Tx module" if kind == "tx" else "receiver"} layout "{layout}"' + (', with the products\' overlay' if overlay else ''),
         '// ' + '; '.join(products)[:600],
         '//-------------------------------------------------------', '',
         '#define DEVICE_HAS_SINGLE_LED_RGB' if rgb else '#define DEVICE_HAS_SINGLE_LED']
    fan = hw.get('misc_fan_en', hw.get('misc_fan_pwm'))
    if fan is not None:
        L.append('#define DEVICE_HAS_FAN_ONOFF')
    L += display_defines(hw)
    if td:
        L.append('#define DEVICE_HAS_DIVERSITY_SINGLE_SPI // must be set, doesn\'t work without it')
    L += ['#define DEVICE_HAS_NO_DEBUG', '', '', '//-- UARTS', '// UARTB = serial port', '// UARTF = debug port', '',
          '#define UARTB_USE_SERIAL', '#define UARTB_BAUD                RX_SERIAL_BAUDRATE']
    if kind == 'tx':  # ESP32 UART0, wired to the module's USB-C
        L += [define('UARTB_USE_TX_IO', 'IO_P1'), define('UARTB_USE_RX_IO', 'IO_P3')]
    elif plat != 'esp8285' and hw.get('serial_tx') is not None and (plat != 'esp32' or (hw['serial_tx'], hw['serial_rx']) != (1, 3)):
        L += [f'#define UARTB_USE_TX_IO           {io(hw["serial_tx"])}', f'#define UARTB_USE_RX_IO           {io(hw["serial_rx"])}']
    L += ['#define UARTB_TXBUFSIZE           RX_SERIAL_TXBUFSIZE', '#define UARTB_RXBUFSIZE           RX_SERIAL_RXBUFSIZE', '',
          '#define UARTF_USE_SERIAL', '#define UARTF_BAUD                115200', '', '']
    L += radio_block(plat, radio, hw, '')
    if td:
        L += radio_block(plat, radio, hw, '2')
    L += ['//-- Button', '']
    if hw.get('button') is not None:
        L += [f'#define BUTTON                    {io(hw["button"])}', '', 'void button_init(void)', '{',
              '    gpio_init(BUTTON, IO_MODE_INPUT_PU);', '}', '', 'IRAM_ATTR bool button_pressed(void)', '{',
              '    return gpio_read_activelow(BUTTON) ? true : false;', '}']
    else:
        L += ['void button_init(void) {} // none on this board', '', 'IRAM_ATTR bool button_pressed(void) { return false; }']
    L += ['', '', '//-- LEDs', '']
    if rgb:
        idx = hw.get('ledidx_rgb_status') or [0]
        L += [f'#define LED_RGB                   {io(hw["led_rgb"])}', f'#define LED_RGB_PIXEL_NUM         {max(idx) + 1}']
        if not hw.get('led_rgb_isgrb'):
            L.append('#define LED_RGB_ORDER_RGB         // ELRS without led_rgb_isgrb: red first')
        L.append('#include "../esp-hal-led-rgb.h"')
    elif hw.get('led') is None:
        L += ['void leds_init(void) {} // no LED on this board', '', 'IRAM_ATTR void led_red_off(void) {}',
              'IRAM_ATTR void led_red_on(void) {}', 'IRAM_ATTR void led_red_toggle(void) {}']
    else:
        L += [f'#define LED_RED                   {io(hw["led"])}', '', 'void leds_init(void)', '{',
              f'    gpio_init(LED_RED, IO_MODE_OUTPUT_PP_{"HIGH" if hw.get("led_invert") else "LOW"});', '}', '',
              f'IRAM_ATTR void led_red_off(void) {{ gpio_{"high" if hw.get("led_invert") else "low"}(LED_RED); }}',
              f'IRAM_ATTR void led_red_on(void) {{ gpio_{"low" if hw.get("led_invert") else "high"}(LED_RED); }}',
              'IRAM_ATTR void led_red_toggle(void) { gpio_toggle(LED_RED); }']
    L += display_block(hw)
    if fan is not None:
        L += ['', '', '//-- Cooling Fan', '', define('FAN_IO', io(fan)), '', 'void fan_init(void)', '{',
              '    gpio_init(FAN_IO, IO_MODE_OUTPUT_PP_LOW);', '}', '',
              'IRAM_ATTR void fan_set_power(int8_t power_dbm) // on from 250 mW, ExpressLRS\'s default', '{',
              '    if (power_dbm >= POWER_24_DBM) { gpio_high(FAN_IO); } else { gpio_low(FAN_IO); }', '}']
    L += ['', ''] + power_block(radio, hw)
    return '\n'.join(L) + '\n'


def bands(radio, hw):
    if radio == 'SX1280':
        return ['FREQUENCY_BAND_2P4_GHZ']
    if radio == 'SX127x':
        return ['FREQUENCY_BAND_868_MHZ', 'FREQUENCY_BAND_915_MHZ_FCC']
    b = []
    if hw.get('power_values_dual') is not None:
        b.append('FREQUENCY_BAND_2P4_GHZ')
    if hw.get('power_values') is not None:
        b += ['FREQUENCY_BAND_868_MHZ', 'FREQUENCY_BAND_915_MHZ_FCC']
    return b


def short(layout):
    s = layout.rsplit('.', 1)[0]
    s = re.sub(r'^Generic ', '', s).replace('True Diversity', 'TD').replace('Diversity', 'Div').replace(' and ', ' ')
    return s


def generate(covered):
    with open(os.path.join(TARGETS, 'targets.json'), encoding='utf-8') as f:
        targets = json.load(f)
    configs, left_out = {}, {}
    for vendor, cats in targets.items():
        if not isinstance(cats, dict):
            continue
        for cat, devs in cats.items():
            kind = cat[:2]
            if kind not in ('rx', 'tx') or not isinstance(devs, dict):
                continue
            for d in devs.values():
                if not isinstance(d, dict) or d.get('platform') not in FAMILY or covered(d, kind):
                    continue
                if kind == 'tx' and not tx_module(d):
                    continue
                if kind == 'tx' and d['platform'] not in TX_FAMILY:
                    left_out[d['product_name']] = f'Tx module on {FAMILY[d["platform"]]["chip"]}, not built yet'
                    continue
                hw = hardware(d, kind)
                radio = radio_of(d, hw)
                why = why_not(d, hw, radio)
                if why:
                    left_out[d['product_name']] = why
                    continue
                key = json.dumps([kind, d['platform'], radio, d['layout_file'], hw], sort_keys=True)
                c = configs.setdefault(key, {'kind': kind, 'plat': d['platform'], 'radio': radio, 'hw': hw, 'layout': d['layout_file'],
                                             'overlay': bool(d.get('overlay')), 'products': []})
                c['products'].append(d['product_name'])
    # names: the layout, and where one layout has several builds the top power or a number
    by_layout = {}
    for c in configs.values():
        by_layout.setdefault((c['kind'], c['plat'], c['layout']), []).append(c)
    for group in by_layout.values():
        group.sort(key=lambda c: sorted(c['products'])[0])
        tops = [max(m for _, m in power_levels(c)) for c in group]
        for i, c in enumerate(group):
            base = short(c['layout'])
            tag = '' if len(group) == 1 else (f' {tops[i]}mW' if tops.count(tops[i]) == 1 else f' v{i + 1}')
            c['title'] = base + tag
            c['env'] = ('elrs-mod-' if c['kind'] == 'tx' else 'elrs-') + re.sub(r'[^a-z0-9]+', '-', (base + tag).lower()).strip('-')
    configs = sorted(configs.values(), key=lambda c: c['env'])
    name_all(configs)
    return configs, left_out


def power_levels(c):
    hw = c['hw']
    if c['radio'] == 'LR1121':
        return [(d, m) for key in ('power_values', 'power_values_dual') if hw.get(key) is not None for d, m, _ in levels(hw, hw[key])]
    return [(d, m) for d, m, _ in levels(hw, hw['power_values'])]


# DEVICE_NAME holds 20 characters and tells the tool which build a TetherLRS radio runs: unique per role
SHORT_NAMES = {
    'DIY S3 DevKit 2400 TD PWM 16': 'DIY S3 TD PWM16',
    'DIY S3 DevKit 2400 TD VTX PWM 11': 'DIY S3 TD VTX PWM11',
    'GEPRC DualBand Linkflow 1000mW': 'GEPRC LINKFLOW DB',
    'GEPRC DualBand Linkflow 2000mW': 'GEPRC LINKFLOW 900V2',  # the dual band layout with one band
    'Namimno FLASH 2400 PA': 'NAMIMNO FLASH 2.4 PA',
    'SPRacing RXG1 Gyro 2400 RX GYRO': 'SPRACING RXG1 2400',
    'SPRacing RXN1 Gyro 2400 RX GYRO IR D4': 'SPRACING RXN1 2400',
}


def name20(title):
    short = {k.upper(): v for k, v in SHORT_NAMES.items()}
    return short.get(title.upper(), title).upper().replace('LR1121', 'LR11').replace('RADIOMASTER ', 'RM ')


def hand_names():
    # the DEVICE_NAMEs of the ESP hals written by hand that an env of platformio.ini builds
    with open(os.path.join(ROOT, 'platformio.ini'), encoding='utf-8') as f:
        built = set(re.findall(r'^\s*-D\s+(\w+)', f.read(), re.M))
    with open(os.path.join(ROOT, 'mLRS', 'Common', 'hal', 'esp', 'esp-device_conf.h'), encoding='utf-8') as f:
        blocks = re.findall(r'#ifdef (\w+)\s+#define DEVICE_NAME\s+"([^"]+)"', f.read())
    return {n.upper() for d, n in blocks if d in built}


def name_all(configs):
    # a Tx module named like a receiver gets MOD: every receiver is also built as Tx
    rx = {name20(c['title']) for c in configs if c['kind'] == 'rx'}
    for c in configs:
        c['name'] = name20(c['title'])
        if c['kind'] == 'tx' and c['name'] in rx:
            c['name'] = c['name'][:16].rstrip() + ' MOD'
    hand = hand_names()
    for role in ('rx', 'tx'):
        seen = {}
        for c in configs:
            if role == 'tx' or c['kind'] == 'rx':
                n = c['name']
                if len(n) > 20 or n in hand or n in seen:
                    sys.exit(f'DEVICE_NAME "{n}" of {c["env"]}: too long or not unique, add a name to SHORT_NAMES')
                seen[n] = c


def files(configs, left_out):
    out = {}
    conf = ['// TetherLRS, generated by tools/tetherlrs_gen_elrs.py from ExpressLRS/targets: do not edit', '']
    halh = ['// TetherLRS, generated by tools/tetherlrs_gen_elrs.py from ExpressLRS/targets: do not edit', '']
    ini = ['; generated by tools/tetherlrs_gen_elrs.py from ExpressLRS/targets, do not edit',
           '; ExpressLRS receivers no hal of mLRS covers, one build per hardware (layout with the overlay that matters),',
           '; and the external Tx modules as ground radios (tx-elrs-mod-*)', '']
    mapping = {'products': {}, 'left_out': dict(sorted(left_out.items()))}
    for c in configs:
        kind = c['kind']
        define = f'{kind.upper()}_ELRS_GEN_' + c['env'][len('elrs-'):].upper().replace('-', '_') + '_' + FAMILY[c['plat']]['chip']
        fname = f'{kind}-hal-{c["env"]}.h'
        out[os.path.join(GEN, fname)] = hal(c['env'], kind, c['plat'], c['radio'], c['hw'], sorted(c['products']), c['layout'], c['overlay'])
        conf += [f'#ifdef {define}', f'  #define DEVICE_NAME "{c["name"]}"',
                 '  #define DEVICE_IS_RECEIVER' + (' // built as Tx only, TLRS_RX_AS_TX' if kind == 'tx' else ''),
                 f'  #define {RADIO[c["radio"]]["has"]}'] + [f'  #define {b}' for b in bands(c['radio'], c['hw'])] + ['#endif', '']
        halh += [f'#ifdef {define}', f'#include "{fname}"', '#endif', '']
        fam = TX_FAMILY[c['plat']] if kind == 'tx' else FAMILY[c['plat']]
        ini += [f'[env:{kind}-{c["env"]}]', f'extends = {fam["common"]}', f'board = {fam["board"]}']
        shows = kind == 'tx' and screen(c['hw'])
        if shows and any(c['hw'].get(k) in range(6, 12) for k in c['hw'] if k.startswith('screen_') and k != 'screen_type'):
            ini.append('board_build.flash_mode = dio  ; as ExpressLRS: the screen is on flash pins QIO would take')
        if c['hw'].get('led_rgb') is not None:
            ini += ['lib_deps =', '  makuna/NeoPixelBus@^2.8.3']
        ini += ['build_src_filter =', f'  ${{{fam["common"]}.build_src_filter}}', f'  +<modules/sx12xx-lib/src/{RADIO[c["radio"]]["src"]}>']
        if shows:
            ini.append('  +<Common/thirdparty/gdisp.c>')
        ini += ['build_flags =', f'  ${{{fam["common"]}.build_flags}}', f'  -D {define}'] + (['  -D TLRS_RX_AS_TX'] if kind == 'tx' else []) + ['']
        for p in c['products']:
            mapping['products'][p] = [c['env'], c['plat']]
    out[os.path.join(GEN, 'elrs-device_conf.h')] = '\n'.join(conf)
    out[os.path.join(GEN, 'elrs-hal.h')] = '\n'.join(halh)
    out[INI] = '\n'.join(ini)
    mapping['products'] = dict(sorted(mapping['products'].items()))
    out[MAP] = json.dumps(mapping, indent=1) + '\n'
    return out


def covered_by_hand():
    # what tools/tetherlrs_elrs_products.py maps to a hal written by hand (an env of platformio.ini); a Tx module only
    # by its own hal
    import tetherlrs_elrs_products as p
    with open(os.path.join(ROOT, 'platformio.ini'), encoding='utf-8') as f:
        envs = set(re.findall(r'^\[env:((?:rx|tx)-[^\]]+)\]', f.read(), re.M))

    def covered(d, kind):
        if kind == 'tx':
            return f'tx-{p.OWN_HAL.get(d["product_name"])}' in envs
        env = p.OWN_HAL.get(d['product_name'], p.slug(d['layout_file']))
        if env in p.BAND_SPLIT:
            env += p.band_suffix(d, TARGETS)
        return f'rx-{env}' in envs
    return covered


if __name__ == '__main__':
    configs, left_out = generate(covered_by_hand())
    out = files(configs, left_out)
    if '--check' in sys.argv:
        stale = [p for p, t in out.items() if not os.path.exists(p) or open(p, encoding='utf-8').read() != t]
        stale += [os.path.join(GEN, f) for f in os.listdir(GEN) if os.path.join(GEN, f) not in out] if os.path.isdir(GEN) else []
        if stale:
            print('out of date:', *stale, 'run python tools/tetherlrs_gen_elrs.py', sep='\n  ')
            sys.exit(1)
        print('generated ExpressLRS builds up to date')
        sys.exit(0)
    os.makedirs(GEN, exist_ok=True)
    for f in os.listdir(GEN):
        if os.path.join(GEN, f) not in out:
            os.remove(os.path.join(GEN, f))
    for p, t in out.items():
        with open(p, 'w', encoding='utf-8', newline='\n') as f:
            f.write(t)
    n = sum(len(c['products']) for c in configs)
    print(f'{len(configs)} builds for {n} products, {len(left_out)} left out')
    for name, why in sorted(left_out.items()):
        print(f'  left out: {name}: {why}')
