#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Full-frame preview encoding, source integrity, metadata and shared SLICE audio."""
import hashlib,json,struct,sys,wave,tempfile,shutil
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import gen_samples as g
b=g.Builder()
for name,kind in g.CC0_SETS:
    b.alias_set(name) if kind=='alias' else b.preview_set(name,kind)
records=json.loads((g.PREVIEW/'manifest.json').read_text())
costs=[88462,62660,36536,137550,33076]
# Fingerprints from the independently assessed full-frame source streams.
encoded_hashes=['e5e72163b1106034aba78ad7160c9c35b43954857f1c9b53ba3e1ddc8372b2c0','557662b45ffffdb7e85c51edfcc2758d568507e53dbe2b30ed89f9d3674d1b91','02fff359dbf02ca648014db9876a3ce7c0ff2047121621d5661d0c198a4a27e7','09d766fe66b6ae5f8edf35905c72278b88b77c0891dce822c0ed280eb61a469c','b9f416d82ed06bdf8b573f9dfaef85b1c630796de8d496a602e1d7e571f4a4d8']
for record,fi,cost,digest in zip(records,[0,2,3,5,6],costs,encoded_hashes):
    p=g.PREVIEW/record['file'];assert hashlib.sha256(p.read_bytes()).hexdigest()==record['sha256']
    z=b.zones[b.sets[fi][1]]
    with wave.open(str(p)) as w: samples=struct.unpack(f'<{w.getnframes()}h',w.readframes(w.getnframes()))
    enc,state=g.ima_encode(samples,z['ls']);enc+=b'\0'*(len(enc)%2)
    assert hashlib.sha256(enc).hexdigest()==digest
    assert len(enc)==cost and bytes(b.blob[z['off']:z['off']+cost])==enc
    assert z['n']==record['frames'] and z['sr']==44100
    assert (z['pred'],z['idx'])==state
assert len(b.blob)==358284 and len(b.zones)==5
assert b.sets[1]==b.sets[0] and b.sets[4]==b.sets[0]
assert b.zones[0]['looped'] and b.zones[0]['ls']==0 and b.zones[0]['le']==176923
assert all(not z['looped'] for z in b.zones[1:])
b.slice_piano();assert b.pno['off']==b.zones[2]['off'] and b.pno['n']==73071 and b.pno['sr']==44100
assert '#define SMP_NSETS 5' in b.header() and '#define SMP_NFACTORY 7' in b.header()
print('factory assets: 5 checksummed originals; full PCM encoding, 358284 B, metadata and shared GUITAR verified')

original=g.PREVIEW
with tempfile.TemporaryDirectory() as tmp:
    g.PREVIEW=Path(tmp)
    shutil.copy(original/'manifest.json',g.PREVIEW/'manifest.json')
    shutil.copy(original/'CELLO.wav',g.PREVIEW/'CELLO.wav')
    p=g.PREVIEW/'CELLO.wav';raw=bytearray(p.read_bytes());raw[100]^=1;p.write_bytes(raw)
    try:
        g.Builder().preview_set('CELLO','sus')
    except ValueError as e:
        assert 'checksum mismatch' in str(e)
    else:
        raise AssertionError('corrupt original was accepted')
    p.unlink()
    try:
        g.Builder().preview_set('CELLO','sus')
    except FileNotFoundError:
        pass
    else:
        raise AssertionError('missing original was accepted')
g.PREVIEW=original
print('factory corruption: changed and missing originals rejected')
