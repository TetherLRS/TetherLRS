#!/usr/bin/env python
'''
TetherLRS: writes tool/src/lib/firmware/elrs-products.json from ExpressLRS's targets.json
    python tools/tetherlrs_elrs_products.py <ExpressLRS targets clone>/targets.json
For each ExpressLRS product name (what an ExpressLRS image carries), the env suffix and the platform: its own hal
here when there is one, else its hardware layout as a slug ("Generic 2400 True Diversity PA" -> generic-2400-td-pa).
'''
import json
import os
import sys

SRC = sys.argv[1]
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tool', 'src', 'lib', 'firmware', 'elrs-products.json')


# ExpressLRS products with a hal of their own here: the env suffix, past the generic layout
OWN_HAL = {
    'BAYCKRC UR500 Dual Band RX': 'bayck-ur500',
    'BAYCKRC UR1000 Dual Band RX': 'bayck-ur1000',
    'DAKEFPV 900MHz MAX 1000mW Diversity RX': 'dakefpv-900-max-1w',
    'DAKEFPV 900MHz NANOPRO MAX 500mW RX': 'dakefpv-900-nanopro',
    'DAKEFPV Nano 2.4GHz RX': 'dakefpv-2400-nano',
    'RadioMaster BR3 Diversity 900Mz RX': 'radiomaster-br3-900',
    'SpeedyBee Nano 2.4GHz RX': 'speedybee-nano-2400',
    'BETAFPV SuperD 2.4GHz RX': 'betafpv-superd-2400',
    'RadioMaster XR1 Dual Band RX': 'radiomaster-xr1',
    'RadioMaster XR4 Dual Band True Diversity RX': 'radiomaster-xr4',
    'Flysky PR02 2.4GHz RX': 'flysky-pr02-2400-td-esp32s3',
    'RadioMaster RP4TD True Diversity 2.4GHz RX': 'radiomaster-rp4td-2400',
}


# product names found on real units that the targets repo does not list (vendors flash their own configurations)
SEEN_ONLY_ON_UNITS = {
    'DAKEFPV 900MHz S-RF-D-ANT MAX 1000mW RX': ['dakefpv-900-max-1w', 'esp32-c3'],  # ELRS 3.6.2 from the factory
}


def slug(layout):
    s = layout.rsplit('.', 1)[0].lower().replace('true diversity', 'td').replace(' ', '-')
    return s


# generic LR1121 builds that also come for one band: a board with one front end gets the build of its band, which
# starts there (the multi-band one starts at 868 MHz); ELRS's power_values are the 900 MHz ones, power_values_dual 2.4 GHz
BAND_SPLIT = ('generic-c3-lr1121', 'generic-lr1121-td')


def band_suffix(o):
    kind = 'TX' if o.get('firmware', '').endswith('_TX') else 'RX'
    with open(os.path.join(os.path.dirname(SRC), kind, o['layout_file']), encoding='utf-8') as f:
        hw = {**json.load(f), **o.get('overlay', {})}
    has900, has24 = hw.get('power_values') is not None, hw.get('power_values_dual') is not None
    return '-2400' if has24 and not has900 else '-900' if has900 and not has24 else ''


def walk(o, out):
    if isinstance(o, dict):
        if 'product_name' in o and 'layout_file' in o:
            name = o['product_name']
            env = OWN_HAL.get(name, slug(o['layout_file']))
            if env in BAND_SPLIT: env += band_suffix(o)
            out[name] = [env, o.get('platform', '')]
            return
        for v in o.values():
            walk(v, out)


with open(SRC, encoding='utf-8') as f:
    data = json.load(f)
products = {}
walk(data, products)
products.update(SEEN_ONLY_ON_UNITS)
with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
    rows = [f'{json.dumps(k)}:{json.dumps(v, separators=(",", ":"))}' for k, v in sorted(products.items())]
    f.write('{\n' + ',\n'.join(rows) + '\n}\n')  # one product a line
print(OUT, len(products), 'products')
