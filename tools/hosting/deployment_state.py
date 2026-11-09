#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Durable Pages transaction. Ordinary runs cannot bypass an unresolved guard.
Git writes exist only in the explicitly gated Actions CLI, never in test backends.
The state branch is the journal; the Pages service is an independent participant.
"""
import argparse
import base64
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('pages_assembly', HERE/'site.py')
site = importlib.util.module_from_spec(spec); spec.loader.exec_module(site)
spec = importlib.util.spec_from_file_location('pages_https', HERE/'verify_public.py')
https = importlib.util.module_from_spec(spec); spec.loader.exec_module(https)
BRANCH = 'coffee-grains-pages-state'
CONTROL = '.deployment/'
PENDING = CONTROL+'pending.json'
TX = re.compile(r'^[1-9][0-9]*-[1-9][0-9]*$')
PUBLISH_STEP = 'Deploy combined site atomically'
TERMINAL = {'succeed', 'deployment_failed', 'deployment_perms_error',
            'deployment_content_failed', 'deployment_cancelled'}


def encode(obj):
    return (json.dumps(obj, sort_keys=True, indent=2)+'\n').encode()


def inventory(files):
    return {name:hashlib.sha256(data).hexdigest() for name,data in sorted(files.items())}


def digest(files):
    return hashlib.sha256(encode(inventory(files))).hexdigest()


def read_tree(root):
    files={}
    for p in root.rglob('*'):
        if p.is_symlink():raise ValueError('Symlink in state/site')
        if p.is_file():files[p.relative_to(root).as_posix()]=p.read_bytes()
    return files


def write_tree(root, files):
    if root.exists() and any(root.iterdir()):raise ValueError('Output must be empty')
    root.mkdir(parents=True,exist_ok=True)
    for name,data in files.items():
        if not name or Path(name).is_absolute() or '..' in Path(name).parts:
            raise ValueError('Invalid state path')
        p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)


def normal(files):
    return {n:b for n,b in files.items() if not n.startswith(CONTROL)}


def channels(files):
    return {c:json.loads(files[c+'/build.json'])['release'] if c+'/build.json' in files else 'none'
            for c in ('stable','dev')}


def idle(files):
    # Unexpected control data is never permission to treat storage as blank.
    if any(n.startswith(CONTROL) for n in files):raise ValueError('Pending deployment: reconcile before any new operation')


def check_site(files):
    if any(n.startswith(CONTROL) for n in files):raise ValueError('Control files cannot be published')
    if not files:return
    with tempfile.TemporaryDirectory(prefix='coffee-grains-state-verify-') as d:
        root=Path(d);write_tree(root,files)
        for release in (root/'releases').iterdir():site.verify(release)
        for c,r in channels(files).items():
            if r=='none':continue
            m=site.verify(root/'releases'/r)
            b=json.loads(files[c+'/build.json'])
            if b.get('channel')!=c or b.get('source')!=m['source']:raise ValueError('Channel identity mismatch')
            if f'../releases/{r}/emu/index.html?channel={c}' not in files[c+'/index.html'].decode():
                raise ValueError('Channel redirect mismatch')


def pending(files):
    if PENDING not in files:raise ValueError('No matching pending transaction; no recovery/retry performed')
    p=json.loads(files[PENDING])
    if p.get('schema')!=1 or not TX.fullmatch(p.get('transaction','')):raise ValueError('Invalid guard')
    previous=normal(files)
    if digest(previous)!=p['previous_digest']:raise ValueError('Retained snapshot changed under guard')
    candidate=dict(previous)
    for name in p['changed']:
        key=CONTROL+'files/'+name
        if key not in files:raise ValueError('Incomplete pending candidate')
        candidate[name]=files[key]
    if set(files)-set(previous)!={PENDING}|{CONTROL+'files/'+n for n in p['changed']}:
        raise ValueError('Unknown pending data')
    if inventory(candidate)!=p['candidate_files'] or digest(candidate)!=p['candidate_digest']:
        raise ValueError('Pending candidate changed')
    if channels(previous)!=p['before'] or channels(candidate)!=p['after']:raise ValueError('Guard channel mismatch')
    check_site(candidate)
    return p,previous,candidate


class Transaction:
    """Store supplies durable read/CAS; observe must compare the ACTUAL published site."""
    def __init__(self,store,observe):self.store=store;self.observe=observe
    def export(self):
        head,files=self.store.read();idle(files);check_site(files);self.observe(files)
        return head,files
    def begin(self,expected,files,operation,release,owner):
        head,previous=self.store.read();idle(previous)
        if head!=expected:raise ValueError('Retained head changed; restart review')
        if operation not in ('build-dev','promote-stable','rollback-dev','rollback-stable'):raise ValueError('Invalid operation')
        if not TX.fullmatch(owner['transaction']) or owner['transaction']!=f"{owner['run_id']}-{owner['run_attempt']}" or not site.SHA.fullmatch(owner['workflow_sha']):
            raise ValueError('Invalid transaction identity')
        channel='stable' if 'stable' in operation else 'dev'
        if channels(files)[channel]!=release:raise ValueError('Candidate does not match approved release')
        other='dev' if channel=='stable' else 'stable'
        if channels(previous)[other]!=channels(files)[other] or {n:b for n,b in previous.items() if n.startswith(other+'/')}!={n:b for n,b in files.items() if n.startswith(other+'/')}:
            raise ValueError('Unapproved channel change')
        if operation!='build-dev':
            approval=json.loads(previous.get('approvals/'+release+'.json',b'{}'))
            if approval.get('release')!=release or not any(n.startswith('releases/'+release+'/') for n in previous):
                raise ValueError('Promotion/rollback requires a retained approved DEV release')
        if set(previous)-set(files):raise ValueError('Retained files cannot be removed')
        for name,data in previous.items():
            if name.startswith(('releases/','approvals/')) and files.get(name)!=data:
                raise ValueError('Immutable file overwrite')
        check_site(previous);check_site(files);self.observe(previous)
        candidate=dict(files)
        candidate['deployment.json']=encode(dict(schema=1,transaction=owner['transaction'],operation=operation))
        changed={n:b for n,b in candidate.items() if previous.get(n)!=b}
        p=dict(schema=1,operation=operation,release=release,previous_head=head,
               previous_digest=digest(previous),candidate_digest=digest(candidate),
               candidate_files=inventory(candidate),changed=sorted(changed),
               before=channels(previous),after=channels(candidate),**owner)
        guarded=dict(previous);guarded[PENDING]=encode(p)
        guarded.update({CONTROL+'files/'+n:b for n,b in changed.items()})
        guard_head=self.store.cas(head,guarded,'Pending Pages '+owner['transaction'])
        # No acknowledgement/readback means NO publication; a possibly written guard blocks retries.
        actual,stored=self.store.read()
        if actual!=guard_head or stored!=guarded:raise ValueError('Guard durability not established')
        return dict(transaction=p['transaction'],guard_head=guard_head),candidate
    def finish(self,receipt,owner=None,recovery=None,settle=None):
        head,files=self.store.read();p,previous,candidate=pending(files)
        if p['transaction']!=receipt['transaction'] or head!=receipt['guard_head']:
            raise ValueError('Stale transaction/guard head; no mutation')
        if recovery is None:
            if owner!=p['transaction']:raise ValueError('Finalizer is not the publication owner')
            chosen=candidate
        else:
            if recovery['outcome'] not in ('adopt-candidate','retain-previous'):raise ValueError('Explicit recovery outcome required')
            chosen=candidate if recovery['outcome']=='adopt-candidate' else previous
            if channels(chosen)!={'stable':recovery['stable'],'dev':recovery['dev']}:
                raise ValueError('Explicit recovery channel selections disagree')
            if settle is None:raise ValueError('Publication settlement evidence required')
            result=settle(p)  # Original run and exact Pages request must be settled.
            if result not in ('candidate','previous','observed'):
                raise ValueError('Missing settlement result')
            if result=='candidate' and recovery['outcome']!='adopt-candidate':
                raise ValueError('Known successful publication cannot be reconciled to older state')
            if result=='previous' and recovery['outcome']!='retain-previous':
                raise ValueError('Undispatched publication cannot adopt an unpublished candidate')
        self.observe(chosen)  # No fallback to another snapshot on a failed/missing/ambiguous observation.
        self.store.cas(head,chosen,'Reconciled Pages '+p['transaction'])
        # A lost final acknowledgement may leave an already reconciled state. Never retry a different choice.
        _,actual=self.store.read()
        if actual!=chosen:raise ValueError('Final state durability not established')
        return chosen


class GitStore:
    """Only the dedicated branch is writable. No force push, source index, hooks or Git config edits."""
    def __init__(self):
        if os.environ.get('GITHUB_REPOSITORY')!='Auberg1ne/Felucca':raise ValueError('Unexpected repository')
        token=os.environ.get('GH_TOKEN')
        if not token:raise ValueError('Scoped Actions token unavailable')
        self.env=dict(os.environ,GIT_CONFIG_COUNT='1',GIT_CONFIG_KEY_0='http.https://github.com/.extraheader',
                      GIT_CONFIG_VALUE_0='AUTHORIZATION: basic '+base64.b64encode(('x-access-token:'+token).encode()).decode())
        self.remote='https://github.com/Auberg1ne/Felucca.git'
    def git(self,root,*args,**kw):
        r=subprocess.run(['git','-C',str(root),*args],env=self.env,capture_output=True,**kw)
        if r.returncode:raise ValueError('State Git operation failed (no automatic retry): '+str(args[0]))
        return r.stdout
    def read(self):
        with tempfile.TemporaryDirectory(prefix='pages-state-read-') as d:
            root=Path(d);self.git(root,'init','--quiet')
            r=subprocess.run(['git','ls-remote','--exit-code',self.remote,'refs/heads/'+BRANCH],env=self.env,capture_output=True)
            if r.returncode==2:return None,{}
            if r.returncode:raise ValueError('Cannot read retained state; not treated as blank')
            fields=r.stdout.decode().split()
            if len(fields)!=2 or fields[1]!='refs/heads/'+BRANCH:raise ValueError('Ambiguous state ref')
            head=fields[0]
            self.git(root,'fetch','--quiet','--depth=1',self.remote,'refs/heads/'+BRANCH)
            if self.git(root,'rev-parse','FETCH_HEAD').decode().strip()!=head:raise ValueError('State changed while reading')
            data=self.git(root,'archive','--format=tar',head)
            out=root/'files';out.mkdir()
            with tarfile.open(fileobj=io.BytesIO(data)) as t:t.extractall(out,filter='data')
            return head,read_tree(out)
    def cas(self,expected,files,message):
        # This production path is deliberately not executed by local unit tests.
        if os.environ.get('GITHUB_ACTIONS')!='true' or os.environ.get('GITHUB_JOB') not in ('publish','reconcile'):
            raise ValueError('Remote mutation is restricted to approved Actions jobs')
        actual,_=self.read()
        if actual!=expected:raise ValueError('State compare-and-swap conflict')
        with tempfile.TemporaryDirectory(prefix='pages-state-write-') as d:
            root=Path(d);self.git(root,'init','--quiet')
            if expected:self.git(root,'fetch','--quiet','--depth=1',self.remote,expected)
            write_tree(root/'tree',files)
            env=self.env.copy();env.update(GIT_WORK_TREE=str(root/'tree'),GIT_AUTHOR_NAME='github-actions[bot]',
                GIT_AUTHOR_EMAIL='41898282+github-actions[bot]@users.noreply.github.com',
                GIT_COMMITTER_NAME='github-actions[bot]',GIT_COMMITTER_EMAIL='41898282+github-actions[bot]@users.noreply.github.com')
            def run(*args,**kw):
                r=subprocess.run(['git','-C',str(root/'tree'),*args],env=env,capture_output=True,**kw)
                if r.returncode:raise ValueError('Durable state update failed or acknowledgement uncertain; reconcile before retry')
                return r.stdout
            run('read-tree','--empty')
            if files:run('add','--pathspec-from-file=-','--pathspec-file-nul',input=b''.join(n.encode()+b'\0' for n in sorted(files)))
            tree=run('write-tree').decode().strip()
            head=run('commit-tree',tree,*(['-p',expected] if expected else []),input=(message+'\n').encode()).decode().strip()
            run('push',self.remote,head+':refs/heads/'+BRANCH)
            return head


def gh_json(path):
    r=subprocess.run(['gh','api',path],capture_output=True,text=True)
    if r.returncode:raise ValueError('Authenticated recovery evidence unavailable; guard retained')
    return json.loads(r.stdout)


def settled(p):
    """Authenticated original-attempt evidence; never infer settlement from a page merely loading."""
    prefix='repos/Auberg1ne/Felucca'
    attempt=f"{prefix}/actions/runs/{p['run_id']}/attempts/{p['run_attempt']}"
    run=gh_json(attempt)
    if run.get('status')!='completed' or run.get('run_attempt')!=p['run_attempt']:
        raise ValueError('Original workflow attempt is not conclusively completed')
    jobs=gh_json(attempt+'/jobs?per_page=100')
    if jobs.get('total_count',101)>100:raise ValueError('Incomplete original job evidence')
    job=[j for j in jobs['jobs'] if j['name']=='publish']
    if len(job)!=1 or job[0].get('status')!='completed':raise ValueError('Original publisher not settled')
    step=[s for s in job[0].get('steps',[]) if s['name']==PUBLISH_STEP]
    if len(step)!=1:raise ValueError('Cannot prove whether Pages publication was dispatched')
    if step[0].get('conclusion')=='skipped':return 'previous'
    r=subprocess.run(['gh','api',f"{prefix}/actions/jobs/{job[0]['id']}/logs"],capture_output=True,text=True)
    if r.returncode:raise ValueError('Original publisher logs unavailable; guard retained')
    ids=re.findall(r'Created deployment for ([0-9a-f]{40}), ID: ([A-Za-z0-9._-]+)',r.stdout)
    if len(ids)!=1 or ids[0][0]!=p['workflow_sha']:raise ValueError('Cannot bind Pages request to guarded workflow')
    status=gh_json(f'{prefix}/pages/deployments/{ids[0][1]}')
    if status.get('status') not in TERMINAL:raise ValueError('Pages publication is not conclusively terminal')
    return 'candidate' if status['status']=='succeed' else 'observed'


def observe(files):
    with tempfile.TemporaryDirectory(prefix='pages-live-check-') as d:
        root=Path(d);write_tree(root,files);https.verify_site(root)


def main():
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('command',choices=['check','export','begin','finalize','reconcile'])
    a.add_argument('--site',type=Path);a.add_argument('--receipt',type=Path);a.add_argument('--operation');a.add_argument('--release')
    a.add_argument('--transaction');a.add_argument('--outcome',choices=['adopt-candidate','retain-previous'])
    a.add_argument('--stable');a.add_argument('--dev');args=a.parse_args()
    store=GitStore();t=Transaction(store,observe)
    if args.command=='check':
        _,files=store.read()
        if args.transaction:
            p,_,_=pending(files)
            if p['transaction']!=args.transaction:raise ValueError('Wrong pending transaction')
        else:idle(files)
    elif args.command=='export':
        head,files=t.export();write_tree(args.site,files);args.receipt.write_bytes(encode(dict(head=head)))
    elif args.command=='begin':
        tx=f"{os.environ['GITHUB_RUN_ID']}-{os.environ['GITHUB_RUN_ATTEMPT']}"
        owner=dict(transaction=tx,run_id=int(os.environ['GITHUB_RUN_ID']),run_attempt=int(os.environ['GITHUB_RUN_ATTEMPT']),workflow_sha=os.environ['GITHUB_SHA'])
        receipt,files=t.begin(json.loads(args.receipt.read_text())['head'],read_tree(args.site),args.operation,args.release,owner)
        # Write the stamped candidate only after the durable guard was acknowledged.
        (args.site/'deployment.json').write_bytes(files['deployment.json']);args.receipt.write_bytes(encode(receipt))
    elif args.command=='finalize':
        t.finish(json.loads(args.receipt.read_text()),owner=f"{os.environ['GITHUB_RUN_ID']}-{os.environ['GITHUB_RUN_ATTEMPT']}")
    else:
        if os.environ.get('GITHUB_JOB')!='reconcile':raise ValueError('Explicit approved reconciliation job required')
        head,files=store.read();p,_,_=pending(files)
        if p['transaction']!=args.transaction:raise ValueError('Wrong reconciliation transaction')
        t.finish(dict(transaction=args.transaction,guard_head=head),recovery=dict(outcome=args.outcome,stable=args.stable,dev=args.dev),settle=settled)

if __name__=='__main__':main()
