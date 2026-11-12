#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Factory inventory and unchanged custom bytes; no generated files are written."""
import hashlib
from pathlib import Path
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import gen_samples as g

hashes = {
    'GUITAR': '9907ef1bfc03b4fe9b2acb941c1c06c5a68b70255f1aa83917cfded97089cb62',
    'VOICE': '93ba5ce8c749491eb3fbb2a4cf4c03d96c34c5f3fbfd4ccfaf65095bc79c1335',
    'HIT': '8bcf3632acb59fa20b7adfcc5fc89590e43e6ab801cc1380bdafa0abe868a96e',
}
for name, digest in hashes.items():
    for p in (ROOT / f'assets/custom-samples/{name}.wav', ROOT / f'assets/samples-cc0/{name}/{name}_C3.wav'):
        assert hashlib.sha256(p.read_bytes()).hexdigest() == digest, p
b = g.Builder()
for name, kind in g.CC0_SETS[:5]:
    if kind == 'alias':
        b.alias_set(name)
    else:
        b.cc0_set(name, kind)
legacy_bytes = bytes(b.blob)
# Independent checkpoint fingerprint (11fe3fe generated SMP_DATA prefix).
assert hashlib.sha256(legacy_bytes).hexdigest() == '086a0e1f1f8caa0fd2764123c19366d1bac56d7aec3ce082fe12e50fb200e6d7'
legacy_zones = [dict(z) for z in b.zones]
costs = []
for name, kind in g.CC0_SETS[5:]:
    before = len(b.blob)
    b.cc0_set(name, kind)
    costs.append(len(b.blob) - before)
assert costs == [31422, 31422], costs
assert bytes(b.blob[:len(legacy_bytes)]) == legacy_bytes
assert b.zones[:len(legacy_zones)] == legacy_zones
assert len(legacy_bytes) == 32526 and len(b.blob) == 95370
assert b.sets[1] == b.sets[0] and b.sets[4] == b.sets[0]
assert [s[0] for s in b.sets] == ['GUITAR', 'GUITAR', 'VOICE', 'HIT', 'GUITAR', 'FLUTE', 'SAX']
assert '#define SMP_NSETS 5' in b.header() and '13, 14' in b.header()
assert '#define SMP_NFACTORY 7' in g.Builder().header()  # absent-library fallback retains ABI
partial = g.Builder()
partial.cc0_set('GUITAR', 'oneshot')
try:
    partial.header()
except ValueError:
    pass
else:
    raise AssertionError('partial inventories must not silently renumber sources')
print('factory assets: passed; factory ADPCM 95370 B, additions 62844 B; legacy bytes/zones unchanged')
