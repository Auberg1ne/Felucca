#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Native SysEx + actual browser codec gates; no fixture regeneration/tool installation."""
import argparse,os,pathlib,shlex,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--baseline-tree',type=pathlib.Path,required=True);p.add_argument('--e-tree',type=pathlib.Path);p.add_argument('--oracle-tree',type=pathlib.Path);p.add_argument('--regressions',action='store_true');p.add_argument('--out',type=pathlib.Path);a=p.parse_args()
base=a.baseline_tree.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=base,text=True).strip()=='fd7f6f036487c53af3d31df3f209b63d6d2d9e8d'
subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests','web'],cwd=base,check=True)
def run(args):return subprocess.run(args,cwd=root,check=True)
with tempfile.TemporaryDirectory(prefix='stable-editor-') as tmp:
 out=a.out.resolve() if a.out else pathlib.Path(tmp);out.mkdir(parents=True,exist_ok=True)
 flags=shlex.split(os.environ.get('CC','cc'))+['-O1','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-fsanitize=address,undefined','-fno-sanitize-recover=all','-Ibuild/gen','-Ifirmware/src']
 for name,source in [('native','tests/editor_stable_test.c'),('win','tests/grain_win_editor_test.c'),('presets','tests/upreset_test.c'),('grc-editor','tests/grc_editor_test.c'),('project-bridge','tests/project_downgrade_bridge_test.c'),('new-gateway','tests/grain_win_i3a_gateway.c'),('old-gateway',str(base/'tests/grain_win_i3a_gateway.c'))]:
  run(flags+[source,'-lm','-o',str(out/name)])
 if a.e_tree:
  et=a.e_tree.resolve()
  assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=et,text=True).strip()=='37187a52589f8f736172b4f77957b7eff4a47520'
  subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests','web'],cwd=et,check=True)
  run(flags+[str(et/'tests/grain_win_i3a_gateway.c'),'-lm','-o',str(out/'e-gateway')])
 for name in ['native','win','presets','grc-editor','project-bridge']:run([str(out/name)])
 run(['node','web/test_stable_protocol.mjs',str(out/'new-gateway'),str(out/'old-gateway')]+([str(out/'e-gateway')] if a.e_tree else []))
 run(['node','web/test_win_persistence.mjs'])
 if a.e_tree:
  fixtures=out/'actual-c100';fixtures.mkdir(exist_ok=True)
  run(flags+['tests/grc_persistence_test.c','-lm','-o',str(out/'grc-export')])
  run([str(out/'grc-export'),str(fixtures)])
  run(flags+['-DGRC_OLD_BRIDGE_SOURCE="'+str(et/'tests/project_downgrade_bridge_test.c')+'"','tests/grc_downgrade_oracle.c','-lm','-o',str(out/'grc-old-oracle')])
  run([str(out/'grc-old-oracle'),str(fixtures)])

 if a.regressions:
  if not a.oracle_tree:p.error('--regressions requires --oracle-tree at evidence checkpoint6062014')
  run(['python3','tests/run_grc_migration.py','--oracle-tree',str(a.oracle_tree.resolve()),'--pcm-oracle-tree',str(base),'--regressions','--out',str(out/'regressions')])
print('Stable editor foundation gates passed; legacy118 preserved; extended119 access and versioned persistence qualified separately')
