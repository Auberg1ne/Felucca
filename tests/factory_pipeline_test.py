#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Private five-source pipeline fixtures; never regenerate production assets.

Uses real Builder/cc0_entries/IMA encoding and actual native firmware decoders.
Synthetic sounds are test material, not proposed production replacements.
"""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import tempfile
import wave
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
spec = importlib.util.spec_from_file_location('factory_fixture_generator', ROOT / 'tools/gen_samples.py')
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)
PROFILES = [('five-distinct', set(range(5))), ('missing-alias-replacements', {0, 2, 3}), ('assetless', set())]


def reference_decode(data, n):
    pred = idx = 0
    out, states = [], []
    for i in range(n):
        states.append((pred, idx))
        code = data[i // 2] >> (4 * (i % 2)) & 15
        step = gen.sio.IMA_STEP[idx]
        delta = (step >> 3) + ((step >> 2) if code & 1 else 0) + ((step >> 1) if code & 2 else 0) + (step if code & 4 else 0)
        pred = max(-32768, min(32767, pred + (-delta if code & 8 else delta)))
        idx = max(0, min(88, idx + gen.sio.IMA_IDX[code & 7]))
        out.append(pred)
    return out, states


def c_rows(rows):
    return ',\n'.join('{' + ','.join(str(x) for x in row) + '}' for row in rows)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out', type=Path)
    ap.add_argument('--sanitize', action='store_true')
    ap.add_argument('--without-break', action='store_true', help='Exercise the one-byte SMP_DATA sentinel when assetless')
    ap.add_argument('--strict-decode', action='store_true', help='Strict ASan/UBSan without DSP render paths')
    ap.add_argument('--slice-off', action='store_true', help='Actual FELUCCA_SLICE=0 build, also omitting BREAK')
    ap.add_argument('--bad-descriptors', action='store_true', help='Test-only mutable factory descriptors for rejection checks')
    args = ap.parse_args()
    if args.slice_off:
        args.without_break = True
    out = (args.out or Path(tempfile.mkdtemp(prefix='coffee-grains-factory-fixtures-'))).resolve()
    if out.is_relative_to(ROOT):
        raise ValueError('Fixture output must be outside the checkout')
    out.mkdir(parents=True, exist_ok=True)
    watched = [ROOT / 'tools/gen_samples.py', ROOT / 'build/gen/felucca_samples.h', ROOT / 'build/gen_samples.cache']
    watched += sorted((ROOT / 'assets').glob('**/*.wav'))
    before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in watched if p.is_file()}
    names = ['FIXTURE' + str(i) for i in range(5)]
    gen.CC0_SETS = [(name, 'sus' if i == 2 else 'oneshot') for i, name in enumerate(names)]
    totals = []
    for label, present in PROFILES:
        dest = out / label
        dest.mkdir(parents=True, exist_ok=False)
        gen.CC0 = dest / 'assets'
        for i in sorted(present):
            folder = gen.CC0 / names[i]
            folder.mkdir(parents=True, exist_ok=True)
            # Common root, distinct harmonic content; looped fixture is long
            # enough for the real fixed sustain-loop/crossfade preparation.
            samples = [int(13000 * (math.sin(2 * math.pi * 261.625565 * k / 22050) +
                       0.35 * math.sin((i + 2) * 2 * math.pi * 261.625565 * k / 22050)) / 1.35)
                       for k in range(22050)]
            with wave.open(str(folder / (names[i] + '_C3.wav')), 'wb') as f:
                f.setnchannels(1); f.setsampwidth(2); f.setframerate(22050)
                f.writeframes(struct.pack('<' + 'h' * len(samples), *samples))
        b = gen.Builder()
        for name, kind in gen.CC0_SETS:
            b.cc0_set(name, kind)
        if not args.without_break:
            # Match actual default generation: BREAK exists even without WAV assets.
            # Existing generated drum WAVs are read only; no generator is run here.
            b.slice_break()
        header = b.header()
        assert header == b.header()
        if args.bad_descriptors:
            header = header.replace('static const smp_zone_t SMP_ZONES', 'static smp_zone_t SMP_ZONES').replace('static const smp_set_t SMP_SETS', 'static smp_set_t SMP_SETS')
        (dest / 'felucca_samples.h').write_text(header)
        crcs, prefixes, loops, hashes = [], [], [], []
        for i, (_, z0, nz) in enumerate(b.sets):
            assert nz == 1 and (i in present) == bool(b.zones[z0]['n'])
            z = b.zones[z0]
            encoded = b.blob[z['off']:z['off'] + (z['n'] + 1) // 2]
            decoded, states = reference_decode(encoded, z['n'])
            assert 0 <= z['off'] <= len(b.blob) and z['off'] % 2 == 0
            if z['n']:
                assert z['off'] + (z['n'] + 1) // 2 <= len(b.blob)
                assert sum((v['n'] + 255) // 256 for v in b.zones[z0:z0+nz]) <= 1024
                assert z['lo'] == 0 and z['hi'] == 127
            if z['looped']:
                assert states[z['ls']] == (z['pred'], z['idx']) and z['ls'] < z['le'] < z['n']
            crcs.append(zlib.crc32(struct.pack('<' + 'h' * len(decoded), *decoded)))
            prefixes.append((decoded[:32] + [0]*32)[:32])
            loops.append((decoded[z['ls']:z['ls']+16] + [0]*16)[:16] if z['looped'] else [0]*16)
            hashes.append(hashlib.sha256(encoded).hexdigest())
        assert len({hashes[i] for i in present}) == len(present)
        assert len({crcs[i] for i in present}) == len(present)
        expected = '#include <stdint.h>\n'
        if args.bad_descriptors:
            expected += '#define FACTORY_TABLE_MUTABLE_TEST 1\n'
        expected += 'static const char *const fixture_names[5]={' + ','.join(json.dumps(n) for n in names) + '};\n'
        expected += 'static const uint8_t fixture_present[5]={' + ','.join(str(int(i in present)) for i in range(5)) + '};\n'
        expected += 'static const uint32_t fixture_crc[5]={' + ','.join(str(c)+'u' for c in crcs) + '};\n'
        expected += 'static const int16_t fixture_prefix[5][32]={' + c_rows(prefixes) + '};\n'
        expected += 'static const int16_t fixture_loop[5][16]={' + c_rows(loops) + '};\n'
        (dest / 'factory_fixture_expected.h').write_text(expected)
        flags = ['-O1', '-w', '-ffp-contract=off']
        if args.slice_off:
            flags += ['-DFELUCCA_SLICE=0']
        if args.sanitize or args.strict_decode:
            flags += ['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all']
            if not args.strict_decode:
                flags += ['-fno-sanitize=signed-integer-overflow,shift']
        cmd = shlex.split(os.environ.get('CC', 'cc')) + flags + ['-I'+str(dest), '-I'+str(ROOT/'build/gen'), '-I'+str(ROOT/'firmware/src'), str(ROOT/'tests/factory_pipeline_test.c'), '-lm', '-o', str(dest/'native')]
        (dest/'command.json').write_text(json.dumps(cmd))
        subprocess.run(cmd, check=True, cwd=ROOT)
        env = dict(os.environ); env.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
        result = subprocess.run([str(dest/'native')] + (['--decode-only'] if args.strict_decode else []), cwd=ROOT, env=env, text=True, capture_output=True)
        (dest/'native.log').write_text(result.stdout+result.stderr)
        assert result.returncode == 0, f'{label}: see {dest / "native.log"}'
        count = sum(line.endswith(' ok') for line in result.stdout.splitlines())
        assert count == (9 if args.strict_decode else 35 if args.slice_off else 43) + (12 if args.bad_descriptors else 0)
        totals.append({'profile': label, 'checks': count, 'blob_bytes': len(b.blob), 'audio_hashes': hashes, 'decoded_crcs': crcs})
        print(f'PASS {label}: {count} native checks; {len(present)} distinct available sources')
    for p, h in before.items():
        assert hashlib.sha256(p.read_bytes()).hexdigest() == h, f'Production input/output changed: {p}'
    (out/'summary.json').write_text(json.dumps(totals, indent=2)+'\n')
    print(f'{sum(t["checks"] for t in totals)} native checks passed; production assets/header/cache unchanged; {out}')


if __name__ == '__main__':
    main()
