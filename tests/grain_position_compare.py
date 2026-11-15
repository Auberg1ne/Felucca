#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Classify intended F2/F3/F6 changes and require identity in unaffected controls."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
import re
from pathlib import Path
import shlex
import subprocess

BASE='fc0fc15dac1f11d4b17a9196b42a282676cb51eb'
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--base-tree',required=True,type=Path)
p.add_argument('--out',required=True,type=Path)
a=p.parse_args();a.base_tree=a.base_tree.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.base_tree,text=True).strip()==BASE
subprocess.run(['git','diff','--exit-code',BASE,'--','firmware','tests','web'],cwd=a.base_tree,check=True)

def build(item):
    name,source,tree=item
    macro='GRID_UI_SOURCE' if source=='grain_only_navigation_test' else 'POSITION_UI_SOURCE'
    cmd=shlex.split(os.environ.get('CC','cc'))+['-O1','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src',f'-D{macro}="{tree}/tests/ui_test.c"',f'tests/{source}.c','-lm','-o',str(a.out/name)]
    with (a.out/(name+'-build.log')).open('w') as log:subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=log,check=True)
with ThreadPoolExecutor(max_workers=3) as pool:
    list(pool.map(build,[(n,s,t) for n,s,t in [('audio','grain_position_audio_test',ROOT),('base','grain_position_audio_test',a.base_tree),('nav','grain_only_navigation_test',ROOT),('nav-base','grain_only_navigation_test',a.base_tree)]]))

def run(name,args):
    r=subprocess.run([str(a.out/name),*map(str,args)],cwd=ROOT,capture_output=True,text=True)
    with (a.out/(name+'-run.log')).open('a') as log:log.write(r.stdout+r.stderr)
    assert r.returncode==0,(name,args,r.stdout,r.stderr)
    return r.stdout.strip()

records=[]
def compare(kind,source,pos,stretch,mode,tracks=1,expect=None):
    args=[kind,source,pos,stretch,mode,tracks]
    old=run('base',args+[a.out/'old.pcm']);new=run('audio',args+[a.out/'new.pcm'])
    b=(a.out/'old.pcm').read_bytes();c=(a.out/'new.pcm').read_bytes()
    assert any(b) and any(c),'silent captures cannot establish identity'
    same=b==c
    if expect is not None:assert same==expect,(args,'unexpected equality/difference',old,new)
    records.append(dict(type=kind,source=source,pos=pos,stretch=stretch,mode=mode,tracks=tracks,
                        identical=same,bytes=len(c),base_sha=hashlib.sha256(b).hexdigest(),fix_sha=hashlib.sha256(c).hexdigest(),base=old,fix=new))

for kind in range(4):
    for source in ('short','medium'):
        for pos in (0,64):
            for stretch in (0,1,64,127):
                compare(kind,source,pos,stretch,'control',expect=True)
                compare(kind,source,pos,stretch,'static') # retain the mixed/reverse boundary cases
    # Isolate live positioning and exact fitting without the old long-head overflow.
    compare(kind,'medium',0,127,'pos',expect=False)
    compare(kind,'medium',64,127,'shape',expect=False)
    compare(kind,'fractional',64,0,'static',expect=False)
    for source in (0,2,3,13,14):
        for pos in (0,64,127):
            for stretch in (0,1,64,127):compare(kind,source,pos,stretch,'static')
        compare(kind,source,0,127,'pos')
        compare(kind,source,64,0,'shape')
        compare(kind,source,64,0,'source')
        compare(kind,source,64,0,'static',tracks=4)
        # Identical fresh processes must render identical four-part audio, not just valid signal.
        run('audio',[kind,source,64,0,'static',4,a.out/'repeat.pcm'])
        assert (a.out/'new.pcm').read_bytes()==(a.out/'repeat.pcm').read_bytes(), 'nondeterministic playback'
        compare(kind,source,0,0,'traverse',tracks=4,expect=False)
        trace=records[-1]['fix']
        length=int(re.search(r'zone-frames=(\d+)',trace)[1])
        maximum=int(re.search(r'head-max=(\d+)',trace)[1])
        wraps=int(re.search(r'wraps=(\d+)',trace)[1])
        assert length-35<=maximum<=length-4 and wraps>=2,(source,trace)
    print(f'personality {kind}: factory/user/static/live/source/four-track matrix passed',flush=True)
compare(1,'medium',0,0,'freeze',expect=False)
for name in ('nav-base','nav'):print(run(name,[a.out/(name+'.bin')]),flush=True)
assert (a.out/'nav-base.bin').read_bytes()==(a.out/'nav.bin').read_bytes()
(a.out/'comparisons.json').write_text(json.dumps(records,indent=2)+'\n')
print('captures',len(records),'identical',sum(r['identical'] for r in records),'intentional differences',sum(not r['identical'] for r in records),'UI byte-identical',flush=True)
