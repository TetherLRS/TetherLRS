#!/usr/bin/env python
'''
TetherLRS: packages the PlatformIO build of one environment for the release and the tool
    python tools/tetherlrs_package.py <env> <out dir> [<version>]
writes <out dir>/<env>/:
    firmware.bin   the application, for the ELRS web page and for the tool's update over a running TetherLRS
    full.bin       bootloader, partitions, boot_app0 and application in one image at 0x0, for the ROM bootloader
    info.json      env, role and device (from the image's marker), chip, version, offsets, sizes and sha256 of both
ESP8266 builds have no separate bootloader, there full.bin is firmware.bin.
'''
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

ENV = sys.argv[1]
OUT = os.path.join(sys.argv[2], ENV)
VERSION = sys.argv[3] if len(sys.argv) > 3 else ''

PIO_HOME = os.environ.get('PLATFORMIO_CORE_DIR', os.path.join(os.path.expanduser('~'), '.platformio'))
BUILD = os.path.join('.pio', 'build', ENV)
ESPTOOL = os.path.join(PIO_HOME, 'packages', 'tool-esptoolpy', 'esptool.py')
BOOT_APP0 = os.path.join(PIO_HOME, 'packages', 'framework-arduinoespressif32', 'tools', 'partitions', 'boot_app0.bin')

# chip id in the extended ESP image header of the bootloader (byte 12)
CHIPS = {0: ('esp32', 0x1000), 2: ('esp32s2', 0x1000), 5: ('esp32c3', 0x0), 9: ('esp32s3', 0x0), 13: ('esp32c6', 0x0)}


def sha256(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


os.makedirs(OUT, exist_ok=True)
firmware = os.path.join(BUILD, 'firmware.bin')
bootloader = os.path.join(BUILD, 'bootloader.bin')
shutil.copy(firmware, os.path.join(OUT, 'firmware.bin'))

info = {'env': ENV, 'version': VERSION}
# the device name, role and bands, as the marker in the image says them: the tool matches a running radio to its env
# by them and shows the bands with the board
with open(firmware, 'rb') as f:
    marker = re.search(rb'TetherLRS:(Tx|Rx):([^:\x00]*):(v\d+\.\d+(?:\.\d+)?(?:-RC\d+)?):(?:([^:\x00]*):)?', f.read())
if marker:
    info['role'] = marker.group(1).decode()
    info['device'] = marker.group(2).decode()
    if marker.group(4):
        info['bands'] = [b for b in marker.group(4).decode().split(',') if b]
if os.path.exists(bootloader):
    with open(bootloader, 'rb') as f:
        header = f.read(16)
    chip, bootloader_offset = CHIPS[header[12] | (header[13] << 8)]
    parts = [(bootloader_offset, bootloader), (0x8000, os.path.join(BUILD, 'partitions.bin')),
             (0xE000, BOOT_APP0), (0x10000, firmware)]
    try: # the CI installs esptool, a local PlatformIO has its own
        import esptool
        cmd = [sys.executable, '-m', 'esptool']
    except ImportError:
        cmd = [sys.executable, ESPTOOL]
    cmd += ['--chip', chip, 'merge_bin', '-o', os.path.join(OUT, 'full.bin')]
    for offset, path in parts:
        cmd += [hex(offset), path]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    info['app_offset'] = 0x10000
else:
    chip = 'esp8266'
    shutil.copy(firmware, os.path.join(OUT, 'full.bin'))
    info['app_offset'] = 0x0
info['chip'] = chip
for name in ('firmware.bin', 'full.bin'):
    path = os.path.join(OUT, name)
    info[name] = {'size': os.path.getsize(path), 'sha256': sha256(path)}

with open(os.path.join(OUT, 'info.json'), 'w') as f:
    json.dump(info, f, indent=1)
print(json.dumps(info))
