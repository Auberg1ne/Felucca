#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exact-base Hann identity, experimental Tukey matrix and listening captures."""
import argparse, concurrent.futures, json, os, re, shlex, struct, subprocess, wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--base-tree',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args()
a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);a.base_tree=a.base_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()=='d9e352f9380da85dedd0b37434a513fed5b6b636'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=a.base_tree,check=True)
cc=shlex.split(os.environ.get('CC','cc'));flags=['-O1','-g','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src']
san=['-fsanitize=address,undefined','-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow','-fno-sanitize-recover=undefined']
def build(name,source,extra):
 with (a.out/(name+'.build.log')).open('w') as f:subprocess.run(cc+flags+extra+[source,'-lm','-o',str(a.out/name)],cwd=ROOT,stdout=f,stderr=f,check=True)
build('base','tests/grain_window_audio_test.c',[f'-DWINDOW_UI_SOURCE="{a.base_tree}/tests/ui_test.c"']+san)
build('default','tests/grain_window_audio_test.c',san)
build('experiment','tests/grain_window_audio_test.c',['-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1']+san)
build('numeric','tests/grain_window_numeric_test.c',['-fsanitize=address,undefined','-fno-sanitize-recover=all'])
build('latch','tests/grain_window_latch_test.c',['-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1']+san)
build('kernel','tests/grain_window_kernel_test.c',['-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1','-fsanitize=address,undefined','-fno-sanitize-recover=all'])
subprocess.run(['python3','tools/gen_grain_tukey.py','--check'],cwd=ROOT,check=True)
for name in ['numeric','latch','kernel']:
 r=subprocess.run([str(a.out/name)],capture_output=True,text=True,check=True);(a.out/(name+'.log')).write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)
def run(case):
 t,src,scenario=case;results={};pcm={}
 for name,w in [('base',0),('default',0),('experiment',0),('experiment',1)]:
  key=name+str(w);path=a.out/f'{t}-{src}-{scenario}-{key}.pcm'
  r=subprocess.run([str(a.out/name),str(w),str(t),str(src),str(scenario),str(path)],cwd=ROOT,capture_output=True,text=True)
  assert r.returncode==0,(case,key,r.stdout,r.stderr)
  assert 'runtime error:' not in r.stderr and 'AddressSanitizer' not in r.stderr
  results[key]=r.stdout.strip();pcm[key]=path.read_bytes();path.unlink()
 assert pcm['base0']==pcm['default0']==pcm['experiment0'],('Hann identity',case)
 fields=[dict(re.findall(r'([\w-]+)=([^ ]+)',results[f'experiment{w}'])) for w in [0,1]]
 for field in ['forward','reverse','grain-bytes','part-bytes']:assert fields[0][field]==fields[1][field],('state/load changed',case,field)
 if src=='sine':assert pcm['experiment0']==pcm['experiment1'],('fallback changed',case)
 else:assert pcm['experiment0']!=pcm['experiment1'],('no window effect',case)
 if src==3 and scenario==1:
  for w in [0,1]:
   data=struct.unpack('<%di'%(len(pcm[f'experiment{w}'])//4),pcm[f'experiment{w}'])
   # Preserve actual mixer integers as PCM16; no gain, normalization or clipping.
   assert max(map(abs,data))<32768,('listening capture needs explicit scale decision',case,w)
   with wave.open(str(a.out/f'{["cloud","freeze","stutter","shimmer"][t]}-{["hann","tukey"][w]}.wav'),'wb') as f:
    f.setnchannels(2);f.setsampwidth(2);f.setframerate(44100);f.writeframes(struct.pack('<%dh'%len(data),*data))
 return {'type':t,'source':src,'scenario':scenario,'measurements':results}
cases=[(t,s,n) for t in range(4) for s in [0,2,3,13,14,'user','sine'] for n in range(3)]
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:rows=list(pool.map(run,cases))
(a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
print(f'{len(rows)} cases: base/default/experimental Hann exact; {len(rows)*4} sanitized renders; Tukey sounding, fallback unchanged',flush=True)
