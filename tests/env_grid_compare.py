#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exact-base non-ENV pixel/audio comparison and reproducible native ENV captures."""
import argparse, os, shlex, struct, subprocess, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
BASE = '574ad82efbe1122e7d07e84faf7ce3ec1d1ca1fe'
p = argparse.ArgumentParser()
p.add_argument('--base-tree', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args(); a.out = a.out.resolve(); a.base_tree = a.base_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'], cwd=a.base_tree, text=True).strip() == BASE
subprocess.run(['git','diff','--exit-code',BASE,'--','firmware','tests','web'], cwd=a.base_tree, check=True)
a.out.mkdir(parents=True, exist_ok=True)
flags = ['-O1','-g','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src']
# Existing rendered-UI/DSP exclusions; no changes to repository sanitizer policy.
sanitize = ['-fsanitize=address,undefined','-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow','-fno-sanitize-recover=undefined']
def build(source, exe, extra):
    with exe.with_suffix('.build.log').open('w') as log:
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+extra+[source,'-lm','-o',str(exe)], cwd=ROOT, stdout=log, stderr=log, check=True)
def png(ppm):
    header, rgb = ppm.read_bytes().split(b'\n255\n', 1)
    assert header == b'P6\n240 240' and len(rgb) == 240*240*3
    def chunk(tag, data):
        return struct.pack('>I', len(data))+tag+data+struct.pack('>I', zlib.crc32(tag+data)&0xffffffff)
    scan = b''.join(b'\0'+rgb[y*720:(y+1)*720] for y in range(240))
    ppm.with_suffix('.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',240,240,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scan,9))+chunk(b'IEND',b''))
for name, tree in [('base', a.base_tree), ('fix', ROOT)]:
    folder = a.out/name; folder.mkdir(exist_ok=True)
    exe = folder/'visuals'
    extra = sanitize+[f'-DENV_WEB_SOURCE="{tree}/web/emu/felucca_web.c"']
    if name == 'base': extra += ['-DENV_BASE_CAPTURE=1']
    build('web/emu/env_grid_visuals_test.c',exe,extra)
    subprocess.run([str(exe),str(folder),str(folder/'non-env.bin')], cwd=ROOT, check=True)
    for ppm in folder.glob('*.ppm'): png(ppm)
    exe = folder/'audio'
    build('tests/long_edit_test.c',exe,[f'-DEDIT_UI_SOURCE="{tree}/tests/ui_test.c"'])
    subprocess.run([str(exe),'--audio','0',str(folder/'audio.pcm')], cwd=ROOT, check=True)
for filename in ['non-env.bin','audio.pcm']:
    left = (a.out/'base'/filename).read_bytes(); right = (a.out/'fix'/filename).read_bytes()
    assert left == right, filename+' differs from exact base'
    print(filename, ': byte-identical', len(right), 'bytes', flush=True)
