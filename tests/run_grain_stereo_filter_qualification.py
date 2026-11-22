#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Isolated L/R model qualification; no firmware source or generated asset writes."""
import argparse,json,pathlib,subprocess,statistics,re
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
def run(cmd,**kw):return subprocess.run(cmd,cwd=root,check=True,**kw)
flags=['cc','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src']
run(flags+['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','tests/grain_stereo_filter_qualification_test.c','-lm','-o',str(out/'qualification')])
run([str(out/'qualification')])
# No target headers/libraries needed: fixed-width integers, no pointers in this state.
s=(root/'tests/grain_stereo_filter_qualification.h').read_text()
layout='typedef int int32_t;\n'+re.search(r'typedef struct \{[^{}]*\} q_lr_lane_t;',s).group()+'\n'+re.search(r'typedef struct \{[^{}]*\} q_lr_state_t;',s).group()
layout+='\n_Static_assert(sizeof(q_lr_state_t)==40,"LR state");_Static_assert(7496+3*sizeof(q_lr_state_t)+8<=12888,"part reservation");\n'
(out/'layout32.c').write_text(layout);run(['cc','-m32','-fsyntax-only',str(out/'layout32.c')])
for name,extra in [('bench',[]),('stack',['-fno-inline'])]:
 run(flags+['-O2','-fstack-usage']+extra+['tests/grain_stereo_filter_qualification_bench.c','-lm','-o',str(out/name)])
results={}
for tracks in [1,4]:
 for stereo in [0,1]:
  values=[run([str(out/'bench'),str(stereo),str(tracks)],capture_output=True,text=True).stdout.split() for _ in range(5)]
  assert len({v[1] for v in values})==1
  results[f'tracks{tracks}-stereo{stereo}']={'median_ns_per_output_frame':statistics.median(float(v[0]) for v in values),'hash':values[0][1]}
(out/'measurements.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))
