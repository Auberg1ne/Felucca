#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exact discrete/anchor identities, all morphs, independent tracks and WAVs."""
import argparse, concurrent.futures, csv, hashlib, json, os, re, shlex, struct, subprocess, wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--base-tree',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);a.base_tree=a.base_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()=='ccf1e227c0f585ed03a5263d870c9dc4a708fc26'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=a.base_tree,check=True)
cc=shlex.split(os.environ.get('CC','cc'));flags=['-O1','-g','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src'];windows=['-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1'];morph=windows+['-DFELUCCA_GRAIN_MORPH_EXPERIMENT=1']
san=['-fsanitize=address,undefined','-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow','-fno-sanitize-recover=undefined'];strict=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
def build(name,source,extra):
 with (a.out/(name+'.build.log')).open('w') as log:subprocess.run(cc+flags+extra+[source,'-lm','-o',str(a.out/name)],cwd=ROOT,stdout=log,stderr=log,check=True)
for name,source,extra in [('base','grain_morph_audio_test',windows+[f'-DMORPH_UI_SOURCE="{a.base_tree}/tests/ui_test.c"']+san),('disabled','grain_morph_audio_test',windows+san),('audio','grain_morph_audio_test',morph+san),('numeric','grain_morph_numeric_test',morph+strict),('kernel','grain_morph_kernel_test',morph+strict),('latch','grain_morph_latch_test',morph+san)]:build(name,'tests/'+source+'.c',extra)
for name in ['numeric','kernel','latch']:
 r=subprocess.run([str(a.out/name)],capture_output=True,text=True);(a.out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==0,(name,r.stdout,r.stderr);print(r.stdout.strip(),flush=True)
measurements=[]
def run(exe,mode,val,t,src,sc):
 path=a.out/f'{exe}-{mode}-{val}-{t}-{src}-{sc}.pcm'
 r=subprocess.run([str(a.out/exe),mode,str(val),str(t),str(src),str(sc),str(path)],cwd=ROOT,capture_output=True,text=True)
 assert r.returncode==0,(exe,mode,val,t,src,sc,r.stdout,r.stderr)
 assert not r.stderr,r.stderr
 pcm=path.read_bytes();path.unlink();row=dict(re.findall(r'([\w-]+)=([^ ]+)',r.stdout));return pcm,row
# All source/personality/duration states: off identity and exact anchors.
def identity(case):
 t,src,sc=case;rows=[]
 for sel,anchor in [(0,64),(1,127),(2,0)]:
  base,br=run('base','off',sel,t,src,sc);disabled,dr=run('disabled','off',sel,t,src,sc);off,orr=run('audio','off',sel,t,src,sc);on,onr=run('audio','morph',anchor,t,src,sc)
  assert base==disabled==off==on,('discrete/anchor identity',case,sel)
  for k in ['forward','reverse','grain-bytes','part-bytes','track-bytes']:assert br[k]==dr[k]==orr[k]==onr[k],('state',case,k)
  rows.append(onr)
 return rows
cases=[(t,src,sc) for t in range(4) for src in [0,2,3,13,14,'user','sine'] for sc in range(3)]
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
 for rows in pool.map(identity,cases):measurements.extend(rows)
print('84 source/personality/duration states: 1008 off/anchor renders exact',flush=True)
# Every control value sounds; no shape mutation or allocation-count changes.
def sweep(t):
 rows=[];listening={};baseline=None
 for val in range(128):
  data,row=run('audio','morph',val,t,3,1);rows.append(row)
  state=tuple(row[k] for k in ['forward','reverse','grain-bytes','part-bytes','track-bytes'])
  if baseline is None:baseline=state
  assert state==baseline,('morph affected scheduling/state',t,val)
  if val in [0,16,32,48,64,80,96,112,127]:listening[val]=struct.unpack('<%di'%(len(data)//4),data)
 target=min(32767*10**(-24/20),.8*min((sum(x*x for x in v)/len(v))**.5 for v in listening.values()))
 for val,v in listening.items():
  assert max(map(abs,v))<32768
  gain=target/(sum(x*x for x in v)/len(v))**.5;assert 0<gain<=1
  for kind,data in [('raw',v),('matched',[round(x*gain) for x in v])]:
   with wave.open(str(a.out/f'{["cloud","freeze","stutter","shimmer"][t]}-m{val:03d}-{kind}.wav'),'wb') as f:f.setnchannels(2);f.setsampwidth(2);f.setframerate(44100);f.writeframes(struct.pack('<%dh'%len(data),*data))
 # Independently held per-track settings and changing future-spawn controls.
 for mode in ['independent','live']:
  for src in [0,2,3,13,14,'user','sine']:
   _,row=run('audio',mode,16,t,src,1);rows.append(row)
 return rows
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
 for rows in pool.map(sweep,range(4)):measurements.extend(rows)
with (a.out/'measurements.csv').open('w') as f:
 writer=csv.DictWriter(f,fieldnames=list(measurements[0]),lineterminator='\n');writer.writeheader();writer.writerows(measurements)
(a.out/'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in sorted(a.out.glob('*.wav'))))
print('512 all-value renders + 56 independent/live renders passed; 1576 total audio runs; 72 raw/matched WAVs',flush=True)
