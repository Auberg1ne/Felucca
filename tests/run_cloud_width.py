#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import argparse,pathlib,subprocess,os,json,array,math,statistics
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--base-tree',required=True,type=pathlib.Path);p.add_argument('--out',required=True,type=pathlib.Path);a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True);os.chdir(root)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()=='c421084fdd0462b01116981fc9c362277af7b5f1'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware'],cwd=a.base_tree,check=True)
# Freestanding 32-bit ABI check using the actual structure declarations.
import re
source=(root/'firmware/src/eng_grain.c').read_text()
layout='typedef unsigned int uint32_t;typedef int int32_t;typedef unsigned short uint16_t;typedef short int16_t;typedef unsigned char uint8_t;typedef signed char int8_t;typedef struct smp_zone_t smp_zone_t;\n#define GR_NG 12\n#define GR_RB 128\n#define GR_NIDX 1075\n#define GR_MAXZ 32\n#define GR_POLY 3\n#define CTL 32\n#define FELUCCA_GRAIN_CLOUD_WIDTH 1\n#define CW_A 31\n#define CW_B 79\n'
layout+=re.search(r'typedef struct \{[^{}]*\} cloud_width_t;', (root/'firmware/src/cloud_width.h').read_text()).group()+'\n'
for name in ['gr_grain_t','gr_head_t','gr_drift_t','gr_part_t']:
 layout+=re.search(r'typedef struct \{[^{}]*\} '+name+r';',source).group()+'\n'
layout+='_Static_assert(sizeof(void*)==4,"pointer");_Static_assert(sizeof(gr_grain_t)==56,"grain");_Static_assert(sizeof(gr_part_t)==8060,"part");_Static_assert(sizeof(gr_part_t)<=12888,"reservation");\n'
(out/'layout32.c').write_text(layout)
subprocess.run(['cc','-m32','-fsyntax-only',str(out/'layout32.c')],check=True)
f=['cc','-O1','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src','-Itests','-fsanitize=address,undefined','-fno-sanitize-recover=all']
subprocess.run(f+['tests/cloud_width_test.c','-lm','-o',str(out/'strict')],check=True);subprocess.run([str(out/'strict')],check=True)
s=(root/'tests/granular_quality_test.c').read_text().replace('t->p[P_PAN]=t->p[P_SPRD]=0;','t->p[P_PAN]=getenv("TEST_PAN")?atoi(getenv("TEST_PAN")):0;t->p[P_SPRD]=getenv("TEST_SPRD")?atoi(getenv("TEST_SPRD")):0;')
s=s.replace('  int32_t out[CTL*2];mix_block(out,CTL);','  if(getenv("C_RAPID"))for(unsigned ch=0;ch<4;ch++)trk[ch].p[P_GRC]=b<512?((b/4)%2?100:0):0;\n  int32_t out[CTL*2];mix_block(out,CTL);')
s=s.replace('  gr_part_t*g=gr_of(t);',"""  if(getenv("TEST_FX")){t->p[P_DIST]=40;t->p[P_ITYPE]=1;t->p[P_IMIX]=80;t->p[P_CHOR]=60;t->p[P_DLY]=60;t->p[P_REV]=60;fx_lowcut=2;song.g[G_RTYPE]=0;}
  if(getenv("TEST_SLICER")){t->p[P_SLCR]=atoi(getenv("TEST_SLICER"));t->p[P_SLDEPTH]=127;}
  gr_part_t*g=gr_of(t);""",1)
(out/'pcm.c').write_text(s)
for name,extra in [('base',[f'-DGRANULAR_QUALITY_UI_SOURCE="{a.base_tree.resolve()}/tests/ui_test.c"']),('new',[])]:
 subprocess.run(f+['-fno-sanitize=signed-integer-overflow,shift','-DGRANULAR_QUALITY_LEGACY_SHORT_PROBE=1']+extra+[str(out/'pcm.c'),'-lm','-o',str(out/name)],check=True)
records={}
def render(binary,kind,case,c=0,source=None,**env):
 key=f'{binary}-{kind}-{case}-{c}-{source}-{env}';path=out/(str(len(records))+'.pcm')
 settings={**os.environ,'GRC_VALUE':str(c),**{k:str(v) for k,v in env.items()}}
 if source is not None:settings['GRAIN_SOURCE']=str(source)
 r=subprocess.run([str(out/binary),str(kind),str(case),str(path)],env=settings,capture_output=True,text=True)
 assert r.returncode==0,(r.stdout,r.stderr)
 d=json.loads(r.stdout);data=path.read_bytes();x=array.array('i');x.frombytes(data);l=x[::2];right=x[1::2];ll=sum(i*i for i in l);rr=sum(i*i for i in right);lr=sum(i*j for i,j in zip(l,right))
 d['correlation']=lr/math.sqrt(ll*rr) if ll and rr else None;d['side_rms']=math.sqrt(sum((i-j)**2 for i,j in zip(l,right))/len(l));records[key]=d
 assert d['peak']>0 and d['rms']>0.1 and d['peak']<=32767
 return d,data
identities=0
for kind in range(4):
 for case in range(6):
  d,b=render('base',kind,case);e,c=render('new',kind,case);assert d==e and b==c;identities+=1
for kind in [1,2,3]:
 for case in range(5):
  for amount in [25,50,100]:
   d,b=render('base',kind,case,amount);e,c=render('new',kind,case,amount);assert d==e and b==c;identities+=1
print(identities,'fresh C0/positive non-CLOUD exact PCM/state pairs',flush=True)
for source in [0,2,3,13,14]:
 for amount in [0,25,50,100]:
  d,b=render('new',0,1,amount,source);e,c=render('new',0,1,amount,source);assert d==e and b==c
  baseline,_=render('base',0,1,0,source)
  for field in ['state_fnv64','rng_draws','admitted','reverse_admitted','max_load_q16','max_active','filter_changes']:assert d[field]==baseline[field],field
  if amount:assert b!=render('new',0,1,0,source)[1] and d['side_rms']>1
for pan in [-64,0,63]:
 for spread in [0,127]:
  render('new',0,5,100,TEST_PAN=pan,TEST_SPRD=spread)
  d,b=render('base',0,5,0,TEST_PAN=pan,TEST_SPRD=spread)
  e,c=render('new',0,5,0,TEST_PAN=pan,TEST_SPRD=spread)
  assert d==e and b==c
render('new',0,5,100,C_RAPID=1)
for slicer in [0,1,2]:
 render('new',0,5,100,C_RAPID=1,TEST_FX=1,TEST_SLICER=slicer,TEST_SPRD=127)
 d,b=render('base',0,5,0,TEST_FX=1,TEST_SLICER=slicer,TEST_SPRD=127)
 e,c=render('new',0,5,0,TEST_FX=1,TEST_SLICER=slicer,TEST_SPRD=127)
 assert d==e and b==c

(out/'audio-metrics.json').write_text(json.dumps(records,indent=2)+'\n')
print('factory C0/25/50/100 deterministic, unchanged RNG/grain/decoder hashes; PAN/V-SPRD/four-track/rapid changes pass',flush=True)

source=(root/'tests/dualcore_test.c').read_text().replace('    t->p[P_ITYPE] =','    if(e==ENGI_GRAIN){t->p[P_GRC]=100;t->p[P_GTYPE]=0;}\n    t->p[P_ITYPE] =',1)
(out/'dualcore.c').write_text(source)
subprocess.run(f+['-fno-sanitize=signed-integer-overflow,shift','-DFELUCCA_GRAIN_ONLY=0',str(out/'dualcore.c'),'-lm','-pthread','-o',str(out/'dualcore')],check=True)
subprocess.run([str(out/'dualcore')],env={**os.environ,'SCENE':'GRAIN'},check=True)
