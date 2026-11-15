#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exact-base unrelated-screen/audio comparison and native SEQ captures."""
import argparse, os, shlex, struct, subprocess, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
BASE = '3e0b8f4512bca6cd1f4b5c2d9c25c9e50813b619'
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
    extra = sanitize+[f'-DSEQ_WEB_SOURCE="{tree}/web/emu/felucca_web.c"']
    if name == 'base': extra += ['-DSEQ_BASE_CAPTURE=1']
    build('web/emu/seq_visuals_test.c',exe,extra)
    subprocess.run([str(exe),str(folder),str(folder/'non-env.bin')], cwd=ROOT, check=True)
    for ppm in folder.glob('*.ppm'): png(ppm)
    exe = folder/'audio'
    build('tests/long_edit_test.c',exe,[f'-DEDIT_UI_SOURCE="{tree}/tests/ui_test.c"'])
    subprocess.run([str(exe),'--audio','0',str(folder/'audio.pcm')], cwd=ROOT, check=True)
for name,tree in [('base',a.base_tree),('fix',ROOT)]:
    exe=a.out/name/'interactions'
    build('tests/seq_theme_test.c',exe,sanitize+[f'-DSEQ_UI_SOURCE="{tree}/tests/ui_test.c"'])
    subprocess.run([str(exe),str(a.out/name/'state.bin')],cwd=ROOT,check=True)
for filename in ['non-env.bin','audio.pcm','state.bin']:
    l=(a.out/'base'/filename).read_bytes();r=(a.out/'fix'/filename).read_bytes();assert l==r,filename+' differs'
    print(filename,': byte-identical',len(r),'bytes',flush=True)
l=(a.out/'base/targets.bin').read_bytes();r=(a.out/'fix/targets.bin').read_bytes();assert len(l)==len(r)
for n in range(len(l)//115200):
    before=l[n*115200:(n+1)*115200];after=r[n*115200:(n+1)*115200]
    assert before[:24*480]==after[:24*480] and before[202*480:]==after[202*480:],f'header/footer changed in SEQ frame {n}'
    assert before!=after,f'SEQ theme missing in frame {n}'
print('SEQ header/footer exact; intentional body changes only',flush=True)

l=(a.out/'base/state.bin.frames').read_bytes();r=(a.out/'fix/state.bin.frames').read_bytes();assert len(l)==len(r) and len(l)%115204==0
for n in range(len(l)//115204):
    p=n*115204;assert l[p:p+4]==r[p:p+4];before=l[p+4:p+115204];after=r[p+4:p+115204]
    if struct.unpack('<I',l[p:p+4])[0]:assert before[:24*480]==after[:24*480] and before[202*480:]==after[202*480:],f'SEQ trace header/footer changed at {n}'
    else:assert before==after,f'non-SEQ restored frame changed at {n}'
print('Interaction render trace: restored non-SEQ pixels and all headers/footers exact',len(l)//115204,'states',flush=True)
