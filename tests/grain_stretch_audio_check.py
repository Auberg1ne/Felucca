#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check actual rendered audio and source-head progression across four values."""
from pathlib import Path
import sys, re, hashlib
folder=Path(sys.argv[1]);prefix=sys.argv[2] if len(sys.argv)>2 else 'stretch'
values=(0,1,64,127)
heads=[];hashes=[]
for value in values:
    raw=(folder/f'{prefix}-{value}.f32').read_bytes()
    assert len(raw)==22050*2*4,(value,len(raw))
    log=(folder/f'{prefix}-{value}.log').read_text()
    heads.append(int(re.search(r'head-Q16=(\d+)',log)[1]))
    hashes.append(hashlib.sha256(raw).hexdigest())
assert heads[0]>heads[1]>heads[2]>heads[3]>0,heads
assert len(set(hashes))==4,hashes
print('STRETCH audio: four nonzero/distinct renders, progressively slower existing source head:',heads)
