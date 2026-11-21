#!/usr/bin/env python3
"""Exact recovered-base bypass PCM and intentional polarity differences (host only)."""
import argparse, json, os, pathlib, subprocess, re
p=argparse.ArgumentParser();p.add_argument('--base-tree',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);p.add_argument('--transitions-only',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];a.out.mkdir(parents=True,exist_ok=True);a.out=a.out.resolve();a.base_tree=a.base_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()=='3ed05c21e5ae4f1e3b8a6f4fb0426be16c58cb26'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware/src'],cwd=a.base_tree,check=True)
flags=['-Os','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src']
cc=os.environ.get('CC','cc');total=0;count=0;changed=0
metrics={}
for side,tree in [('base',a.base_tree),('new',root)]:
 exe=a.out/f'transition-{side}'
 defines=['-DTRANSITION_MEASURE_BASE=1'] if side=='base' else []
 subprocess.run([cc,*flags,*defines,f'-DGRAIN_TRANSITION_UI_SOURCE="{tree}/tests/ui_test.c"','tests/grain_filter_transition_test.c','-lm','-o',str(exe)],cwd=root,check=True)
 r=subprocess.run([str(exe)],cwd=root,check=True,text=True,capture_output=True);(a.out/f'transition-{side}.log').write_text(r.stdout+r.stderr)
 edges=list(map(int,re.findall(r'first-edge=(\d+)',r.stdout)));steps=list(map(int,re.findall(r'max-step=(\d+)',r.stdout)))
 assert len(edges)==len(steps)==96
 metrics[side]={'first_edge':max(edges),'adjacent_step':max(steps)}
assert metrics['base']['first_edge']>1000 and metrics['new']['first_edge']==0
assert metrics['new']['adjacent_step']<=1024 and metrics['new']['adjacent_step']<metrics['base']['adjacent_step']/10
(a.out/'transitions.json').write_text(json.dumps(metrics,indent=2)+'\n');print(metrics,flush=True)
if a.transitions_only: raise SystemExit(0)
for side,tree in [('base',a.base_tree),('new',root)]:
 subprocess.run([cc,*flags,f'-DFILTER_UI_SOURCE="{tree}/tests/ui_test.c"','tests/grain_headroom_audio_test.c','-lm','-o',str(a.out/f'audio-{side}')],cwd=root,check=True)
for kind in range(4):
 for source in [0,2,3,13,14,'user','sine']:
  for cut in [-50,-25,-1,0,1,25,50]:
   for res in [0,127]:
    args=[kind,source,127,cut,res,'static',4];data=[]
    for side in ['base','new']:
     path=a.out/f'{side}.pcm';r=subprocess.run([str(a.out/f'audio-{side}'),*map(str,args),str(path)],cwd=root,text=True,capture_output=True);assert r.returncode==0,(args,r.stdout,r.stderr)
     b=path.read_bytes();assert any(b),('silent output',args);data.append(b)
     with (a.out/f'{side}.log').open('a') as f:f.write(r.stdout)
    if cut == 0:
     assert data[0]==data[1],('bypass PCM changed',args);total+=len(data[0]);count+=1
    else:
     assert data[0]!=data[1],('polarity reversal missing',args);changed+=1
 print(f'personality {kind}: bypass PCM identical; nonzero cutoff PCM intentionally changed',flush=True)
result={'cases':count,'identical_bytes':total,'intentional_nonzero_changes':changed,'sources':7,'tracks':4,'cutoffs':[-50,-25,-1,0,1,25,50],'resonance':[0,127]};(a.out/'identity.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
