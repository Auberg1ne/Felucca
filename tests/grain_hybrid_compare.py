#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Hybrid: disabled compatibility, six policies, independent optimization checks."""
import argparse,concurrent.futures,csv,hashlib,os,re,shlex,struct,subprocess,wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--base-tree',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--bench',action='store_true');a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);a.base_tree=a.base_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()=='ae9d55541f26199e57813d8268c53b3e6265987e'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=a.base_tree,check=True)
cc=shlex.split(os.environ.get('CC','cc'));flags=['-O1','-g','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src'];old=['-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1','-DFELUCCA_GRAIN_MORPH_EXPERIMENT=1','-DFELUCCA_GRAIN_FIDELITY_EXPERIMENT=1'];hybrid=old+['-DFELUCCA_GRAIN_HYBRID_EXPERIMENT=1'];san=['-fsanitize=address,undefined','-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow','-fno-sanitize-recover=undefined']
def build(name,src,extra):
 with (a.out/(name+'.build.log')).open('w') as log:subprocess.run(cc+flags+extra+[src,'-lm','-o',str(a.out/name)],cwd=ROOT,stdout=log,stderr=log,check=True)
for name,extra in [('base',old+[f'-DHYBRID_UI_SOURCE="{a.base_tree}/tests/ui_test.c"']),('disabled',old),('audio',hybrid)]:build(name,'tests/grain_hybrid_audio_test.c',extra+san)
build('kernel','tests/grain_hybrid_kernel_test.c',hybrid+['-fsanitize=address,undefined','-fno-sanitize-recover=all'])
with (a.out/'kernel.log').open('w') as f:subprocess.run([str(a.out/'kernel'),str(a.out/'kernel.csv')],stdout=f,stderr=f,check=True)
print((a.out/'kernel.log').read_text().strip(),flush=True)
build('latch','tests/grain_hybrid_latch_test.c',hybrid+san)
for threshold in [32,64,128]:
 for cap in [4,8]:
  for opt in [0,1]:
   for tiny in [0,1]:
    r=subprocess.run([str(a.out/'latch'),str(threshold),str(cap),str(opt),str(tiny)],capture_output=True,text=True,check=True)
    (a.out/f'latch-{threshold}-{cap}-{opt}-{tiny}.log').write_text(r.stdout+r.stderr)
rows=[]
def run(exe,method,cap,threshold,opt,t,src,sc,m=32):
 path=a.out/f'{exe}-{method}-{cap}-{threshold}-{opt}-{t}-{src}-{sc}-{m}.pcm'
 r=subprocess.run([str(a.out/exe),str(method),str(cap),str(threshold),str(opt),'morph',str(m),str(t),str(src),str(sc),str(path)],cwd=ROOT,capture_output=True,text=True)
 assert r.returncode==0,(exe,method,cap,threshold,opt,t,src,sc,m,r.stdout,r.stderr);assert not r.stderr,r.stderr
 data=path.read_bytes();path.unlink();return data,dict(re.findall(r'([\w-]+)=([^\s]+)',r.stdout))
cases=set((t,3,1,m) for t in range(4) for m in range(128));cases.update((t,s,sc,m) for t in range(4) for s in [0,2,3,13,14,'user','sine'] for sc in range(3) for m in [0,32,64,127])
def identity(c):
 t,s,sc,m=c;out=[];res=[]
 for exe in ['base','disabled','audio']:
  data,r=run(exe,0,8,32,0,t,s,sc,m);out.append(data);res.append(r)
 assert out[0]==out[1]==out[2],('A identity',c)
 # Previous B4, B8 and C remain unchanged with hybrid compiled in.
 if s==3 and sc==1 and m in [0,32,64,127]:
  for method,cap in [(1,4),(1,8),(2,8)]:
   b,_=run('base',method,cap,32,0,t,s,sc,m);v,_=run('audio',method,cap,32,0,t,s,sc,m);assert b==v,('old method changed',c,method,cap)
 return res[-1]
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:rows.extend(pool.map(identity,sorted(cases,key=str)))
print(len(cases),'A identity cases + 48 old B/C identity cases passed',flush=True)
policies=[(threshold,cap) for threshold in [32,64,128] for cap in [4,8]]
# Full source/personality/duration matrix, all anchors and an intermediate.
alt=[(t,s,sc,m,th,cap) for t in range(4) for s in [0,2,3,13,14,'user','sine','short35','short131'] for sc in ([1] if s in ['short35','short131'] else [0,1,2]) for m in [0,32,64,127] for th,cap in policies]
def alternative(c):
 t,s,sc,m,th,cap=c;b,r0=run('audio',3,cap,th,0,t,s,sc,m);v,r1=run('audio',3,cap,th,1,t,s,sc,m);assert b==v,('optimization PCM',c)
 for k in ['forward','reverse','grain-bytes','part-bytes','track-bytes']:assert r0[k]==r1[k],('state',c,k)
 return r1
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:rows.extend(pool.map(alternative,alt))
# Listening: all personalities, normal and fitted short sources; initial/reference
# plus candidate 64 and 128 crossovers, with optimization separately verified above.
meta=[]
for t,name in enumerate(['cloud','freeze','stutter','shimmer']):
 for src in [3,'short35','short131']:
  configs=[('A',0,8,32,0),('B4',1,4,32,0),('B8',1,8,32,0),('C',2,8,32,0),('H32',3,8,32,1),('H64',3,8,64,1),('H128',3,8,128,1)]
  recordings=[]
  for label,method,cap,th,opt in configs:
   b,r=run('audio',method,cap,th,opt,t,src,1);rows.append(r);recordings.append((label,struct.unpack('<%di'%(len(b)//4),b)))
  rms=[(sum(x*x for x in v)/len(v))**.5 for _,v in recordings];target=min(32767*10**(-24/20),.95*32767*min(rms[i]/max(map(abs,v)) for i,(_,v) in enumerate(recordings) if rms[i]))
  for i,(label,v) in enumerate(recordings):
   gain=target/rms[i] if rms[i] else 0;meta.append(dict(type=t,source=src,label=label,gain=gain,raw_RMS=rms[i],target_RMS=target,status='matched' if rms[i] else 'silent_unmatchable'))
   for kind,data in [('raw',v),('matched',[round(x*gain) for x in v])]:
    assert max(map(abs,data))<32768
    with wave.open(str(a.out/f'{name}-{src}-{label}-{kind}.wav'),'wb') as f:f.setnchannels(2);f.setsampwidth(2);f.setframerate(44100);f.writeframes(struct.pack('<%dh'%len(data),*data))
for filename,data in [('audio.csv',rows),('listening.csv',meta)]:
 with (a.out/filename).open('w') as f:w=csv.DictWriter(f,fieldnames=list(data[0]),lineterminator='\n');w.writeheader();w.writerows(data)
(a.out/'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in sorted(a.out.glob('*.wav'))))
print('audio executions',len(cases)*3+96+len(alt)*2+84,'passed; six policy optimization pairs',len(alt),'168 WAVs',flush=True)

# Resource comparison uses one unchanged harness against both source trees.
for name,extra in [('base',old+[f'-DHYBRID_UI_SOURCE="{a.base_tree}/tests/ui_test.c"']),('disabled',old),('enabled',hybrid)]:
 subprocess.run(cc+['-Os','-w','-ffp-contract=off','-fstack-usage','-Ibuild/gen','-Ifirmware/src']+extra+['-c','tests/grain_hybrid_audio_test.c','-o',str(a.out/f'resource-{name}.o')],cwd=ROOT,check=True)
assert (a.out/'resource-base.o').read_bytes()==(a.out/'resource-disabled.o').read_bytes()
(a.out/'resource-size.log').write_text(subprocess.check_output(['size',*[str(a.out/f'resource-{n}.o') for n in ['base','disabled','enabled']]],text=True))
if a.bench:
 for name in ['bench','load_test','mix_bench']:
  build(name,'tests/grain_hybrid_'+name+'.c',hybrid+['-O2'])
  subprocess.run([str(a.out/name),str(a.out/(name+'.csv'))],cwd=ROOT,check=True)

 # Fresh mixer processes must have identical non-envelope state for every policy.
 mix=list(csv.DictReader((a.out/'mix_bench.csv').open()))
 for key in {(r['personality'],r['fast']) for r in mix}:
  group=[r for r in mix if (r['personality'],r['fast'])==key]
  assert len({(r['grain_hash'],r['max_grains'],r['mean_grains']) for r in group})==1
