#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Reproduction, PackBits row bounds, native RGB565 bytes and original integrity."""
import hashlib,json,re,struct,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import gen_boot_splash as g
source=g.SOURCE.parent
with tempfile.TemporaryDirectory() as tmp:
    out=Path(tmp);g.generate(out)
    for name in ['boot_splash.h','boot_splash_240.png','conversion.json']:
        assert (out/name).read_bytes()==(source/name).read_bytes(),name
meta=json.loads((source/'conversion.json').read_text())
assert meta['credit']=='CREATED BY Auberg1ne' and meta['art_rect']==[0,12,240,192]
assert abs(240/192-1402/1122)<0.001 # integer-pixel rounding of a uniform scale
text=(source/'boot_splash.h').read_text()
def arr(name):return list(map(int,re.search(name+r'\[\d+\] = \{(.*?)\};',text,re.S)[1].replace('\n','').replace(' ','').strip(',').split(',')))
pal,rows,data=map(arr,['BOOT_ART_PALETTE','BOOT_ART_ROWS','BOOT_ART_DATA'])
assert len(pal)==256 and len(rows)==241 and rows[0]==0 and rows[-1]==len(data)
rgb=bytearray()
for start,end in zip(rows,rows[1:]):
    p=start;row=[]
    while p<end:
        t=data[p];p+=1;n=(t&127)+1
        if t&128:row.extend([data[p]]*n);p+=1
        else:row.extend(data[p:p+n]);p+=n
        assert len(row)<=240 and p<=end
    assert len(row)==240 and p==end
    rgb.extend(b''.join(struct.pack('<H',pal[v]) for v in row))
assert len(rgb)==115200 and hashlib.sha256(rgb).hexdigest()==meta['rgb565_sha256']
h=2166136261
for v in rgb:h=((h^v)*16777619)&0xffffffff
print(f'splash asset: reproduction, 240 bounded rows, source hash and RGB565 hash passed; FNV {h:08x}; {meta["total_asset_bytes"]} B')
