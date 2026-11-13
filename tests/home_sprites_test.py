#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import hashlib,json,re,sys,tempfile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import gen_home_sprites as g
base=ROOT/'assets/home-scientists'
for digest,name in (x.split() for x in (base/'reference/SHA256SUMS').read_text().splitlines()):
    assert hashlib.sha256((base/'reference'/name).read_bytes()).hexdigest()==digest
with tempfile.TemporaryDirectory() as tmp:
    out=Path(tmp);g.generate(out)
    for name in ['home_sprites.h','sprites.json','poses_native.png']:assert (out/name).read_bytes()==(base/name).read_bytes(),name
text=(base/'home_sprites.h').read_text();info=json.loads((base/'sprites.json').read_text())
def arr(name):return list(map(int,re.search(name+r'\[\d+\] = \{(.*?)\};',text,re.S)[1].replace('\n','').replace(' ','').strip(',').split(',')))
pal,offsets,data=map(arr,['HOME_SPRITE_PALETTE','HOME_SPRITE_OFFSETS','HOME_SPRITE_DATA'])
assert len(pal)==256 and len(offsets)==13 and offsets[0]==0 and offsets[-1]==len(data)
hashes=[]
for start,end in zip(offsets,offsets[1:]):
    p=start;pixels=[]
    while p<end:
        t=data[p];p+=1;n=(t&127)+1
        if t&128:pixels.extend([data[p]]*n);p+=1
        else:pixels.extend(data[p:p+n]);p+=n
        assert len(pixels)<=90*56 and p<=end
    assert p==end and len(pixels)==90*56
    rgb=b''.join(pal[k].to_bytes(2,'little') for k in pixels);hashes.append(hashlib.sha256(rgb).hexdigest())
assert hashes==info['frame_rgb565_sha256'] and len(set(hashes))==12
assert len(data)+512+52==30135
print('HOME sprites: source hashes, exact reproduction, 12 distinct bounded poses, RGB565 hashes and 30135 B verified')
