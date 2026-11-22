#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import argparse,pathlib,subprocess,statistics,json
root=pathlib.Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--out',required=True,type=pathlib.Path);a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
s=(root/'tests/grain_sparkle_bench.c').read_text().replace('t->p[P_GTYPE]=type;','t->p[P_GTYPE]=type;t->p[P_VOICE]=V_POLY;')
s=s.replace('if(k<tracks)trk_note_on(t,48+2*k,100);','''
#if FELUCCA_GRAIN_STEREO_INTEGRATION
  gr_test_spread[k]=atoi(argv[1]);
#endif
  if(k<tracks)for(unsigned note=0;note<(tracks==1?3:2);note++)trk_note_on(t,24+2*k+note*7,100);''')
(out/'bench.c').write_text(s)
flags=['cc','-O2','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-fstack-usage','-Ibuild/gen','-Ifirmware/src','-Itests']
for mode,extra in [('cheap',[]),('expensive',['-DFELUCCA_GRAIN_STEREO_PROTOTYPE=1','-DFELUCCA_GRAIN_STEREO_INTEGRATION=1'])]:subprocess.run(flags+extra+[str(out/'bench.c'),'-lm','-o',str(out/mode)],cwd=root,check=True)
r={}
for tracks in [1,4]:
 for mode,c in [('cheap',0),('cheap',100),('expensive',100)]:
  runs=[subprocess.check_output([str(out/mode),str(c),str(tracks),'0'],text=True).split() for _ in range(3)]
  assert len({tuple(v[1:]) for v in runs})==1
  r[f'{tracks}-{mode}-{c}']={'ns_frame':statistics.median(float(v[0]) for v in runs),'peak':int(runs[0][1]),'hash':runs[0][2]}
(out/'results.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
