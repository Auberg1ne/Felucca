#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Three-window comparisons: original Hann, previous Tukey, Gaussian and matched WAVs."""
import argparse, concurrent.futures, csv, hashlib, json, os, re, shlex, struct, subprocess, wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--base-tree',required=True,type=Path);p.add_argument('--legacy-tree',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args()
a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);a.base_tree=a.base_tree.resolve();a.legacy_tree=a.legacy_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()=='aeed25c6869a0735399b7c7adb494e7c3a9c439d'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=a.base_tree,check=True)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.legacy_tree,text=True).strip()=='d9e352f9380da85dedd0b37434a513fed5b6b636'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=a.legacy_tree,check=True)
cc=shlex.split(os.environ.get('CC','cc'));flags=['-O1','-g','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src']
san=['-fsanitize=address,undefined','-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow','-fno-sanitize-recover=undefined']
def build(name,source,extra):
 with (a.out/(name+'.build.log')).open('w') as f:subprocess.run(cc+flags+extra+[source,'-lm','-o',str(a.out/name)],cwd=ROOT,stdout=f,stderr=f,check=True)
build('legacy','tests/grain_window_audio_test.c',[f'-DWINDOW_UI_SOURCE="{a.legacy_tree}/tests/ui_test.c"']+san)
build('base','tests/grain_window_audio_test.c',[f'-DWINDOW_UI_SOURCE="{a.base_tree}/tests/ui_test.c"','-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1']+san)
build('default','tests/grain_window_audio_test.c',san)
build('experiment','tests/grain_window_audio_test.c',['-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1']+san)
build('numeric','tests/grain_window_numeric_test.c',['-fsanitize=address,undefined','-fno-sanitize-recover=all'])
build('gauss-numeric','tests/grain_gauss_numeric_test.c',['-fsanitize=address,undefined','-fno-sanitize-recover=all'])
build('latch','tests/grain_window_latch_test.c',['-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1']+san)
build('kernel','tests/grain_window_kernel_test.c',['-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1','-fsanitize=address,undefined','-fno-sanitize-recover=all'])
subprocess.run(['python3','tools/gen_grain_tukey.py','--check'],cwd=ROOT,check=True)
subprocess.run(['python3','tools/gen_grain_gauss.py','--check'],cwd=ROOT,check=True)
for name in ['numeric','gauss-numeric','latch','kernel']:
 r=subprocess.run([str(a.out/name)],capture_output=True,text=True,check=True);(a.out/(name+'.log')).write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)
for test in ['grain_position_test','grain_only_test']:
 wrapper=a.out/(test+'-gauss.c')
 source=(ROOT/'tests'/(test+'.c')).read_text()
 assert source.count('int main(void)')==1
 wrapper.write_text(source.replace('int main(void)','int retained_main(void)',1)+'\nint main(void){gr_test_window=2;return retained_main();}\n')
 build(test+'-gauss',str(wrapper),['-Itests','-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1']+san)
 r=subprocess.run([str(a.out/(test+'-gauss'))],capture_output=True,text=True,check=True)
 (a.out/(test+'-gauss.log')).write_text(r.stdout+r.stderr);print(r.stdout.splitlines()[-1],flush=True)
def run(case):
 t,src,scenario=case;results={};pcm={}
 for name,w in [('legacy',0),('base',0),('base',1),('default',0),('experiment',0),('experiment',1),('experiment',2)]:
  key=name+str(w);path=a.out/f'{t}-{src}-{scenario}-{key}.pcm'
  r=subprocess.run([str(a.out/name),str(w),str(t),str(src),str(scenario),str(path)],cwd=ROOT,capture_output=True,text=True)
  assert r.returncode==0,(case,key,r.stdout,r.stderr)
  assert 'runtime error:' not in r.stderr and 'AddressSanitizer' not in r.stderr
  results[key]=r.stdout.strip();pcm[key]=path.read_bytes();path.unlink()
 assert pcm['legacy0']==pcm['base0']==pcm['default0']==pcm['experiment0'],('Hann identity',case)
 assert pcm['base1']==pcm['experiment1'],('Tukey identity',case)
 fields=[dict(re.findall(r'([\w-]+)=([^ ]+)',results[f'experiment{w}'])) for w in [0,1,2]]
 for field in ['forward','reverse','grain-bytes','part-bytes','track-bytes']:assert fields[0][field]==fields[1][field]==fields[2][field],('state/load changed',case,field)
 if src=='sine':assert pcm['experiment0']==pcm['experiment1']==pcm['experiment2'],('fallback changed',case)
 else:assert len({pcm[f'experiment{w}'] for w in range(3)})==3,('no window effect',case)
 listening=None
 if src==3 and scenario==1:
  data=[struct.unpack('<%di'%(len(pcm[f'experiment{w}'])//4),pcm[f'experiment{w}']) for w in range(3)]
  rms=[(sum(x*x for x in v)/len(v))**.5 for v in data];target=min(32767*10**(-24/20),.8*min(rms))
  listening=[]
  for w in range(3):
   assert max(map(abs,data[w]))<32768,('raw capture needs explicit scale decision',case,w)
   gain=target/rms[w];assert 0<gain<=1
   listening.append(dict(window=w,gain=gain,raw_RMS=rms[w],target_RMS=target))
   for kind,v in [('raw',data[w]),('matched',[round(x*gain) for x in data[w]])]:
    assert max(map(abs,v))<32768
    with wave.open(str(a.out/f'{["cloud","freeze","stutter","shimmer"][t]}-{["hann","tukey","gauss"][w]}-{kind}.wav'),'wb') as f:
     f.setnchannels(2);f.setsampwidth(2);f.setframerate(44100);f.writeframes(struct.pack('<%dh'%len(v),*v))
 return {'type':t,'source':src,'scenario':scenario,'measurements':results,'listening':listening}
cases=[(t,s,n) for t in range(4) for s in [0,2,3,13,14,'user','sine'] for n in range(3)]
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:rows=list(pool.map(run,cases))
measurements=[]
for row in rows:
 for w in range(3):measurements.append(dict(re.findall(r'([\w-]+)=([^ ]+)',row['measurements'][f'experiment{w}'])))
with (a.out/'measurements.csv').open('w') as f:
 writer=csv.DictWriter(f,fieldnames=list(measurements[0]),lineterminator='\n');writer.writeheader();writer.writerows(measurements)
(a.out/'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in sorted(a.out.glob('*.wav'))))
(a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
print(f'{len(rows)} cases: base/default/experimental Hann exact; {len(rows)*7} sanitized renders; previous Tukey exact; Gaussian sounding, fallback unchanged',flush=True)
