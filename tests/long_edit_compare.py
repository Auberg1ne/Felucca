#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exact-base retained EDIT taps, other gestures and native UI snapshots."""
import argparse,subprocess,os,shlex
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE='b58797d2477e32b70d53f42c0918fc6bda64056a'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base-tree',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);a.base_tree=a.base_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()==BASE
subprocess.run(['git','diff','--exit-code',BASE,'--','firmware','tests','web'],cwd=a.base_tree,check=True)
for source,macro in [('long_edit_test','EDIT_UI_SOURCE'),('grain_only_navigation_test','GRID_UI_SOURCE')]:
 for name,tree in [('base',a.base_tree),('fix',ROOT)]:
  exe=a.out/(source+'-'+name)
  with (a.out/(exe.name+'-build.log')).open('w') as log:subprocess.run(shlex.split(os.environ.get('CC','cc'))+['-O1','-g','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src',f'-D{macro}="{tree}/tests/ui_test.c"','tests/'+source+'.c','-lm','-o',str(exe)],cwd=ROOT,stdout=log,stderr=log,check=True)
  if source=='long_edit_test' and name=='base':
   r=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True);(a.out/'base-expected-failure.log').write_text(r.stdout+r.stderr)
   assert r.returncode!=0 and 'long EDIT line' in r.stderr,'new hold test must reject the old menu behavior'
  subprocess.run([str(exe),str(exe)+'.bin'],cwd=ROOT,check=True)
 left=(a.out/(source+'-base.bin')).read_bytes();right=(a.out/(source+'-fix.bin')).read_bytes()
 assert left==right,source+' retained behavior/pixels changed'
 print(source,': byte-identical',len(right),'bytes',flush=True)

# Separate fresh processes avoid carrying DSP/FX/transport globals between controls.
exe=a.out/'long_edit_test-fix'
for held in [0,1]:subprocess.run([str(exe),'--audio',str(held),str(a.out/('audio-'+str(held)+'.pcm'))],cwd=ROOT,check=True)
control=(a.out/'audio-0.pcm').read_bytes();held=(a.out/'audio-1.pcm').read_bytes()
assert any(control) and control==held,'holding EDIT altered sounding playback'
print('Four-track sequencer held/control audio: byte-identical',len(held),'bytes')
