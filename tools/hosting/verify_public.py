#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Strict HTTPS observation used before guard creation and before finalization.
Only HTTP404 means absent. Network errors, stale/mixed/unknown data block progress.
No public mutation; this is not an audio/browser validation."""
import argparse
import hashlib
import json
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import urlopen

BASE='https://auberg1ne.github.io/Felucca/'

def fetch(path,nonce):
    try:
        with urlopen(BASE+path+'?verify='+nonce,timeout=30) as r:
            if not r.geturl().startswith(BASE):raise ValueError('Unexpected redirect')
            data=r.read(4*1024*1024+1)
            if len(data)>4*1024*1024:raise ValueError('Unexpected response size')
            return data
    except HTTPError as e:
        if e.code==404:return None
        raise


def verify_site(site,get=fetch):
    marker=site/'deployment.json'
    nonce=hashlib.sha256(marker.read_bytes() if marker.exists() else b'initial-state-check').hexdigest()
    def exact(path):
        p=site/path;expected=p.read_bytes() if p.is_file() else None
        if get(path,nonce)!=expected:raise ValueError('Published/retained state mismatch: '+path)
    exact('deployment.json');exact('index.html')
    for c in ['stable','dev']:
        exact(c+'/index.html');exact(c+'/build.json')
        local=site/c/'build.json'
        if not local.exists():continue
        release=json.loads(local.read_text())['release']
        path='releases/'+release+'/manifest.json';exact(path)
        manifest=json.loads((site/path).read_text())
        for name,expected in manifest['files'].items():
            data=get('releases/'+release+'/'+name,nonce)
            if data is None or hashlib.sha256(data).hexdigest()!=expected:
                raise ValueError('Public asset mismatch: '+name)
    print('Published channels and runtime checksums match the selected snapshot; not a browser/audio claim')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',default=BASE);p.add_argument('--site',type=Path,required=True)
    args=p.parse_args()
    if args.base.rstrip('/')+'/'!=BASE:raise ValueError('Unexpected deployment origin/path')
    verify_site(args.site)

if __name__=='__main__':main()
