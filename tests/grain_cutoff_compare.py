#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Same sounding harness against the exact pre-fix base; no baseline fixture edits."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

BASE = '44ec4606c0469cc79326a4887b78e321ec704739'
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--base-tree', required=True, type=Path)
p.add_argument('--out', required=True, type=Path)
p.add_argument('--sanitize', action='store_true')
a = p.parse_args()
a.base_tree = a.base_tree.resolve()
a.out = a.out.resolve()
a.out.mkdir(parents=True, exist_ok=True)
assert subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=a.base_tree, text=True).strip() == BASE
subprocess.run(['git', 'diff', '--exit-code', BASE, '--', 'firmware', 'web', 'tests'], cwd=a.base_tree, check=True)
flags = ['-O1', '-g', '-w', '-ffp-contract=off', '-Ibuild/gen', '-Ifirmware/src']
if a.sanitize:
    flags += ['-fsanitize=address,undefined', '-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow', '-fno-sanitize-recover=undefined']

def build(job):
    name, source, tree, extra = job
    macro = 'GRID_UI_SOURCE' if source == 'grain_only_navigation_test' else 'CUTOFF_UI_SOURCE'
    cmd = shlex.split(os.environ.get('CC', 'cc')) + flags + extra
    cmd += [f'-D{macro}="{tree}/tests/ui_test.c"', f'tests/{source}.c', '-lm', '-o', str(a.out / name)]
    with (a.out / f'{name}-build.log').open('w') as log:
        subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=log, check=True)

jobs = [('unit', 'grain_cutoff_zero_test', ROOT, []),
        ('unit-base', 'grain_cutoff_zero_test', a.base_tree, []),
        ('audio', 'grain_cutoff_audio_test', ROOT, []),
        ('audio-base', 'grain_cutoff_audio_test', a.base_tree, []),
        ('audio-muted', 'grain_cutoff_audio_test', ROOT, ['-DAUDIO_TEST_MASTER=0']),
        ('nav', 'grain_only_navigation_test', ROOT, []),
        ('nav-base', 'grain_only_navigation_test', a.base_tree, [])]
with ThreadPoolExecutor(max_workers=3) as pool:
    list(pool.map(build, jobs))

def run(name, args=(), success=True):
    result = subprocess.run([str(a.out / name), *map(str, args)], cwd=ROOT, capture_output=True, text=True)
    with (a.out / f'{name}-run.log').open('a') as log:
        log.write(result.stdout + result.stderr)
    if success:
        assert result.returncode == 0, (name, args, result.stdout, result.stderr)
    else:
        assert result.returncode == 1, (name, args, result.returncode, result.stdout, result.stderr)
        # An expected red test must never conceal a sanitizer failure.
        assert 'runtime error:' not in result.stderr and 'AddressSanitizer' not in result.stderr
    return result.stdout

print(run('unit').strip(), flush=True)
run('unit-base', success=False)  # The original missing dry output is caught by the new assertions.
run('audio-muted', [0, 25, a.out / 'muted.pcm'], success=False)
assert not any((a.out / 'muted.pcm').read_bytes())
records = []
def compare(kind, cutoff, tracks=1):
    run('audio-base', [kind, cutoff, a.out / 'base.pcm', tracks])
    evidence = run('audio', [kind, cutoff, a.out / 'current.pcm', tracks])
    base = (a.out / 'base.pcm').read_bytes()
    current = (a.out / 'current.pcm').read_bytes()
    assert base == current, (kind, cutoff, tracks)
    assert any(current), (kind, cutoff, tracks, 'silent comparison')
    records.append({'type': kind, 'cutoff': cutoff, 'tracks': tracks, 'bytes': len(current),
                    'sha256': hashlib.sha256(current).hexdigest(), 'evidence': evidence.strip()})

for kind in range(4):
    for cutoff in [c for c in range(-50, 51) if c]:
        compare(kind, cutoff)
    for cutoff in (-25, 25):
        compare(kind, cutoff, 4)
    print(f'personality {kind}: 100 nonzero CUTOFF renders byte-identical to exact base', flush=True)
    for tracks in (1, 4):
        print(run('audio', [kind, 0, a.out / 'zero.pcm', tracks]).strip(), flush=True)
    run('audio-base', [kind, 0, a.out / 'zero-base.pcm'], success=False)
    assert not any((a.out / 'zero-base.pcm').read_bytes())
    print(run('audio', [kind, 'switch', a.out / 'switch.pcm', 4]).strip(), flush=True)
for name in ('nav-base', 'nav'):
    print(run(name, [a.out / f'{name}.bin']).strip(), flush=True)
assert (a.out / 'nav-base.bin').read_bytes() == (a.out / 'nav.bin').read_bytes()
(a.out / 'comparisons.json').write_text(json.dumps(records, indent=2) + '\n')
print(f'{len(records)} sounding captures / {sum(r["bytes"] for r in records)} bytes identical; UI unchanged; zero-master rejected', flush=True)
