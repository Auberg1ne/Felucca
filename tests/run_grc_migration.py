#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test-only proof against the exact Step239 production revision. No freeze/write mode."""
import argparse, hashlib, json, os, pathlib, re, shlex, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
base='0da978b6b356c7c2891151217337b7bdf42e920e'
p=argparse.ArgumentParser();p.add_argument('--out',type=pathlib.Path);p.add_argument('--regressions',action='store_true');a=p.parse_args()
def run(args,**kw):return subprocess.run(args,cwd=root,check=True,**kw)
run(['git','merge-base','--is-ancestor',base,'HEAD'])
run(['git','diff','--exit-code',base,'--','firmware','web','build.sh','build_coffee_grains.sh','tools'])
# Check the actual accepted-version registry, not all numeric mentions: candidate
# numbers already occur as deliberately rejected historical test vectors.
s=(root/'firmware/src/project.c').read_text()
versions={int(k):int(v,16) for k,v in re.findall(r'^#define PROJ_MAGIC_V(\d+)\s+(0x[0-9A-Fa-f]+)',s,re.M)}
assert set(versions)==set(range(1,13)),versions
assert 0x46554e44 not in versions.values()
u=(root/'firmware/src/upreset.c').read_text()
assert re.search(r'^#define UP_VER 6u',u,re.M) and re.search(r'^#define UP_VER_GRID 7u',u,re.M)
print('Candidate project13/FUND and preset8/9 are unallocated in production; existing negative fixtures retained',flush=True)
with tempfile.TemporaryDirectory(prefix='grc-model-') as tmp:
 out=a.out.resolve() if a.out else pathlib.Path(tmp);out.mkdir(parents=True,exist_ok=True)
 before={str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in (root/'tests/fixtures').rglob('*.bin')}
 flags=['-O1','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-Ibuild/gen','-Ifirmware/src','-Itests']
 for name in ['grc_model_standalone','grc_migration_test','grain_win_persistence_test']:
  exe=str(out/name);run(shlex.split(os.environ.get('CC','cc'))+flags+[f'tests/{name}.c','-lm','-o',exe])
  r=run([exe],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)
 if a.regressions:
  run(['python3','tests/run_granular_quality.py','--sanitize','--out',str(out/'granular')])
  name='grain_filter_transition_test';exe=str(out/name)
  run(shlex.split(os.environ.get('CC','cc'))+flags+[f'tests/{name}.c','-lm','-o',exe]);r=run([exe],capture_output=True,text=True)
  (out/(name+'.log')).write_text(r.stdout+r.stderr);print(r.stdout.splitlines()[-1],flush=True)
 assert before=={str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in (root/'tests/fixtures').rglob('*.bin')}
print('GRC model/oracles passed; all binary fixtures unchanged. Known damaged-bank downgrade gaps remain blockers.',flush=True)
