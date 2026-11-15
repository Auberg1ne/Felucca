#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Fidelity methods: legacy PCM identity, all morphs, short-zone listening."""
import argparse, concurrent.futures, csv, hashlib, os, re, shlex, struct, subprocess, wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--base-tree',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);a.base_tree=a.base_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()=='4c34b6e38612ca13d71aee5a5cc3cd7d9da3b07d'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=a.base_tree,check=True)
def legacy_function(path):
 text=path.read_text();start=text.index('static int gr_run(');brace=text.index('{',start);depth=0
 for i in range(brace,len(text)):
  if text[i]=='{':depth+=1
  elif text[i]=='}':
   depth-=1
   if not depth:return text[start:i+1]
assert legacy_function(ROOT/'firmware/src/eng_grain.c')==legacy_function(a.base_tree/'firmware/src/eng_grain.c')
cc=shlex.split(os.environ.get('CC','cc'));flags=['-O1','-g','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src'];exp=['-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1','-DFELUCCA_GRAIN_MORPH_EXPERIMENT=1'];fidelity=exp+['-DFELUCCA_GRAIN_FIDELITY_EXPERIMENT=1'];san=['-fsanitize=address,undefined','-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow','-fno-sanitize-recover=undefined']
def build(name,source,extra):
 with (a.out/(name+'.build.log')).open('w') as f:subprocess.run(cc+flags+extra+[source,'-lm','-o',str(a.out/name)],cwd=ROOT,stdout=f,stderr=f,check=True)
for name,extra in [('base',exp+[f'-DFIDELITY_UI_SOURCE="{a.base_tree}/tests/ui_test.c"']),('audio',fidelity)]:build(name,'tests/grain_fidelity_audio_test.c',extra+san)
build('kernel','tests/grain_fidelity_kernel_test.c',fidelity+['-fsanitize=address,undefined','-fno-sanitize-recover=all'])
with (a.out/'kernel.log').open('w') as f:subprocess.run([str(a.out/'kernel'),str(a.out/'kernel.csv')],stdout=f,stderr=f,check=True)
print((a.out/'kernel.log').read_text().strip(),flush=True)
build('latch','tests/grain_fidelity_latch_test.c',fidelity+san)
for method,cap in [(1,4),(1,8),(1,16),(2,4)]:
 r=subprocess.run([str(a.out/'latch'),str(method),str(cap)],capture_output=True,text=True,check=True)
 (a.out/f'latch-{method}-{cap}.log').write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)
rows=[]
def run(exe,method,cap,t,src,sc,val=32,mode='morph'):
 path=a.out/f'{exe}-{method}-{cap}-{t}-{src}-{sc}-{val}-{mode}.pcm'
 r=subprocess.run([str(a.out/exe),str(method),str(cap),mode,str(val),str(t),str(src),str(sc),str(path)],cwd=ROOT,capture_output=True,text=True)
 assert r.returncode==0,(exe,method,t,src,sc,val,r.stdout,r.stderr);assert not r.stderr,r.stderr
 data=path.read_bytes();path.unlink();return data,dict(re.findall(r'([\w-]+)=([^ ]+)',r.stdout))
# All morph values, every personality; all source/duration anchor states.
cases=set((t,3,1,m) for t in range(4) for m in range(128))
cases.update((t,s,sc,m) for t in range(4) for s in [0,2,3,13,14,'user','sine'] for sc in range(3) for m in [0,32,64,127])
def identity(c):
 t,s,sc,m=c;b,_=run('base',0,4,t,s,sc,m);v,r=run('audio',0,4,t,s,sc,m);assert b==v,('legacy identity',c);return r
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:rows.extend(pool.map(identity,sorted(cases,key=str)))
print(len(cases),'exact previous-morph/legacy comparisons passed',flush=True)
alts=[(t,s,sc,m,method,cap) for t in range(4) for s in [0,2,3,13,14,'user','sine'] for sc in range(3) for m in [0,32,64,127] for method,cap in [(1,4),(1,8),(1,16),(2,4)]]
ref_state={(r['type'],r['source'],r['scenario'],r['control']):tuple(r[k] for k in ['forward','reverse','grain-bytes','part-bytes','track-bytes']) for r in rows}
def alternative(c):
 t,s,sc,m,method,cap=c;_,r=run('audio',method,cap,t,s,sc,m)
 assert tuple(r[k] for k in ['forward','reverse','grain-bytes','part-bytes','track-bytes'])==ref_state[(str(t),str(s),str(sc),str(m))],('admission/state changed',c)
 return r
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:rows.extend(pool.map(alternative,alts))
# Different methods and morphs on simultaneous parts; no global method coercion.
for t in range(4):
 for s in [3,'short35','short131']:
  _,r=run('audio','mixed',4,t,s,1,32,'independent');rows.append(r)
# Representative raw and matched files. Silence cannot be RMS-matched.
listening_metadata=[]
for t,name in enumerate(['cloud','freeze','stutter','shimmer']):
 for src in [3,'short35','short131']:
  rendered=[]
  for method in [0,1,2]:
   data,r=run('audio',method,4,t,src,1);rows.append(r);rendered.append(struct.unpack('<%di'%(len(data)//4),data))
  rms=[(sum(x*x for x in v)/len(v))**.5 for v in rendered];target=min(32767*10**(-24/20),.95*32767*min(rms[i]/max(map(abs,rendered[i])) for i in range(3) if rms[i]))
  for method,v in enumerate(rendered):
   assert max(map(abs,v))<32768;gain=target/rms[method] if rms[method] else 0
   listening_metadata.append(dict(type=t,source=src,method=method,raw_RMS=rms[method],gain=gain,target_RMS=target,status='matched' if rms[method] else 'silent_unmatchable'))
   for kind,data in [('raw',v),('matched',[round(x*gain) for x in v])]:
    assert max(map(abs,data))<32768
    with wave.open(str(a.out/f'{name}-{src}-{["A","B4","C"][method]}-{kind}.wav'),'wb') as f:f.setnchannels(2);f.setsampwidth(2);f.setframerate(44100);f.writeframes(struct.pack('<%dh'%len(data),*data))
rows.sort(key=lambda r:tuple(r[k] for k in ['type','source','scenario','mode','control','method','cap']))
with (a.out/'audio.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
with (a.out/'listening.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=list(listening_metadata[0]),lineterminator='\n');w.writeheader();w.writerows(listening_metadata)
(a.out/'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in sorted(a.out.glob('*.wav'))))
print('alternative renders',len(alts),'mixed 12, listening 36; total audio runs',2*len(cases)+len(alts)+48,'passed; 72 WAVs (silent baselines explicitly retained)',flush=True)
