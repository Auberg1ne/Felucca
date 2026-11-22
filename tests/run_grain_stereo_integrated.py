#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import pathlib,subprocess,os,json,argparse,re
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--base-tree',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args()
os.chdir(root);out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()=='b4442c5c1a018adc69fbd7d1bf5f2418360e00a7'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=a.base_tree,check=True)
s=(root/'tests/granular_quality_test.c').read_text().replace('t->p[P_PAN]=t->p[P_SPRD]=0;','t->p[P_PAN]=getenv("TEST_PAN")?atoi(getenv("TEST_PAN")):0;t->p[P_SPRD]=getenv("TEST_SPRD")?atoi(getenv("TEST_SPRD")):0;')
s=s.replace('mix_block(out,CTL);', 'mix_block(out,CTL);\n#ifdef TEST_STEREO_TRANSPORT\nfor(unsigned ch=0;ch<4;ch++){grain_stereo_view_t view=track_grain_stereo(&trk[ch],CTL);REQUIRE(view.frames==CTL);for(unsigned g=0;g<2;g++)for(unsigned i=0;i<CTL;i++)REQUIRE(view.group[g][i]==0); }\n#endif')
s=s.replace('host_preset(t,ENGI_GRAIN,0);','host_preset(t,ENGI_GRAIN,0);\n#if FELUCCA_GRAIN_STEREO_PROTOTYPE && !FELUCCA_GRAIN_STEREO_INTEGRATION\ngr_test_spread[p]=100;\n#endif')
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
integrated_layout=layout[:layout.index('typedef struct {')].replace('#define FELUCCA_GRAIN_STEREO_PROTOTYPE 1','#define FELUCCA_GRAIN_STEREO_PROTOTYPE 1\n#define FELUCCA_GRAIN_STEREO_INTEGRATION 1')
for name in ['gr_lr_lane_t','gr_lr_voice_t','gr_grain_t','gr_head_t','gr_drift_t','gr_part_t']:
 integrated_layout+=re.search(r'typedef struct \{[^{}]*\} '+name+r';',source).group()+'\n'
integrated_layout+='_Static_assert(sizeof(gr_part_t)==8376,"integrated part");_Static_assert(sizeof(gr_grain_t)==56,"grain");_Static_assert(sizeof(gr_part_t)<=12888,"reservation");\n'
(out/'integrated32.c').write_text(integrated_layout);subprocess.run(['cc','-m32','-fsyntax-only',str(out/'integrated32.c')],check=True)
flags=['cc','-O1','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-sanitize=signed-integer-overflow,shift','-DGRANULAR_QUALITY_LEGACY_SHORT_PROBE=1','-Ibuild/gen','-Ifirmware/src','-Itests']
for label,tree in [('base',str(a.base_tree.resolve())),('new',str(root)),('probe',str(root)),('integrated',str(root))]:
 subprocess.run(flags+(['-DFELUCCA_GRAIN_STEREO_INTEGRATION=1'] if label=='integrated' else [])+(['-DFELUCCA_GRAIN_STEREO_PROTOTYPE=1'] if label in ['probe','integrated'] else [])+(['-DTEST_STEREO_TRANSPORT=1'] if label!='base' else [])+[f'-DGRANULAR_QUALITY_UI_SOURCE="{tree}/tests/ui_test.c"',str(out/'pcm.c'),'-lm','-o',str(out/label)],check=True)
count=0
for kind in range(4):
 for scenario in range(6):
  for c in [0,25,50,100]:
   results=[]
   for label in ['base','new','probe','integrated']:
    r=subprocess.run([str(out/label),str(kind),str(scenario),str(out/(label+'.pcm'))],env={**os.environ,'GRC_VALUE':str(c)},capture_output=True,check=True)
    results.append((r.stdout,(out/(label+'.pcm')).read_bytes()))
   assert results[0]==results[1]==results[2]==results[3],(kind,scenario,c)
   count+=1
print(count,'C0/positive-C PCM and state pairs exact',flush=True)
for pan in [-64,0,63]:
 for spread in [0,64,127]:
  results=[]
  for label in ['base','new','probe','integrated']:
   r=subprocess.run([str(out/label),'0','5',str(out/(label+'.pcm'))],env={**os.environ,'GRC_VALUE':'100','TEST_PAN':str(pan),'TEST_SPRD':str(spread)},capture_output=True,check=True)
   results.append((r.stdout,(out/(label+'.pcm')).read_bytes()))
  assert results[0]==results[1]==results[2]==results[3],(pan,spread)
  count+=1
print(count,'four-way exact PCM/state cases including PAN/V-SPRD',flush=True)

s=(root/'tests/granular_quality_test.c').read_text()
s=s.replace('t->p[P_PAN]=t->p[P_SPRD]=0;', '''t->p[P_PAN]=getenv("TEST_PAN")?atoi(getenv("TEST_PAN")):0;t->p[P_SPRD]=getenv("TEST_SPRD")?atoi(getenv("TEST_SPRD")):0;
#if FELUCCA_GRAIN_STEREO_INTEGRATION
gr_test_spread[p]=getenv("STEREO_WIDTH")?atoi(getenv("STEREO_WIDTH")):0;
#endif''')
s=s.replace('  gr_part_t*g=gr_of(t);','''  if(getenv("TEST_FX")){t->p[P_DIST]=40;t->p[P_ITYPE]=1;t->p[P_IMIX]=80;t->p[P_CHOR]=60;t->p[P_DLY]=60;t->p[P_REV]=60;fx_lowcut=2;song.g[G_RTYPE]=0;}
  if(getenv("TEST_SLICER")){t->p[P_SLCR]=atoi(getenv("TEST_SLICER"));t->p[P_SLDEPTH]=127;}
  gr_part_t*g=gr_of(t);''',1)
s=s.replace('  int32_t out[CTL*2];mix_block(out,CTL);','''
#if FELUCCA_GRAIN_STEREO_INTEGRATION
  if(getenv("STEREO_LIVE"))for(unsigned ch=0;ch<4;ch++)gr_test_spread[ch]=b>=256&&b<512?100:0;
#endif
  int32_t out[CTL*2];mix_block(out,CTL);''')
s=s.replace('  int32_t out[CTL*2];mix_block(out,CTL);', '\n#if FELUCCA_GRAIN_STEREO_INTEGRATION\n  if(getenv("HARDPAN"))for(unsigned ch=0;ch<4;ch++)for(unsigned g=0;g<GR_NG;g++)gr_of(&trk[ch])->pan[g]=atoi(getenv("HARDPAN"));\n#endif\n  int32_t out[CTL*2];mix_block(out,CTL);')
s=s.replace('  int32_t out[CTL*2];mix_block(out,CTL);','  if(getenv("TAIL_ONLY")&&b==768)for(unsigned ch=0;ch<4;ch++)trk_note_off(&trk[ch],60+2*ch);\n  int32_t out[CTL*2];mix_block(out,CTL);\n  if(getenv("TAIL_ONLY")&&b==2047){unsigned nonzero=0;for(unsigned j=0;j<sizeof rev_comb/sizeof *rev_comb;j++)nonzero+=rev_comb[j]!=0;REQUIRE(nonzero>0);}')
s=s.replace('  int32_t out[CTL*2];mix_block(out,CTL);', r"""
  __typeof__(fx) room_before=fx;
  static int16_t room_comb_before[sizeof rev_comb/sizeof *rev_comb],room_comb_actual[sizeof rev_comb/sizeof *rev_comb];
  static unsigned char room_ap_before[sizeof rev_u],room_ap_actual[sizeof rev_u];
  unsigned room_check=getenv("TEST_FX")&&getenv("STEREO_LIVE")&&b>=500&&b<=800;
  if(room_check){memcpy(room_comb_before,rev_comb,sizeof rev_comb);memcpy(room_ap_before,&rev_u,sizeof rev_u);REQUIRE(song.g[G_RTYPE]==0&&fx.rtype==0);}
  int32_t out[CTL*2];mix_block(out,CTL);
  if(room_check){
   __typeof__(fx) actual=fx;memcpy(room_comb_actual,rev_comb,sizeof rev_comb);memcpy(room_ap_actual,&rev_u,sizeof rev_u);
   fx=room_before;memcpy(rev_comb,room_comb_before,sizeof rev_comb);memcpy(&rev_u,room_ap_before,sizeof rev_u);
   int32_t reference[CTL]={0};rev_room(send_r,reference,CTL);
   REQUIRE(!memcmp(room_comb_actual,rev_comb,sizeof rev_comb)&&!memcmp(room_ap_actual,&rev_u,sizeof rev_u));
   REQUIRE(!memcmp(fx.comb_i,actual.comb_i,sizeof fx.comb_i)&&!memcmp(fx.ap_i,actual.ap_i,sizeof fx.ap_i)&&!memcmp(fx.comb_lp,actual.comb_lp,sizeof fx.comb_lp));
   fx=actual;
  }
""")
s=s.replace('t->p[P_LEVEL]=100;','t->p[P_LEVEL]=getenv("TEST_LEVEL")?atoi(getenv("TEST_LEVEL")):100;')
s=s.replace('  int32_t out[CTL*2];mix_block(out,CTL);','  if(getenv("TEST_MUTE")){perf_kill=0;perf_held=b>=256&&b<512?PF_MUTE:0;}\n  int32_t out[CTL*2];mix_block(out,CTL);\n  if(getenv("TEST_MUTE")&&b>=300&&b<500)for(unsigned ch=0;ch<4;ch++){REQUIRE(pf.mg[ch]==0);for(unsigned i=0;i<2*CTL;i++)REQUIRE(!((int32_t*)gr_of(&trk[ch])->stereo_side)[i]);}\n  if(getenv("TEST_LEVEL")&&atoi(getenv("TEST_LEVEL"))==127&&b==1000)REQUIRE(lim_env>LIM_T);')
(out/'active.c').write_text(s)

subprocess.run(strict+['tests/grain_stereo_integrated_test.c','-lm','-o',str(out/'strict-integrated')],check=True)
subprocess.run([str(out/'strict-integrated')],check=True)
subprocess.run(flags+['-DFELUCCA_GRAIN_STEREO_PROTOTYPE=1','-DFELUCCA_GRAIN_STEREO_INTEGRATION=1',str(out/'active.c'),'-lm','-o',str(out/'active')],check=True)
import array,math
records={}
def render(key,kind,case,**settings):
 path=out/(key+'.pcm')
 r=subprocess.run([str(out/'active'),str(kind),str(case),str(path)],env={**os.environ,**{k:str(v) for k,v in settings.items()}},capture_output=True,text=True,check=True)
 d=json.loads(r.stdout);data=path.read_bytes();x=array.array('i');x.frombytes(data)
 assert d['rms']>0 and 0<d['peak']<=32767
 d['separation_rms']=math.sqrt(sum((a-b)**2 for a,b in zip(x[::2],x[1::2]))/(len(x)//2))
 records[key]=d
 return d,data
subprocess.run(flags+[f'-DGRANULAR_QUALITY_UI_SOURCE="{a.base_tree.resolve()}/tests/ui_test.c"',str(out/'active.c'),'-lm','-o',str(out/'active-legacy')],check=True)
for kind in range(4):
 for slicer in [0,1,2]:
  settings={'STEREO_WIDTH':'0','TEST_FX':'1','TEST_SLICER':str(slicer),'GRC_VALUE':'100'}
  result=[]
  for binary in ['active-legacy','active']:
   path=out/f'legacyFX-{kind}-{slicer}-{binary}.pcm'
   r=subprocess.run([str(out/binary),str(kind),'5',str(path)],env={**os.environ,**settings},capture_output=True,text=True,check=True)
   result.append((r.stdout,path.read_bytes()))
  assert result[0]==result[1],(kind,slicer)
print('12 default-off positive-C mixed FX/slicer pairs exact',flush=True)
for source in [0,2,3,13,14]:
 for pan in [-64,0,63]:
  for spread in [0,127]:
   key=f's{source}-p{pan}-v{spread}'
   settings=dict(GRAIN_SOURCE=source,STEREO_WIDTH=100,TEST_PAN=pan,TEST_SPRD=spread)
   d,b=render(key,0,1,**settings)
   if pan==0:assert d['separation_rms']>20
   again,data=render(key+'-repeat',0,1,**settings);assert b==data and d==again
   reference,_=render(key+'-mono',0,1,**{**settings,'STEREO_WIDTH':0})
   for field in ['state_fnv64','rng_draws','admitted','reverse_admitted','max_load_q16','max_active','filter_changes']:assert d[field]==reference[field],(key,field)
for pan in [-64,64]:
 d,data=render(f'hard{pan}',0,1,STEREO_WIDTH=100,HARDPAN=pan)
 x=array.array('i');x.frombytes(data);silent=x[1::2] if pan<0 else x[::2];assert max(abs(i) for i in silent)==0
for fx in [0,1]:
 for slicer in [0,1,2]:
  settings=dict(STEREO_WIDTH=100,STEREO_LIVE=1,TEST_SPRD=127)
  if fx:settings['TEST_FX']=1
  if slicer:settings['TEST_SLICER']=slicer
  render(f'fx{fx}-sl{slicer}',0,5,**settings)
render('ROOM-tail',0,5,STEREO_WIDTH=100,STEREO_LIVE=1,TEST_FX=1,TAIL_ONLY=1)
render('level40',0,5,STEREO_WIDTH=100,TEST_LEVEL=40)
render('limiter-level127',0,5,STEREO_WIDTH=100,TEST_LEVEL=127)
render('mute-live',0,5,STEREO_WIDTH=100,TEST_MUTE=1)
(out/'channel-results.json').write_text(json.dumps(records,indent=2)+'\n')
print('30 factory/PAN/V-SPRD pairs repeatable and match mono state; hard pans silent opposite; six four-track effects/slicer/live cases and ROOM tail pass',flush=True)

# Actual threaded CPU2 routing, diagnostic multi-engine scenes including CLOUD.
source=(root/'tests/dualcore_test.c').read_text().replace('    t->p[P_ITYPE] =','    if(e==ENGI_GRAIN){gr_test_spread[k]=100;t->p[P_GTYPE]=0;}\n    t->p[P_ITYPE] =',1)
(out/'dualcore.c').write_text(source)
subprocess.run(flags+['-DFELUCCA_GRAIN_ONLY=0','-DFELUCCA_GRAIN_STEREO_PROTOTYPE=1','-DFELUCCA_GRAIN_STEREO_INTEGRATION=1',str(out/'dualcore.c'),'-lm','-pthread','-o',str(out/'dualcore')],check=True)
subprocess.run([str(out/'dualcore')],env={**os.environ,'SCENE':'GRAIN'},check=True)
# Optimized host resources; no sanitizer instrumentation in timing/stack probes.
resource=out/'resources';resource.mkdir(exist_ok=True)
rf=['cc','-O2','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-fstack-usage','-Ibuild/gen','-Ifirmware/src','-Itests']
for name,extra in [('base',[f'-DGRANULAR_QUALITY_UI_SOURCE="{a.base_tree.resolve()}/tests/ui_test.c"']),('default',[]),('enabled',['-DFELUCCA_GRAIN_STEREO_PROTOTYPE=1','-DFELUCCA_GRAIN_STEREO_INTEGRATION=1'])]:
 subprocess.run(rf+extra+[str(out/'active.c'),'-lm','-o',str(resource/name)],check=True)
subprocess.run(['size']+[str(resource/n) for n in ['base','default','enabled']],check=True)
subprocess.run(rf+['tests/grain_stereo_integrated_bench.c','-lm','-pthread','-o',str(resource/'bench')],check=True)
import statistics
measurements={}
for tracks in [1,4]:
 for stage in [0,1,3,4,5,6,7,8,9,10,11]:
  runs=[subprocess.check_output([str(resource/'bench'),str(stage),str(tracks)],text=True).split() for _ in range(3)]
  assert len({r[1] for r in runs})==1
  measurements[f'{tracks}-{stage}']={'median_ns_per_frame':statistics.median(float(r[0]) for r in runs),'hash':runs[0][1],'worker_stack_highwater':max(int(r[2]) for r in runs),'observed_grains_per_part':[int(v) for v in runs[0][3:7]]}
(resource/'measurements.json').write_text(json.dumps(measurements,indent=2)+'\n');print(json.dumps(measurements,indent=2))
