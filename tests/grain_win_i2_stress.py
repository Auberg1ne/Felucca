# SPDX-License-Identifier: GPL-3.0-only
import subprocess,concurrent.futures,os,shlex
from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--i1-tree',type=Path,required=True);p.add_argument('--experiment-tree',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];base=a.i1_tree.resolve();exp=a.experiment_tree.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
for tree,sha in [(base,'68f3e7a848b2410670945c07b6a66f3689ee88e3'),(exp,'4c34b6e38612ca13d71aee5a5cc3cd7d9da3b07d')]:
 assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=tree,text=True).strip()==sha
 subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=tree,check=True)

flags=shlex.split(os.environ.get('CC','cc'))+['-O1','-g','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src'];san=['-fsanitize=address,undefined','-fno-sanitize=shift,signed-integer-overflow,bounds,object-size,pointer-overflow','-fno-sanitize-recover=all']
specs=[('stress-new',root,['-DWIN_I2_PRODUCTION=1']),('stress-exp',exp,['-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1','-DFELUCCA_GRAIN_MORPH_EXPERIMENT=1']),('c1-refuse',root,['-DWIN_I2_PRODUCTION=1','-DC1_HOST=1','-DWIN_I2_C1_REFUSE=1']),('c1-wait-fail',root,['-DWIN_I2_PRODUCTION=1','-DC1_HOST=1','-DWIN_I2_C1_WAIT_FAIL=1'])]
def build(spec):
 name,tree,extra=spec
 with(out/(name+'.build.log')).open('w')as log:subprocess.run(flags+san+extra+[f'-DMORPH_UI_SOURCE="{tree}/tests/ui_test.c"','tests/grain_win_i2_audio_test.c','-lm','-pthread','-o',str(out/name)],cwd=root,stdout=log,stderr=log,check=True)
 print(name,'built',flush=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:list(pool.map(build,specs))
def run(exe,mode,val,t,src,sc,env):
 path=out/f'{exe}-{mode}-{val}-{t}-{src}-{sc}.pcm';r=subprocess.run([str(out/exe),mode,str(val),str(t),str(src),str(sc),str(path)],cwd=root,env={**os.environ,**env},capture_output=True,text=True);assert not r.returncode and not r.stderr,(exe,val,t,src,sc,r.stdout,r.stderr);data=path.read_bytes();path.unlink();return data,r.stdout
cases=[(v,t,s,sc,{'WIN_I2_PTCH':p,'WIN_I2_RAND':r,'WIN_I2_DENS':d})for v in [0,32,64,96,127]for t in range(4)for s in [13,'user']for sc,p,r,d in [(0,'-24','0','0'),(2,'24','127','127')]]
def stress(case):
 v,t,s,sc,env=case;x,_=run('stress-exp','morph',v,t,s,sc,env);y,log=run('stress-new','morph',v,t,s,sc,env);assert x==y,case;return log
with concurrent.futures.ThreadPoolExecutor(max_workers=3)as pool:logs=list(pool.map(stress,cases))
(out/'stress.log').write_text(''.join(logs));print('80 extreme-control comparisons passed, 160 sanitized renders',flush=True)
for t in range(4):
 for src in [0,2,3,13,14,'user','sine']:
  ref,_=run('stress-new','live',16,t,src,1,{})
  for exe in ['c1-refuse','c1-wait-fail']:
   got,_=run(exe,'live',16,t,src,1,{});assert got==ref,(exe,t,src)
print('28 live fallback cases, 84 sanitized renders: rejected jobs and unexecuted timed-out jobs exact',flush=True)
# Full non-DSP snapshot stream compares without masking any state.
for name,tree in [('ui-i1',base),('ui-new',root)]:
 with(out/(name+'.log')).open('w')as log:
  subprocess.run(flags+[f'-DGRID_UI_SOURCE="{tree}/tests/ui_test.c"','tests/grain_only_navigation_test.c','-lm','-o',str(out/name)],cwd=root,stdout=log,stderr=log,check=True)
  subprocess.run([str(out/name),str(out/(name+'.bin'))],cwd=root,stdout=log,stderr=log,check=True)
assert(out/'ui-i1.bin').read_bytes()==(out/'ui-new.bin').read_bytes()
print('225 retained UI frames and all captured navigation/parameter state byte-identical',flush=True)
