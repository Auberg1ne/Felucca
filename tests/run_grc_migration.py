#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Historical Step239/240A proof plus current ownership/NOR gates. No freeze/write mode."""
import argparse, hashlib, json, os, pathlib, re, shlex, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
base='0da978b6b356c7c2891151217337b7bdf42e920e'
p=argparse.ArgumentParser();p.add_argument('--out',type=pathlib.Path);p.add_argument('--regressions',action='store_true');p.add_argument('--pcm-oracle-tree',type=pathlib.Path);p.add_argument('--oracle-tree',type=pathlib.Path,help='clean evidence checkpoint6062014 for historical unsafe downgrade oracles');a=p.parse_args()
def run(args,**kw):return subprocess.run(args,cwd=root,check=True,**kw)
run(['git','merge-base','--is-ancestor',base,'HEAD'])
oracle=root
if a.oracle_tree:
 oracle=a.oracle_tree.resolve()
 assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=oracle,text=True).strip()=='6062014872f4f3e2bb9a247cf610066e46406524'
 subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests','web','tools','build.sh','build_coffee_grains.sh'],cwd=oracle,check=True)
else:
 run(['git','diff','--exit-code',base,'--','firmware','web','build.sh','build_coffee_grains.sh','tools'])
# Check both accepted-version registries, not numeric mentions in negative tests.
for registry in {root,oracle}:
 s=(registry/'firmware/src/project.c').read_text()
 versions={int(k):int(v,16) for k,v in re.findall(r'^#define PROJ_MAGIC_V(\d+)\s+(0x[0-9A-Fa-f]+)',s,re.M)}
 assert set(versions)==set(range(1,14) if registry==root else range(1,13)),versions
 assert (0x46554e44 in versions.values()) == (registry==root)
 u=(registry/'firmware/src/upreset.c').read_text()
 assert re.search(r'^#define UP_VER '+('8u' if registry==root else '6u'),u,re.M) and re.search(r'^#define UP_VER_GRID '+('9u' if registry==root else '7u'),u,re.M)
print('FUND13 and presets8/9 allocated only in current reader; frozen oracle confirms prior availability',flush=True)
with tempfile.TemporaryDirectory(prefix='grc-model-') as tmp:
 out=a.out.resolve() if a.out else pathlib.Path(tmp);out.mkdir(parents=True,exist_ok=True)
 before={str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in (root/'tests/fixtures').rglob('*.bin')}
 flags=['-O1','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-Ibuild/gen','-Ifirmware/src','-Itests']
 names=['grc_model_standalone','grc_migration_test','preset_bank_downgrade_feasibility_test','grc_win_compat_test','grc_persistence_test','grc_backup_safety_test']
 if (root/'tests/preset_bank_ownership_test.c').exists(): names.append('preset_bank_ownership_test')
 for name in names:
  source_tree=oracle if name in ('grc_migration_test','preset_bank_downgrade_feasibility_test') else root
  exe=str(out/name);run(shlex.split(os.environ.get('CC','cc'))+flags+[str(source_tree/f'tests/{name}.c'),'-lm','-o',exe])
  r=run([exe],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)
 if a.regressions:
  if not a.pcm_oracle_tree: p.error('--regressions requires --pcm-oracle-tree at fd7f6f0')
  run(['python3','tests/run_granular_quality.py','--sanitize','--oracle-tree',str(a.pcm_oracle_tree.resolve()),'--out',str(out/'granular')])
  name='grain_filter_transition_test';exe=str(out/name)
  run(shlex.split(os.environ.get('CC','cc'))+flags+[f'tests/{name}.c','-lm','-o',exe]);r=run([exe],capture_output=True,text=True)
  (out/(name+'.log')).write_text(r.stdout+r.stderr);print(r.stdout.splitlines()[-1],flush=True)
 assert before=={str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in (root/'tests/fixtures').rglob('*.bin')}
print('GRC model/oracles passed; all binary fixtures unchanged. Historical oracle retains damaged-bank downgrade counterexamples; this is not a claim about new-firmware protection.',flush=True)
