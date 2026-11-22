#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exact-base PCM oracle plus deterministic DRIFT activation compatibility and lifecycle probes."""
import argparse,array,hashlib,json,math,os,pathlib,subprocess,wave
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--base-tree',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()=='1d0e3ae47488a058ffeda00835dbc0cf518637d8'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=a.base_tree,check=True)
def run(cmd,**kw):return subprocess.run(cmd,cwd=root,check=True,**kw)
flags=['cc','-O1','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src','-fsanitize=address,undefined','-fno-sanitize-recover=all']
for name in ['grain_drift_state_test','grain_frozen_drift_test']:
 run(flags+[f'tests/{name}.c','-lm','-o',str(a.out/name)]);run([str(a.out/name)])
for name,extra in [('base',[f'-DGRANULAR_QUALITY_UI_SOURCE="{a.base_tree.resolve()}/tests/ui_test.c"']),('new',[])]:
 run(flags+['-fno-sanitize=signed-integer-overflow,shift','-DGRANULAR_QUALITY_LEGACY_SHORT_PROBE=1']+extra+['tests/granular_quality_test.c','-lm','-o',str(a.out/name)])
records={};identities=0
baseenv={k:v for k,v in os.environ.items() if k not in ['GRC_VALUE','GRC_LIVE','GRAIN_SOURCE','GRAIN_BLOCKS','DRIFT_REPORT']}
def render(binary,kind,scenario,c=0,source=None,live=False,blocks=2048):
 key=f'{binary}-{kind}-{scenario}-{c}-{source}-{int(live)}-{blocks}';path=a.out/(key+'.pcm');env={**baseenv,'GRC_VALUE':str(c)}
 if source is not None:env['GRAIN_SOURCE']=str(source)
 if live:env['GRC_LIVE']='chaos'
 env['GRAIN_BLOCKS']=str(blocks)
 if blocks>2048:env['DRIFT_REPORT']='1'
 r=run([str(a.out/binary),str(kind),str(scenario),str(path)],env=env,capture_output=True,text=True);d=json.loads(r.stdout);b=path.read_bytes();x=array.array('i');x.frombytes(b)
 assert d['rms']>0.1 and d['nonzero']>0 and 0<d['peak']<32767
 d['sha256']=hashlib.sha256(b).hexdigest();d['jump']=max(abs(x[i]-x[i-2]) for i in range(2,len(x)));d['hf_energy']=sum((x[i]-x[i-2])**2 for i in range(2,len(x)))/len(x)
 if blocks>2048:
  label,phase,offset=r.stderr.strip().split();assert label=='DRIFT';assert int(phase)==(blocks*32*4869)&0xffffffff and abs(int(offset))<=2097152
  d['phase']=int(phase);d['displacement']=int(offset)
 records[key]=d
 if binary=='new' and kind==1 and scenario==1 and source==13 and not live:
  with wave.open(str(a.out/f'SYNTH-DRIFT-C{c}-{blocks}.wav'),'wb') as w:
   w.setnchannels(2);w.setsampwidth(2);w.setframerate(44100);w.writeframes(array.array('h',x).tobytes())
 return d,b
active=0
def compare(kind,scenario,c=0,source=None,live=False):
 global identities,active
 old,b=render('base',kind,scenario,c,source,live);new,n=render('new',kind,scenario,c,source,live)
 enabled=(kind==1 or scenario==5) and (c>0 or live)
 if not enabled:
  assert b==n and old==new;identities+=1
 else:
  assert b!=n
  for field in ['rng_draws','admitted','reverse_admitted','max_load_q16','max_active','filter_changes']:
   assert old[field]==new[field],(field,kind,scenario,c,source)
  repeat,r=render('new',kind,scenario,c,source,live);assert n==r and new==repeat
  x=array.array('i');x.frombytes(n);y=array.array('i');y.frombytes(b);diff=math.sqrt(sum((a-b)**2 for a,b in zip(x,y))/len(x));assert diff>1
  active+=1
for kind in range(4):
 for scenario in range(6):
  for c in [0,25,50,100]:
   compare(kind,scenario,c)
print(f'24 C0 cases exact; {identities} inactive pairs exact and {active} active DRIFT cases distinct/deterministic',flush=True)
for source in [0,2,3,13,14]:
 for kind in [1,2,3]:
  for c in [25,50,100]:
   compare(kind,1,c,source)
  compare(kind,3,100,source,True)
long,b=render('new',1,1,100,13,False,65536)
again,r=render('new',1,1,100,13,False,65536);assert b==r and long==again
print('47.55-second real mixer render passed two phase wraps, bounds and exact repeat',flush=True)
(a.out/'measurements.json').write_text(json.dumps(records,indent=2,sort_keys=True)+'\n')
print(f'DRIFT activation: {identities} inactive pairs exact, {active} active pairs distinct/repeatable; unchanged scheduling/RNG/load; five sources/live reverse passed',flush=True)
