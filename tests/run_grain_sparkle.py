#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exact-base PCM oracle plus deterministic SPARKLE audio/strict kernel probes."""
import argparse,array,hashlib,json,math,os,pathlib,subprocess,wave
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--base-tree',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()=='74533962cf44d7362497c5d3ed101a885e0e295d'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=a.base_tree,check=True)
def run(cmd,**kw):return subprocess.run(cmd,cwd=root,check=True,**kw)
flags=['cc','-O1','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src','-fsanitize=address,undefined','-fno-sanitize-recover=all']
run(flags+['tests/grain_sparkle_test.c','-lm','-o',str(a.out/'kernel')]);run([str(a.out/'kernel')])
for name,extra in [('base',[f'-DGRANULAR_QUALITY_UI_SOURCE="{a.base_tree.resolve()}/tests/ui_test.c"']),('new',[])]:
 run(flags+['-fno-sanitize=signed-integer-overflow,shift','-DGRANULAR_QUALITY_LEGACY_SHORT_PROBE=1']+extra+['tests/granular_quality_test.c','-lm','-o',str(a.out/name)])
records={};identities=0
baseenv={k:v for k,v in os.environ.items() if k not in ['GRC_VALUE','GRC_LIVE','GRAIN_SOURCE']}
def render(binary,kind,scenario,c=0,source=None,live=False):
 key=f'{binary}-{kind}-{scenario}-{c}-{source}-{int(live)}';path=a.out/(key+'.pcm');env={**baseenv,'GRC_VALUE':str(c)}
 if source is not None:env['GRAIN_SOURCE']=str(source)
 if live:env['GRC_LIVE']='1'
 r=run([str(a.out/binary),str(kind),str(scenario),str(path)],env=env,capture_output=True,text=True);d=json.loads(r.stdout);b=path.read_bytes();x=array.array('i');x.frombytes(b)
 assert d['rms']>0.1 and d['nonzero']>0 and 0<d['peak']<32767
 d['sha256']=hashlib.sha256(b).hexdigest();d['jump']=max(abs(x[i]-x[i-2]) for i in range(2,len(x)));d['hf_energy']=sum((x[i]-x[i-2])**2 for i in range(2,len(x)))/len(x)
 records[key]=d
 if binary=='new' and kind==3 and scenario==1 and source==13 and not live:
  with wave.open(str(a.out/f'SYNTH-SPARKLE-C{c}.wav'),'wb') as w:
   w.setnchannels(2);w.setsampwidth(2);w.setframerate(44100);w.writeframes(array.array('h',x).tobytes())
 return d,b
for kind in range(4):
 for scenario in range(6):
  old,b=render('base',kind,scenario);new,n=render('new',kind,scenario);assert b==n and old==new;identities+=1
  if kind==0 and scenario!=5:
   d,c=render('new',kind,scenario,100);assert c==b and d==new;identities+=1
print(f'{identities} exact base/C0 and unaffected C100 comparisons passed',flush=True)
for source in [0,2,3,13,14]:
 old,b=render('base',3,1,source=source);zero,z=render('new',3,1,source=source);assert b==z and old==zero
 outputs=[]
 for c in [25,50,100]:
  d,n=render('new',3,1,c,source);repeat,r=render('new',3,1,c,source);assert n==r and d==repeat and n!=z;outputs.append(d['sha256'])
  # Difference must be substantive, not merely one changed integer sample.
  x=array.array('i');x.frombytes(n);y=array.array('i');y.frombytes(z)
  difference=math.sqrt(sum((a-b)**2 for a,b in zip(x,y))/len(x));assert difference>1
  print(f'SRC{source} C{c}: RMS={d["rms"]:.3f}, peak={d["peak"]:.0f}, diffRMS={difference:.3f}, jump={d["jump"]}',flush=True)
 assert len(set(outputs))==3
 d,n=render('new',3,3,100,source,True);e,r=render('new',3,3,100,source,True);assert n==r and d==e
for c in [25,50,100]:render('new',3,5,c)
(a.out/'measurements.json').write_text(json.dumps(records,indent=2,sort_keys=True)+'\n')
print('SPARKLE PCM: five sources, C25/50/100, repeat/live reverse and four-track bounds passed',flush=True)
