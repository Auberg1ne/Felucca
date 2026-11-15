#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exact-base bypass identity and actual final-output TONE/HF regression matrix."""
import argparse, concurrent.futures, hashlib, json, os, re, shlex, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE='00f21a456e0dce4029a3a57bbd1481c2f26956b1'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base-tree',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);a.base_tree=a.base_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()==BASE
subprocess.run(['git','diff','--exit-code',BASE,'--','firmware','tests','web'],cwd=a.base_tree,check=True)
flags=shlex.split(os.environ.get('CFLAGS','-O1 -g -w -ffp-contract=off'))
def build(item):
 name,tree=item
 with (a.out/(name+'-build.log')).open('w') as log:
  subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+['-Ibuild/gen','-Ifirmware/src',f'-DTONE_UI_SOURCE="{tree}/tests/ui_test.c"','tests/grain_tone_audio_test.c','-lm','-o',str(a.out/name)],cwd=ROOT,stdout=log,stderr=log,check=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(build,[('base',a.base_tree),('fix',ROOT)]))
records=[]
def run(name,args,keep=False):
 ident='-'.join(map(str,args));pcm=a.out/(name+'-'+ident+'.pcm')
 r=subprocess.run([str(a.out/name),*map(str,args),str(pcm)],cwd=ROOT,capture_output=True,text=True)
 with (a.out/(name+'-run.log')).open('a') as f:f.write(r.stdout+r.stderr)
 assert r.returncode==0,(name,args,r.stdout,r.stderr)
 data=pcm.read_bytes();assert any(data),'silence is not audio equivalence'
 record=dict(args=args,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),trace=r.stdout.strip(),hf=float(re.search(r'HF=([\d.]+)',r.stdout)[1]))
 record['rms']=float(re.search(r'RMS=([\d.]+)',r.stdout)[1]);record['hf_energy']=record['hf']*record['rms']**2
 pcm.unlink();return (record,data) if keep else record
sources=[0,2,3,13,14,'user','sine'];tones=[0,1,32,64,96,126,127]
for kind in range(4):
 for source in sources:
  curve=[]
  for tone in tones:
   fixed=run('fix',[kind,source,tone,0,0,'static',4]);records.append(fixed);curve.append(fixed['hf_energy'])
  assert all(x<=y*1.0001 for x,y in zip(curve,curve[1:])) # 0.01% finite-window/integer noise floor near bypass,(kind,source,curve)
  assert curve[0]<curve[-1]*0.5,(kind,source,curve)
  assert len({x['sha256'] for x in records[-7:]})==7
  # All cutoff signs and resonance endpoints must preserve exact settled bypass.
  for cut in [-50,-25,0,25,50]:
   for res in [0,127]:
    args=[kind,source,127,cut,res,'static',4]
    fixed,newpcm=run('fix',args,True);old,oldpcm=run('base',args,True)
    assert newpcm==oldpcm,('bypass PCM bytes changed',args)
    assert fixed['sha256']==old['sha256'] and fixed['bytes']==old['bytes'],('bypass changed',args)
    fixed['base_sha256']=old['sha256'];records.append(fixed)
  records.append(run('fix',[kind,source,127,0,0,'live',4]))
 # Every requested tone crossed with cutoff/resonance endpoints; rotate real source material.
 for ci,cut in enumerate([-50,-25,0,25,50]):
  for res in [0,127]:
   for tone in tones:records.append(run('fix',[kind,sources[ci],tone,cut,res,'static',4]))
 print('personality',kind,': all sources, monotonic HF, exact bypass, cutoff/resonance and live matrix passed',flush=True)
# Validate complete spectral curves including the cutoff/resonance matrix.
groups={}
for row in records:
 args=row['args']
 if args[5]=='static':groups.setdefault((args[0],str(args[1]),args[3],args[4]),{})[args[2]]=row['hf_energy']
for key,values in groups.items():
 if len(values)==7:
  curve=[values[t] for t in tones]
  assert all(x<=y*1.0001 for x,y in zip(curve,curve[1:])),(key,curve)
print('Complete monotonic HF curves',sum(len(v)==7 for v in groups.values()))
(a.out/'comparisons.json').write_text(json.dumps(records,indent=2)+'\n')
print('TONE captures',len(records),'exact-base identities',sum('base_sha256' in x for x in records),'nonzero final output required throughout')
