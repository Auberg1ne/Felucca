#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Strict exact-base comparisons: require PCM identity unless baseline F9 actually traps."""
import argparse,concurrent.futures,hashlib,json,os,shlex,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE='ba89712fdfe56cc1f56402865b0739024fbf29b5'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base-tree',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);a.base_tree=a.base_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()==BASE
subprocess.run(['git','diff','--exit-code',BASE,'--','firmware','tests','web'],cwd=a.base_tree,check=True)
flags=['-O1','-g','-w','-ffp-contract=off','-fsanitize=address,undefined','-fno-sanitize=shift','-fno-sanitize-recover=undefined','-Ibuild/gen','-Ifirmware/src']
def build(item):
 name,tree,src=item
 with (a.out/(name+'-build.log')).open('w') as log:subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+[f'-DFILTER_UI_SOURCE="{tree}/tests/ui_test.c"','tests/'+src+'.c','-lm','-o',str(a.out/name)],cwd=ROOT,stdout=log,stderr=log,check=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(build,[('base',a.base_tree,'grain_headroom_audio_test'),('fix',ROOT,'grain_headroom_audio_test'),('repro-base',a.base_tree,'grain_headroom_repro'),('repro-fix',ROOT,'grain_headroom_repro')]))
# Required before/after reproduction is checked, not excused as a baseline failure.
for name in ['repro-base','repro-fix']:
 r=subprocess.run([str(a.out/name)],cwd=ROOT,capture_output=True,text=True);(a.out/(name+'.log')).write_text(r.stdout+r.stderr)
 if name=='repro-base':assert r.returncode!=0 and 'signed integer overflow' in r.stderr and 'dsp.c:8:' in r.stderr,r
 else:assert r.returncode==0,r
records=[]
def compare(args):
 results={}
 for name in ['base','fix']:
  pcm=a.out/(name+'.pcm');r=subprocess.run([str(a.out/name),*map(str,args),str(pcm)],cwd=ROOT,capture_output=True,text=True)
  with (a.out/(name+'-run.log')).open('a') as f:f.write(str(args)+'\n'+r.stdout+r.stderr)
  if name=='base' and r.returncode:
   assert 'signed integer overflow' in r.stderr and 'dsp.c:8:' in r.stderr,('unexpected base failure',args,r.stderr)
   results[name]=dict(overflow=True,diagnostic=r.stderr.strip());continue
  assert r.returncode==0,(name,args,r.stdout,r.stderr)
  data=pcm.read_bytes();assert any(data),'silence cannot establish equivalence'
  results[name]=dict(overflow=False,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),trace=r.stdout.strip(),data=data)
 if not results['base']['overflow']:assert results['base']['data']==results['fix']['data'],('changed defined audio',args,results)
 records.append(dict(args=args,identical=not results['base']['overflow'],base={k:v for k,v in results['base'].items() if k!='data'},fix={k:v for k,v in results['fix'].items() if k!='data'}))
for kind in range(4):
 for source in [0,2,3,13,14,'user','sine']:
  for cut in [-50,-25,0,25,50]:
   for res in [0,127]:
    for tone in [0,64,127]:compare([kind,source,tone,cut,res,'static',4])
  compare([kind,source,127,0,0,'sweep',4])
  if isinstance(source,int):
   for cut in [-50,-25,0,25,50]:
    for res in [0,127]:compare([kind,source,127,cut,res,'unison',4])
    compare([kind,source,127,cut,127,'hot',4])
 print('personality',kind,': strict sounding source/tone/cutoff/resonance/unison/live matrix passed',flush=True)
# CUTOFF=0 controls must never be excused as resonant-overflow cases.
assert all(x['identical'] for x in records if x['args'][3]==0 and x['args'][5]!='sweep')
assert sum(x['identical'] for x in records)>=228, 'ordinary dry controls cannot be vacuous'
(a.out/'comparisons.json').write_text(json.dumps(records,indent=2)+'\n')
print('Headroom captures',len(records),'byte-identical',sum(x['identical'] for x in records),'base F9 overflows',sum(not x['identical'] for x in records))
