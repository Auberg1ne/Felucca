#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read-only verification; --freeze is explicit, allowed only at the audited base.
PCM is signed int32 little-endian stereo, 44100 Hz, 65536 frames/case.
Generated captures/executables stay outside Git. No production hooks required.
"""
import argparse, hashlib, json, os, pathlib, shlex, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--freeze', action='store_true')
p.add_argument('--sanitize', action='store_true')
p.add_argument('--out', type=pathlib.Path)
p.add_argument('--oracle-tree',type=pathlib.Path,help='clean pre-C checkpoint for historical serialized-byte fixtures')
a = p.parse_args()
fixture = root/'tests/fixtures/granular-quality.json'
base = '49e7698c646b1b7395b4c7059fd1e8274ce67df4'
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def run(args, **kw): return subprocess.run(args, cwd=root, check=True, **kw)
if a.oracle_tree:
    assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.oracle_tree,text=True).strip()=='fd7f6f036487c53af3d31df3f209b63d6d2d9e8d'
    subprocess.run(['git','diff','--exit-code','HEAD','--','firmware','tests'],cwd=a.oracle_tree,check=True)
if a.freeze:
    assert subprocess.check_output(['git','rev-parse','HEAD'], cwd=root, text=True).strip() == base
    run(['git','diff','--exit-code',base,'--','firmware','web','build.sh','build_coffee_grains.sh'])
result = {'base':base, 'pcm':'s32le stereo 44100Hz; 65536 frames', 'cases':{}, 'inputs':{}}
# Pin generated decoder/table inputs and immutable historical fixture bytes too.
for pattern in ['build/gen/felucca_*.h','tests/fixtures/grain-win-i1r/*.bin','tests/fixtures/grain-win-i1/*.bin']:
    for path in sorted(root.glob(pattern)): result['inputs'][str(path.relative_to(root))] = sha(path)
with tempfile.TemporaryDirectory(prefix='grain-quality-') as tmp:
    out = a.out.resolve() if a.out else pathlib.Path(tmp)
    out.mkdir(parents=True, exist_ok=True)
    flags = ['-O1','-g','-w','-Wno-implicit-function-declaration','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src']
    if a.sanitize:
        # Existing mixer/ADPCM paths intentionally use signed wrap and shifts.
        # Bounds/object/pointer checks remain enabled in this new harness.
        flags += ['-fsanitize=address,undefined','-fno-sanitize=signed-integer-overflow,shift','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
    # The frozen short probe records the old defect. Keep it as a legacy
    # oracle while the new short-envelope gate verifies corrected rendering.
    if not a.freeze:
        flags += ['-DGRANULAR_QUALITY_LEGACY_SHORT_PROBE=1']
    exe = str(out/'granular-quality')
    run(shlex.split(os.environ.get('CC','cc'))+flags+['tests/granular_quality_test.c','-lm','-o',exe])
    for kind, name in enumerate(['CLOUD','FREEZE','STUTTER','SHIMMER']):
        for scenario, mode in enumerate(['low-density','high-density','minimum-size','reverse','mod-filter','four-track']):
            pcm = out/f'{name}-{mode}.pcm'
            r = run([exe,str(kind),str(scenario),str(pcm)],capture_output=True,text=True)
            record = json.loads(r.stdout)
            assert 0 < record['peak'] < 32767, ('capture silence/clipping',name,mode)
            assert pcm.stat().st_size == 65536*2*4
            record['pcm_sha256'] = sha(pcm)
            result['cases'][f'{name}/{mode}'] = record
            print(f'{name}/{mode}: {record["pcm_sha256"]}',flush=True)
    storage = str(out/'granular-quality-storage')
    run(shlex.split(os.environ.get('CC','cc'))+flags+([f'-DGRANULAR_QUALITY_PERSISTENCE_SOURCE="{a.oracle_tree.resolve()}/tests/persistence_test.c"'] if a.oracle_tree else [])+['tests/granular_quality_storage_test.c','-lm','-o',storage])
    r=run([storage],capture_output=True,text=True)
    result['storage']=json.loads(r.stdout)
    if not a.freeze:
        # The fix must pass alongside its retained legacy oracle. No sanitizer
        # exclusions on the new numerical/decoder kernel gate.
        kernel=str(out/'grain-short-envelope')
        strict=[f for f in flags if f != '-fno-sanitize=signed-integer-overflow,shift']
        run(shlex.split(os.environ.get('CC','cc'))+strict+['tests/grain_short_envelope_test.c','-lm','-o',kernel])
        r=run([kernel],capture_output=True,text=True)
        (out/'grain-short-envelope.log').write_text(r.stdout+r.stderr)
        print(r.stdout.strip(),flush=True)
        gate=str(out/'grain-stutter-gate')
        run(shlex.split(os.environ.get('CC','cc'))+strict+['tests/grain_stutter_gate_test.c','-lm','-o',gate])
        r=run([gate],capture_output=True,text=True)
        (out/'grain-stutter-gate.log').write_text(r.stdout+r.stderr)
        print(r.stdout.strip(),flush=True)
    if a.freeze:
        fixture.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    else:
        expected=json.loads(fixture.read_text())
        # Step239 changes audio only where a STUTTER gate participates. The
        # original golden remains intact; never override decoder/RNG fields.
        overlay=json.loads((root/'tests/fixtures/granular-quality-stutter.json').read_text())
        assert overlay['base']=='f0be0148acd39e27fe772e028cdc72630bdd7066'
        affected={k for k in expected['cases'] if k.startswith('STUTTER/') or k.endswith('/four-track')}
        assert set(overlay['audio'])==affected
        for key, audio in overlay['audio'].items():
            assert set(audio)=={'pcm_sha256','peak','rms','nonzero'}
            expected['cases'][key].update(audio)
        assert result==expected, 'Frozen PCM/state/input mismatch; do not regenerate without review'
    # Reuse frozen legacy readers and physical NOR assertions without rewriting
    # historical binaries or substituting a mock parser.
    for test in ['grain_win_persistence_test','upreset_test']:
        binary=str(out/test)
        source=(a.oracle_tree.resolve()/f'tests/{test}.c') if a.oracle_tree and test=='grain_win_persistence_test' else root/f'tests/{test}.c'
        run(shlex.split(os.environ.get('CC','cc'))+flags+[str(source),'-lm','-o',binary])
        r=run([binary],capture_output=True,text=True)
        (out/(test+'.log')).write_text(r.stdout+r.stderr)
        print(r.stdout.splitlines()[-1],flush=True)
    # A separate process repeat catches process-local seeds/state and ASLR dependence.
    pcm=out/'repeat.pcm'
    r=run([exe,'0','0',str(pcm)],capture_output=True,text=True)
    record=json.loads(r.stdout);record['pcm_sha256']=sha(pcm)
    assert record==result['cases']['CLOUD/low-density']
    # SPARKLE, CHAOS and FROZEN DRIFT are active. CLOUD stays exact;
    # Each active personality has a dedicated exact-base comparison gate.
    for kind,name in enumerate(['CLOUD','FREEZE','STUTTER','SHIMMER']):
        for scenario,mode in enumerate(['low-density','high-density','minimum-size','reverse','mod-filter','four-track']):
            r=run([exe,str(kind),str(scenario),str(pcm)],capture_output=True,text=True,env={**os.environ,'GRC_VALUE':'100'})
            c=json.loads(r.stdout);c['pcm_sha256']=sha(pcm)
            if kind==0 and scenario!=5:
                assert c==result['cases'][f'{name}/{mode}'],('inactive C changed audio/state',name,mode)
            else:
                assert c['pcm_sha256']!=result['cases'][f'{name}/{mode}']['pcm_sha256'],('active C silent/no difference',name,mode)
    print('Actual C100: 5 CLOUD PCM/state cases identical; 19 DRIFT/CHAOS/SPARKLE/four-track cases intentionally changed',flush=True)
print('24 frozen cases + independent process repeat passed; mappings, short windows, RNG, admission and load checked')
