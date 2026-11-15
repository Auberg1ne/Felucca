#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prove that removing grid hints changes only y188..199; capture native frames."""
import argparse, os, shlex, struct, subprocess, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE='5bb8fcc97253c00e1d619b5851164bed95b269b6'
p=argparse.ArgumentParser();p.add_argument('--base-tree',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();a.out=a.out.resolve();a.base_tree=a.base_tree.resolve();a.out.mkdir(parents=True,exist_ok=True)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()==BASE
subprocess.run(['git','diff','--exit-code',BASE,'--','firmware','tests','web'],cwd=a.base_tree,check=True)
flags=['-O1','-g','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src']
sanitizers=['-fsanitize=address,undefined','-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow','-fno-sanitize-recover=undefined']
def build(src,exe,extra):
 with exe.with_suffix('.build.log').open('w') as log:subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+extra+[src,'-lm','-o',str(exe)],cwd=ROOT,stdout=log,stderr=log,check=True)
def png(ppm):
 header,rgb=ppm.read_bytes().split(b'\n255\n',1);assert header==b'P6\n240 240' and len(rgb)==172800
 def chunk(tag,data):return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
 scan=b''.join(b'\0'+rgb[y*720:(y+1)*720] for y in range(240))
 ppm.with_suffix('.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',240,240,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scan,9))+chunk(b'IEND',b''))
for name,tree in [('base',a.base_tree),('fix',ROOT)]:
 folder=a.out/name;folder.mkdir(exist_ok=True);exe=folder/'visuals'
 extra=sanitizers+[f'-DGRID_HINTS_WEB_SOURCE="{tree}/web/emu/felucca_web.c"']
 if name=='base':extra+=['-DGRID_HINTS_BASE_CAPTURE=1']
 build('web/emu/grid_hints_test.c',exe,extra)
 subprocess.run([str(exe),str(folder),str(folder/'frames.bin')],cwd=ROOT,check=True)
 for ppm in folder.glob('*.ppm'):png(ppm)
 exe=folder/'audio';build('tests/long_edit_test.c',exe,[f'-DEDIT_UI_SOURCE="{tree}/tests/ui_test.c"'])
 subprocess.run([str(exe),'--audio','0',str(folder/'audio.pcm')],cwd=ROOT,check=True)
left=(a.out/'base/frames.bin').read_bytes();right=(a.out/'fix/frames.bin').read_bytes()
assert len(left)==len(right) and len(left)%(240*240*2)==0
changed=0;frame_bytes=240*240*2
for n in range(len(left)//frame_bytes):
 l=left[n*frame_bytes:(n+1)*frame_bytes];r=right[n*frame_bytes:(n+1)*frame_bytes]
 if n<32:
  assert l[:188*480]==r[:188*480] and l[200*480:]==r[200*480:],f'outside hint region changed in frame {n}'
  assert not any(r[188*480:200*480]),f'hint region not black in frame {n}'
  assert l!=r,f'base capture did not contain hints in frame {n}'
  changed+=sum(l[i:i+2]!=r[i:i+2] for i in range(188*480,200*480,2))
 else:assert l==r,f'non-grid frame {n} changed'
print(f'Pixel comparison: {len(left)//frame_bytes} frames; all changes limited to hint region in 32 grid frames ({changed} pixels removed)',flush=True)
l=(a.out/'base/audio.pcm').read_bytes();r=(a.out/'fix/audio.pcm').read_bytes();assert l==r
print('Nonzero four-track audio: byte-identical',len(r),'bytes',flush=True)
