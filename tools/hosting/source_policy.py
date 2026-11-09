#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Controller-owned source authorization. No ref aliases or implicit main fixtures.
Approval changes require a reviewed controller PR; environment approval is additional.
All Git operations here are read-only."""
import argparse
import json
from pathlib import Path
import re
import subprocess

APPROVALS=Path(__file__).with_name('approved-sources.json')
SHA=re.compile(r'^[0-9a-f]{40}$')


def approved(revision,policy=APPROVALS):
    if not isinstance(revision,str) or not SHA.fullmatch(revision):
        raise ValueError('A full lowercase40-character source commit SHA is required')
    p=json.loads(policy.read_text())
    if not isinstance(p,dict) or set(p)!={'schema','sources'}:
        raise ValueError('Malformed approved-source policy')
    sources=p.get('sources')
    if p.get('schema')!=1 or not isinstance(sources,list) or not 1<=len(sources)<=256 or any(not isinstance(s,str) or not SHA.fullmatch(s) for s in sources) or len(set(sources))!=len(sources):
        raise ValueError('Malformed or unsupported approved-source policy')
    if revision not in sources:raise ValueError('Source commit is not approved in the controller policy')
    return revision


def verify_checkout(root,revision,require_submodules=False):
    approved(revision)
    root=Path(root).resolve()
    def git(*args):
        result=subprocess.run(['git','-C',str(root),*args],capture_output=True,text=True)
        if result.returncode:raise ValueError('Cannot verify source checkout: '+args[0])
        return result.stdout
    if git('rev-parse','--show-toplevel').strip()!=str(root):raise ValueError('Fixture/source path must be the checkout root')
    if git('cat-file','-t',revision).strip()!='commit' or git('rev-parse','HEAD').strip()!=revision:
        raise ValueError('Source checkout is not the exact approved commit')
    if git('status','--porcelain','--untracked-files=normal').strip():
        raise ValueError('Source checkout has uncommitted changes')
    if require_submodules:
        modules=git('submodule','status','--recursive').splitlines()
        if not modules or any(line and line[0]!=' ' for line in modules):
            raise ValueError('Required source submodules are missing, modified or conflicted')
    return root


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('revision');p.add_argument('--checkout',type=Path)
    p.add_argument('--require-submodules',action='store_true');args=p.parse_args()
    approved(args.revision)
    if args.checkout:verify_checkout(args.checkout,args.revision,args.require_submodules)
    elif args.require_submodules:raise ValueError('--checkout is required for submodule verification')
    print('Approved source verified:',args.revision)

if __name__=='__main__':main()
