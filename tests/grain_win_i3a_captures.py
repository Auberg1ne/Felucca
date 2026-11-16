#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import subprocess,concurrent.futures
from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--base-tree',type=Path,required=True);p.add_argument('--out',type=Path,required=True);args=p.parse_args()
root=Path(__file__).resolve().parents[1];base=args.base_tree.resolve();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=base,text=True).strip()=='838540d20ac6a8b9d2356793c979c9e8bf95bbfa'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests','web'],cwd=base,check=True)

def run(tree):
 name='capture-new'if tree==root else'capture-i2';dest=out/name;dest.mkdir(exist_ok=True)
 with(out/(name+'.log')).open('w')as log:
  subprocess.run(['cc','-O1','-g','-w','-ffp-contract=off','-fsanitize=address,undefined','-fno-sanitize=shift,signed-integer-overflow,bounds,object-size,pointer-overflow','-fno-sanitize-recover=all','-Ibuild/gen','-Ifirmware/src',*(['-DWIN_CONTROLS_CURRENT=1']if tree==root else[]),f'-DWIN_CONTROLS_EMU_SOURCE="{tree}/web/emu/felucca_web.c"','web/emu/win_controls_test.c','-lm','-o',str(out/(name+"-test"))],cwd=root,stdout=log,stderr=log,check=True)
  subprocess.run([str(out/(name+"-test")),str(out/(name+'.bin')),str(dest)],cwd=root,stdout=log,stderr=log,check=True)
 print(name,'passed',flush=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:list(pool.map(run,[base,root]))
a=(out/'capture-i2.bin').read_bytes();b=(out/'capture-new.bin').read_bytes();stride=16+240*240*2;assert len(a)==len(b)==120*stride
import struct
changed=0
for off in range(0,len(a),stride):
 assert a[off:off+16]==b[off:off+16];ch,page,row,value=struct.unpack_from('4I',a,off)
 for i in range(240*240):
  pos=off+16+2*i
  if a[pos:pos+2]!=b[pos:pos+2]:
   y,x=divmod(i,240);assert page==1 and 62<=x<119 and 136<=y<186,(ch,page,row,value,x,y);changed+=1
print('120 before/after frames: changes confined to page1 WIN cell;',changed,'changed pixels; all page2 pixels identical',flush=True)
