#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Production WIN vs exact I1 Hann and validated 4c34b6e morph; no goldens."""
import argparse,concurrent.futures,subprocess,os,re,csv,hashlib,shlex
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--i1-tree',type=Path,required=True);p.add_argument('--experiment-tree',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
for tree,sha in [(a.i1_tree,'68f3e7a848b2410670945c07b6a66f3689ee88e3'),(a.experiment_tree,'4c34b6e38612ca13d71aee5a5cc3cd7d9da3b07d')]:
 assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=tree,text=True).strip()==sha
 subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=tree,check=True)
# I1 remains the production base: original Hann and endpoint kernel are intact.
def function_body(text,name):
 match=re.search(r'static[^{};]*\b'+name+r'\([^;{}]*\)[^{;]*\{',text);assert match,name
 end=match.end();depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[match.start():end]
current=(root/'firmware/src/eng_grain.c').read_text();legacy=(a.i1_tree/'firmware/src/eng_grain.c').read_text();reference=(a.experiment_tree/'firmware/src/eng_grain.c').read_text()
for name in ['gr_run','gr_win']:assert function_body(current,name)==function_body(legacy,name),name
assert function_body(current,'gr_morph_win')==function_body(reference,'gr_morph_win')
for name in ['grain_gauss.h','grain_tukey.h','grain_morph.h']:assert(root/'firmware/src'/name).read_bytes()==(a.experiment_tree/'firmware/src'/name).read_bytes(),name
print('I1 Hann/kernel and validated morph/tables are source-identical',flush=True)
base=shlex.split(os.environ.get('CC','cc'))+['-O1','-g','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src']
san=['-fsanitize=address,undefined','-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow','-fno-sanitize-recover=all']
def build(spec):
 name,tree,flags=spec
 with(a.out/(name+'.build.log')).open('w')as log:subprocess.run(base+san+flags+[f'-DMORPH_UI_SOURCE="{tree.resolve()}/tests/ui_test.c"','tests/grain_win_i2_audio_test.c','-lm','-pthread','-o',str(a.out/name)],cwd=root,stdout=log,stderr=log,check=True)
 return name
specs=[('i1',a.i1_tree,[]),('experiment',a.experiment_tree,['-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1','-DFELUCCA_GRAIN_MORPH_EXPERIMENT=1']),('production',root,['-DWIN_I2_PRODUCTION=1']),('dual',root,['-DWIN_I2_PRODUCTION=1','-DC1_HOST=1'])]
with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:
 for name in pool.map(build,specs):print('built',name,flush=True)
rows=[];runs=0
# Each execution is a fresh process: ui_power_on alone does not reset every DSP static.
def run(exe,mode,val,t,src,sc):
 path=a.out/f'{exe}-{mode}-{val}-{t}-{src}-{sc}.pcm'
 r=subprocess.run([str(a.out/exe),mode,str(val),str(t),str(src),str(sc),str(path)],cwd=root,capture_output=True,text=True)
 assert not r.returncode and not r.stderr,(exe,mode,val,t,src,sc,r.stdout,r.stderr)
 data=path.read_bytes();path.unlink();row=dict(re.findall(r'([\w-]+)=([^ ]+)',r.stdout.strip()));row['sha256']=hashlib.sha256(data).hexdigest();return data,row
sources=[0,2,3,13,14,'user','sine']
def anchor(case):
 t,src,sc=case;rr=[];legacy,lr=run('i1','morph',64,t,src,sc)
 for value in [0,64,127]:
  x,xr=run('experiment','morph',value,t,src,sc);y,yr=run('production','morph',value,t,src,sc)
  assert x==y,('anchor',case,value)
  if value==64:assert y==legacy,('Hann default',case)
  for key in ['forward','reverse','grain-bytes','part-bytes','track-bytes']:assert xr[key]==yr[key],(case,value,key)
  rr.append(yr)
 return rr
with concurrent.futures.ThreadPoolExecutor(max_workers=3)as pool:
 for rr in pool.map(anchor,[(t,s,sc)for t in range(4)for s in sources for sc in range(3)]):rows+=rr
print('84 legacy Hann comparisons; 252 Gaussian/Hann/Tukey anchors exact (588 renders)',flush=True);runs+=588
# All intermediate controls also match the validated experiment, including scheduling counts.
def sweep(case):
 t,value=case;x,xr=run('experiment','morph',value,t,3,1);y,yr=run('production','morph',value,t,3,1)
 assert x==y,('sweep',case)
 for k in ['forward','reverse','grain-bytes','part-bytes','track-bytes']:assert xr[k]==yr[k],(case,k)
 return yr
with concurrent.futures.ThreadPoolExecutor(max_workers=3)as pool:rows+=list(pool.map(sweep,[(t,v)for t in range(4)for v in range(128)]))
print('512 all-value sounding comparisons exact (1024 renders)',flush=True);runs+=1024
# Live and independent controls, all sources, actual pthread C1 ownership, every split.
def live(case):
 mode,t,src=case;x,xr=run('experiment',mode,16,t,src,1);y,yr=run('production',mode,16,t,src,1);z,zr=run('dual',mode,16,t,src,1)
 assert x==y==z,('independent/live/dual',case)
 return yr
with concurrent.futures.ThreadPoolExecutor(max_workers=3)as pool:rows+=list(pool.map(live,[(m,t,s)for m in ['independent','live']for t in range(4)for s in sources]))
print('56 live/independent cases match experiment and rotating C1 splits (168 renders)',flush=True);runs+=168
with(a.out/'audio.csv').open('w')as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
print(f'{runs} sanitized renders passed; {len(rows)} production measurements',flush=True)
