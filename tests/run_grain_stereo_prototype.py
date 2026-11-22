#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import pathlib,subprocess,os,json,argparse,re
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--base-tree',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args()
os.chdir(root);out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()=='e43141e44f9985e4c27bcb09ce47bdfed4193ea1'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=a.base_tree,check=True)
s=(root/'tests/granular_quality_test.c').read_text().replace('t->p[P_PAN]=t->p[P_SPRD]=0;','t->p[P_PAN]=getenv("TEST_PAN")?atoi(getenv("TEST_PAN")):0;t->p[P_SPRD]=getenv("TEST_SPRD")?atoi(getenv("TEST_SPRD")):0;')
s=s.replace('mix_block(out,CTL);', 'mix_block(out,CTL);\n#ifdef TEST_STEREO_TRANSPORT\nfor(unsigned ch=0;ch<4;ch++){grain_stereo_view_t view=track_grain_stereo(&trk[ch],CTL);REQUIRE(view.frames==CTL);for(unsigned g=0;g<2;g++)for(unsigned i=0;i<CTL;i++)REQUIRE(view.group[g][i]==0); }\n#endif')
s=s.replace('host_preset(t,ENGI_GRAIN,0);','host_preset(t,ENGI_GRAIN,0);\n#if FELUCCA_GRAIN_STEREO_PROTOTYPE\ngr_test_spread[p]=100;\n#endif')
(out/'pcm.c').write_text(s)
strict=['cc','-O1','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-fsanitize=address,undefined','-fno-sanitize-recover=all','-Ibuild/gen','-Ifirmware/src']
subprocess.run(strict+['tests/grain_stereo_prototype_test.c','-lm','-o',str(out/'kernel')],check=True)
subprocess.run([str(out/'kernel')],check=True)
# Verify the actual structure declarations with a freestanding 32-bit host layout.
source=(root/'firmware/src/eng_grain.c').read_text()
layout='typedef unsigned int uint32_t;typedef int int32_t;typedef unsigned short uint16_t;typedef short int16_t;typedef unsigned char uint8_t;typedef signed char int8_t;typedef struct smp_zone_t smp_zone_t;\n#define GR_NG 12\n#define GR_RB 128\n#define GR_NIDX 1075\n#define GR_MAXZ 32\n#define GR_POLY 3\n#define CTL 32\n#define FELUCCA_GRAIN_STEREO_PROTOTYPE 1\n'
for name in ['gr_grain_t','gr_head_t','gr_drift_t','gr_part_t']:
 layout+=re.search(r'typedef struct \{[^{}]*\} '+name+r';',source).group()+'\n'
layout+='_Static_assert(sizeof(void*)==4,"pointer");_Static_assert(sizeof(gr_grain_t)==56,"grain");_Static_assert(sizeof(gr_part_t)==7496,"part");_Static_assert(sizeof(gr_part_t)<=12888,"reservation");\n'
(out/'layout32.c').write_text(layout)
subprocess.run(['cc','-m32','-fsyntax-only',str(out/'layout32.c')],check=True)
flags=['cc','-O1','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-sanitize=signed-integer-overflow,shift','-DGRANULAR_QUALITY_LEGACY_SHORT_PROBE=1','-Ibuild/gen','-Ifirmware/src','-Itests']
for label,tree in [('base',str(a.base_tree.resolve())),('new',str(root)),('probe',str(root))]:
 subprocess.run(flags+(['-DFELUCCA_GRAIN_STEREO_PROTOTYPE=1'] if label=='probe' else [])+(['-DTEST_STEREO_TRANSPORT=1'] if label!='base' else [])+[f'-DGRANULAR_QUALITY_UI_SOURCE="{tree}/tests/ui_test.c"',str(out/'pcm.c'),'-lm','-o',str(out/label)],check=True)
count=0
for kind in range(4):
 for scenario in range(6):
  for c in [0,25,50,100]:
   results=[]
   for label in ['base','new','probe']:
    r=subprocess.run([str(out/label),str(kind),str(scenario),str(out/(label+'.pcm'))],env={**os.environ,'GRC_VALUE':str(c)},capture_output=True,check=True)
    results.append((r.stdout,(out/(label+'.pcm')).read_bytes()))
   assert results[0]==results[1]==results[2],(kind,scenario,c)
   count+=1
print(count,'C0/positive-C PCM and state pairs exact',flush=True)
for pan in [-64,0,63]:
 for spread in [0,64,127]:
  results=[]
  for label in ['base','new','probe']:
   r=subprocess.run([str(out/label),'0','5',str(out/(label+'.pcm'))],env={**os.environ,'GRC_VALUE':'100','TEST_PAN':str(pan),'TEST_SPRD':str(spread)},capture_output=True,check=True)
   results.append((r.stdout,(out/(label+'.pcm')).read_bytes()))
  assert results[0]==results[1]==results[2],(pan,spread)
  count+=1
print(count,'three-way exact PCM/state cases including PAN/V-SPRD',flush=True)
