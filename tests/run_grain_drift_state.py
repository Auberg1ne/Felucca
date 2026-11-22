#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exact-base PCM oracle plus deterministic DRIFT foundation identity and lifecycle probes."""
import argparse,array,hashlib,json,math,os,pathlib,subprocess,wave
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--base-tree',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()=='79fd756709ff37fb7e82bbd8c814110999213dec'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=a.base_tree,check=True)
def run(cmd,**kw):return subprocess.run(cmd,cwd=root,check=True,**kw)
flags=['cc','-O1','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src','-fsanitize=address,undefined','-fno-sanitize-recover=all']
run(flags+['tests/grain_drift_state_test.c','-lm','-o',str(a.out/'kernel')]);run([str(a.out/'kernel')])
for name,extra in [('base',[f'-DGRANULAR_QUALITY_UI_SOURCE="{a.base_tree.resolve()}/tests/ui_test.c"']),('new',[])]:
 run(flags+['-fno-sanitize=signed-integer-overflow,shift','-DGRANULAR_QUALITY_LEGACY_SHORT_PROBE=1']+extra+['tests/granular_quality_test.c','-lm','-o',str(a.out/name)])
records={};identities=0
baseenv={k:v for k,v in os.environ.items() if k not in ['GRC_VALUE','GRC_LIVE','GRAIN_SOURCE']}
def render(binary,kind,scenario,c=0,source=None,live=False):
 key=f'{binary}-{kind}-{scenario}-{c}-{source}-{int(live)}';path=a.out/(key+'.pcm');env={**baseenv,'GRC_VALUE':str(c)}
 if source is not None:env['GRAIN_SOURCE']=str(source)
 if live:env['GRC_LIVE']='chaos'
 r=run([str(a.out/binary),str(kind),str(scenario),str(path)],env=env,capture_output=True,text=True);d=json.loads(r.stdout);b=path.read_bytes();x=array.array('i');x.frombytes(b)
 assert d['rms']>0.1 and d['nonzero']>0 and 0<d['peak']<32767
 d['sha256']=hashlib.sha256(b).hexdigest();d['jump']=max(abs(x[i]-x[i-2]) for i in range(2,len(x)));d['hf_energy']=sum((x[i]-x[i-2])**2 for i in range(2,len(x)))/len(x)
 records[key]=d
 return d,b
for kind in range(4):
 for scenario in range(6):
  for c in [0,25,50,100]:
   old,b=render('base',kind,scenario,c);new,n=render('new',kind,scenario,c);assert b==n and old==new;identities+=1
print(f'{identities} exact Task242 PCM/legacy-state comparisons passed (24 cases x4 C values)',flush=True)
for source in [0,2,3,13,14]:
 for kind in [1,2,3]:
  for c in [25,50,100]:
   old,b=render('base',kind,1,c,source);new,n=render('new',kind,1,c,source);assert b==n and old==new;identities+=1
  old,b=render('base',kind,3,100,source,True);new,n=render('new',kind,3,100,source,True);assert b==n and old==new;identities+=1
(a.out/'measurements.json').write_text(json.dumps(records,indent=2,sort_keys=True)+'\n')
print(f'DRIFT foundation: {identities} exact PCM/legacy-state pairs, FROZEN/SPARKLE/CHAOS, five sources and live reverse passed',flush=True)
