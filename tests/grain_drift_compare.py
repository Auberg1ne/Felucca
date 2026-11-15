#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exact-base CLOUD-neutral and other-personality identity; actual sounding drift matrix."""
import argparse, concurrent.futures, hashlib, json, os, re, shlex, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE='9a924c895ba257c38c34706d77d12dc93fac71b6'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base-tree',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);a.base_tree=a.base_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()==BASE
subprocess.run(['git','diff','--exit-code',BASE,'--','firmware','tests','web'],cwd=a.base_tree,check=True)
flags=shlex.split(os.environ.get('CFLAGS','-O1 -g -w -ffp-contract=off'))
def build(item):
 name,tree=item
 with (a.out/(name+'-build.log')).open('w') as log:
  subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+['-Ibuild/gen','-Ifirmware/src',f'-DDRIFT_UI_SOURCE="{tree}/tests/ui_test.c"','tests/grain_drift_audio_test.c','-lm','-o',str(a.out/name)],cwd=ROOT,stdout=log,stderr=log,check=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(build,[('base',a.base_tree),('fix',ROOT)]))
records=[]
def run(name,args):
 pcm=a.out/(name+'.pcm');r=subprocess.run([str(a.out/name),*map(str,args),str(pcm)],cwd=ROOT,capture_output=True,text=True)
 with (a.out/(name+'-run.log')).open('a') as f:f.write(r.stdout+r.stderr)
 assert r.returncode==0,(name,args,r.stdout,r.stderr)
 data=pcm.read_bytes();assert any(data),'silent audio cannot establish identity'
 row=dict(args=args,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),trace=r.stdout.strip())
 for key in ['head-first','head-last','head-changes','head-min','head-max','wraps','zone-frames']:
  row[key]=int(re.search(key+r'=(\d+)',r.stdout)[1])
 return row,data
values=[0,1,32,63,64,65,96,127];sources=[0,2,3,13,14];stretches=[0,1,64,127]
for kind in range(4):
 for source in sources:
  for stretch in stretches:
   for drift in values:
    args=[kind,source,drift,stretch,'static',4];fixed,new=run('fix',args);old,before=run('base',args)
    identity=kind!=0 or drift==64
    assert (new==before)==identity,('unexpected equality/difference',args,fixed,old)
    fixed['identical']=identity;fixed['base_sha256']=old['sha256'];records.append(fixed)
    if kind==0 and drift==0:assert fixed['head-first']==fixed['head-last'] and fixed['head-changes']==0
  if kind==0:
   for mode in ['pos','shape','drift','type']:
    fixed,_=run('fix',[kind,source,0,0,mode,4]);records.append(fixed)
    assert fixed['head-changes']>0,(mode,'live target/speed/type never moved head')
   for speed in [64,127]:
    fixed,_=run('fix',[kind,source,speed,0,'traverse',4]);records.append(fixed)
    assert fixed['wraps']>=2 and fixed['head-max']>=fixed['zone-frames']-65
 print('personality',kind,': factory/STRETCH/A matrix and isolation passed',flush=True)
(a.out/'comparisons.json').write_text(json.dumps(records,indent=2)+'\n')
print('DRIFT captures',len(records),'byte-identical',sum(x.get('identical',False) for x in records),'intentional CLOUD changes',sum(x.get('identical') is False for x in records))
