#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Allocator-driven real audio: zero-DTUNE/non-UNISON identity and measured rate changes."""
import argparse,concurrent.futures,hashlib,json,os,re,shlex,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE='1f0cbb4d24cc26b5a4929f9a33cf1e457b080f1b'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base-tree',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);a.base_tree=a.base_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()==BASE
subprocess.run(['git','diff','--exit-code',BASE,'--','firmware','tests','web'],cwd=a.base_tree,check=True)
def build(item):
 name,tree=item
 with (a.out/(name+'-build.log')).open('w') as log:subprocess.run(shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -w -ffp-contract=off'))+['-Ibuild/gen','-Ifirmware/src',f'-DDTUNE_UI_SOURCE="{tree}/tests/ui_test.c"','tests/grain_dtune_audio_test.c','-lm','-o',str(a.out/name)],cwd=ROOT,stdout=log,stderr=log,check=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(build,[('base',a.base_tree),('fix',ROOT)]))
records=[]
def run(name,args):
 pcm=a.out/(name+'.pcm');r=subprocess.run([str(a.out/name),*map(str,args),str(pcm)],cwd=ROOT,capture_output=True,text=True)
 with (a.out/(name+'-run.log')).open('a') as f:f.write(r.stdout+r.stderr)
 assert r.returncode==0,(name,args,r.stdout,r.stderr)
 data=pcm.read_bytes();assert any(data),'silent captures cannot validate identity'
 row=dict(args=args,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),trace=r.stdout.strip())
 for key in ['head-hash','step-hash','maxvoices']:row[key]=int(re.search(key+r'=(\d+)',r.stdout)[1])
 return row,data

def compare(args,identity=None,heads=False):
 fixed,new=run('fix',args);old,before=run('base',args);same=new==before
 if identity is not None:assert same==identity,('unexpected PCM equality/difference',args,fixed,old)
 if heads:assert fixed['head-hash']==old['head-hash'],('head movement changed',args)
 if identity is False:assert fixed['step-hash']!=old['step-hash'],('PCM changed without measured grain-step difference',args)
 fixed.update(identical=same,base_sha256=old['sha256'],base_step_hash=old['step-hash'],base_head_hash=old['head-hash']);records.append(fixed)

for kind in range(4):
 for source in [0,2,3,13,14]:
  for spread in [0,127]:
   for d in [0,1,40,64,127]:
    # Well below the rate cap: genuine three-voice pitch changes, not just PCM hashes.
    compare([kind,source,d,3,0,0,0,spread,'static',1],identity=d<=1,heads=True)
   # Four simultaneous tracks share the existing eight-voice budget.
   for ptch in [-24,0,24]:
    for rand in [0,127]:
     for extreme in [0,1]:
      compare([kind,source,0,3,ptch,rand,extreme,spread,'static',4],identity=True)
      compare([kind,source,127,3,ptch,rand,extreme,spread,'static',4]) # caps may converge
   compare([kind,source,127,3,24,0,0,spread,'capped',4],identity=True,heads=True)
   for mode in [0,1,2]:compare([kind,source,127,mode,0,127,0,spread,'static',4],identity=True,heads=True)
  for modev,d in [(3,0),(0,127)]:compare([kind,source,d,modev,0,127,0,127,'residual',4],identity=True,heads=True)
  compare([kind,source,0,3,0,0,0,127,'held',1],identity=True,heads=True)
  compare([kind,source,0,3,0,0,0,127,'retrigger',1],identity=False)
 print('personality',kind,': zero/non-UNISON identity, three-voice separation and four-track boundaries passed',flush=True)
(a.out/'comparisons.json').write_text(json.dumps(records,indent=2)+'\n')
print('DTUNE captures',len(records),'byte-identical',sum(x['identical'] for x in records),'intentional changes',sum(not x['identical'] for x in records))
