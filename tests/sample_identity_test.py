#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Real sample generator + firmware compatibility matrix, entirely in test output.

No source assets are moved: present material directories are read-only symlinks.
Run from the repo root; native cases need the existing build/gen dependencies.
"""
import argparse
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
spec = importlib.util.spec_from_file_location('identity_generator', ROOT / 'tools/gen_samples.py')
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)
NAMES = ['GUITAR', 'GUITAR', 'VOICE', 'HIT', 'GUITAR']
ASSETS = gen.CC0
PROFILES = [
    ('full', {'GUITAR', 'VOICE', 'HIT'}, True),
    ('assetless', set(), True),
    ('empty-directory', set(), True),
    ('assetless-slice-off', set(), False),
    ('missing-guitar', {'VOICE', 'HIT'}, True),
    ('missing-voice', {'GUITAR', 'HIT'}, True),
    ('missing-hit', {'GUITAR', 'VOICE'}, True),
]


def matrix(args, out):
    count, skipped = 0, 0
    for name, present, slice_on in PROFILES:
        missing = sorted(m for m in present if not any((ASSETS / m).glob('*.wav')))
        if missing:
            print(f'SKIP {name}: real material fixtures unavailable: {", ".join(missing)}')
            skipped += 1
            continue
        root = out / name
        cc0 = root / 'assets/samples-cc0'
        if present or name == 'empty-directory':
            cc0.mkdir(parents=True, exist_ok=True)
        for material in present:
            path = ASSETS / material
            assert path.is_dir(), f'Missing test dependency: {path}'
            link = cc0 / material
            if not link.exists():
                link.symlink_to(path, target_is_directory=True)
            assert link.resolve() == path.resolve(), f'Unexpected fixture link: {link}'
        gen.SRC, gen.CC0 = root, cc0
        gen.GENDIR, gen.CACHE = root / 'build/genwav', root / 'build/gen_samples.cache'
        gen.CACHE.parent.mkdir(parents=True, exist_ok=True)
        header = root / 'felucca_samples.h'
        old_slice = os.environ.get('FELUCCA_SLICE')
        os.environ['FELUCCA_SLICE'] = '1' if slice_on else '0'
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                gen.main(header)
            first = header.read_bytes()
            # The actual cache path must preserve the same identities too.
            with contextlib.redirect_stdout(io.StringIO()) as cached:
                gen.main(header)
            assert '(cached)' in cached.getvalue(), f'{name}: cache path not exercised'
            assert header.read_bytes() == first, f'{name}: cache changed header'
        finally:
            if old_slice is None:
                os.environ.pop('FELUCCA_SLICE', None)
            else:
                os.environ['FELUCCA_SLICE'] = old_slice
        text = first.decode()
        assert '#define SMP_NSETS 5\n' in text, f'{name}: factory IDs shifted'
        assert '#define SMP_NPRESETS 4\n' in text, f'{name}: preset IDs shifted'
        block = re.search(r'SMP_SETS\[\] = \{(.*?)\n\};', text, re.S).group(1)
        sets = re.findall(r'\{"([^"]+)", (\d+), (\d+)\}', block)
        assert [s[0] for s in sets] == NAMES, f'{name}: factory labels shifted'
        assert 'SMP_SET_ORIG[] = {0, 0, 2, 3, 0}' in text
        zones = re.search(r'SMP_ZONES\[\] = \{(.*?)\n\};', text, re.S).group(1)
        zones = [[int(n.strip()) for n in z.split(',')] for z in re.findall(r'\{([^}]+)\}', zones)]
        for label, start, nz in sets:
            start, nz = int(start), int(nz)
            assert nz >= 1 and start + nz <= len(zones), f'{name}: invalid factory zone bounds'
            assert bool(zones[start][1]) == (label in present), f'{name}: wrong availability for {label}'
        if 'GUITAR' not in present:
            assert '#define SLC_PIANO_INIT {0}' in text, f'{name}: missing factory SLICE source exposed'
        if name == 'full' and args.reference:
            assert first == args.reference.read_bytes(), 'Bundled header/audio changed'
        count += 1
        print(f'PASS {name}: five stable factories, aliases, availability and cache')
        if args.native and (not args.sanitize or name in {'full', 'assetless', 'missing-guitar'}):
            binary = root / 'identity_test'
            flags = ['-O1', '-w', '-ffp-contract=off']
            if args.sanitize:
                flags += ['-g', '-fsanitize=address,undefined',
                          '-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow',
                          '-fno-sanitize-recover=undefined']
            # The SLICE-specific compatibility fixture requires SLICE. For an
            # actual disabled build, use the guarded real upload/recovery suite.
            source = 'tests/eight_slot_compat_test.c' if slice_on else 'tests/sample_delete_test.c'
            if not slice_on:
                flags += ['-DFELUCCA_SLICE=0']
            cmd = shlex.split(os.environ.get('CC', 'cc')) + flags + [
                '-I' + str(root), '-I' + str(ROOT / 'build/gen'), '-I' + str(ROOT / 'firmware/src'),
                str(ROOT / source), '-lm', '-o', str(binary)]
            subprocess.run(cmd, check=True, cwd=ROOT)
            env = dict(os.environ)
            env.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
            result = subprocess.run([str(binary)], text=True, capture_output=True, env=env, cwd=ROOT)
            (root / 'native.log').write_text(result.stdout + result.stderr)
            assert result.returncode == 0, f'{name}: native checks failed; see {root / "native.log"}'
            checks = sum(' ok' in line for line in result.stdout.splitlines())
            print(f'PASS {name}: {checks} actual firmware compatibility checks')
    print(f'{count} generator profiles passed; {skipped} profiles skipped')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--reference', type=Path, help='Optional pre-change full header; require byte identity')
    parser.add_argument('--native', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    if args.sanitize:
        args.native = True
    if args.out:
        args.out.mkdir(parents=True, exist_ok=True)
        matrix(args, args.out.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix='coffee-grains-identity-') as tmp:
            matrix(args, Path(tmp))


if __name__ == '__main__':
    main()
