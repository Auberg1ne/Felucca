#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Real transaction/assembly/HTTPS-verifier code, independent durable simulated
Pages and state stores. Never run Git commits/pushes or contact GitHub in tests."""
import base64
from contextlib import redirect_stdout
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('state',HERE/'deployment_state.py')
state=importlib.util.module_from_spec(spec);spec.loader.exec_module(state)
spec=importlib.util.spec_from_file_location('assembly_tests',HERE/'site_test.py')
assembly=importlib.util.module_from_spec(spec);spec.loader.exec_module(assembly)

def persist(path,payload):
    temporary=path.with_suffix('.new')
    with temporary.open('wb') as f:f.write(payload);f.flush();os.fsync(f.fileno())
    temporary.replace(path)
    directory=os.open(path.parent,os.O_DIRECTORY)
    try:os.fsync(directory)
    finally:os.close(directory)

class DiskStore:
    """Atomic durable pointer substitute; includes before/after-write uncertainty."""
    def __init__(self,path,files):
        self.path=path;self.fault=None;self.cas(None,files,'initial')
    def read(self):
        data=json.loads(self.path.read_text())
        return data['head'],{k:base64.b64decode(v) for k,v in data['files'].items()}
    def cas(self,expected,files,message):
        if self.path.exists() and self.read()[0]!=expected:raise ValueError('CAS conflict')
        fault=self.fault;self.fault=None
        if fault=='before':raise OSError('Simulated durable state write failure')
        head=state.digest(files)
        payload=state.encode(dict(head=head,files={k:base64.b64encode(v).decode() for k,v in files.items()}))
        persist(self.path,payload)
        if fault=='after':raise OSError('Simulated lost write acknowledgement')
        return head

class Published:
    def __init__(self,path,files):self.path=path;self.publish(files)
    def publish(self,files):
        # Simulated service persistence separate from retained state.
        persist(self.path,state.encode({k:base64.b64encode(v).decode() for k,v in files.items()}))
    def files(self):return {k:base64.b64decode(v) for k,v in json.loads(self.path.read_text()).items()}
    def observe(self,files):
        actual=self.files()
        with tempfile.TemporaryDirectory() as tmp,redirect_stdout(io.StringIO()):
            root=Path(tmp);state.write_tree(root,files)
            state.https.verify_site(root,get=lambda path,nonce:actual.get(path))

class Safety(unittest.TestCase):
    def setUp(self):
        self.fixture=assembly.Assembly('test_complete_manifest');self.fixture.setUp();self.addCleanup(self.fixture.doCleanups)
        f=self.fixture;self.root=f.root;self.first=f.release;self.second=f.sha+'-2-1';self.third=f.sha+'-3-1'
        state.site.channel(f.out,'dev',self.first);state.site.channel(f.out,'stable',self.first,True)
        state.site.package(f.src,f.build,f.out,f.sha,self.second,'2026-11-09T01:00:00Z')
        state.site.channel(f.out,'dev',self.second)
        files=state.read_tree(f.out)
        self.store=DiskStore(self.root/'retained.json',files);self.live=Published(self.root/'published.json',files)
        self.t=state.Transaction(self.store,self.live.observe);self.serial=10
    def owner(self):
        self.serial+=1
        return dict(transaction=f'{self.serial}-1',run_id=self.serial,run_attempt=1,workflow_sha='e'*40)
    def candidate(self,op='promote-stable',release=None):
        release=release or self.second
        _,files=self.store.read();root=self.root/f'candidate-{self.serial}'
        state.write_tree(root,state.normal(files))
        state.site.channel(root,'stable' if 'stable' in op else 'dev',release,True)
        return state.read_tree(root)
    def begin(self,op='promote-stable',release=None):
        owner=self.owner();head,_=self.t.export()
        receipt,files=self.t.begin(head,self.candidate(op,release),op,release or self.second,owner)
        return receipt,files,owner
    def evidence(self,p,step='success',status='succeed',run_status='completed',log_sha=None):
        prefix='repos/Auberg1ne/Felucca';base=f"{prefix}/actions/runs/{p['run_id']}/attempts/{p['run_attempt']}"
        records={base:dict(status=run_status,run_attempt=p['run_attempt']),
            base+'/jobs?per_page=100':dict(total_count=1,jobs=[dict(name='publish',id=7,status='completed',steps=[dict(name=state.PUBLISH_STEP,conclusion=step)])]),
            prefix+'/pages/deployments/known-id':dict(status=status)}
        def get(path):return records[path]
        result=subprocess.CompletedProcess(['gh'],0,f"Created deployment for {log_sha or p['workflow_sha']}, ID: known-id\n",'')
        with patch.object(state,'gh_json',side_effect=get),patch.object(state.subprocess,'run',return_value=result):
            return state.settled(p)
    def recover(self,receipt,outcome,stable,dev=None,**evidence):
        return self.t.finish(receipt,recovery=dict(outcome=outcome,stable=stable,dev=dev or self.second),
                             settle=lambda p:self.evidence(p,**evidence))
    def assert_sites(self,live_stable,retained_stable,guard):
        _,files=self.store.read()
        self.assertEqual(state.channels(self.live.files())['stable'],live_stable)
        self.assertEqual(state.channels(state.normal(files))['stable'],retained_stable)
        self.assertEqual(state.PENDING in files,guard)
    def assert_blocked(self):
        before=self.store.path.read_bytes();live=self.live.path.read_bytes()
        with self.assertRaises(ValueError):self.t.export()
        # Ordinary DEV/rollback/retry paths all share begin's unconditional idle check.
        for op in ['build-dev','rollback-dev','promote-stable','rollback-stable']:
            with self.assertRaises(ValueError):self.t.begin(self.store.read()[0],{},op,self.first,self.owner())
        self.assertEqual(before,self.store.path.read_bytes());self.assertEqual(live,self.live.path.read_bytes())
    def test_original_failure_reproduces_without_guard(self):
        old=state.normal(self.store.read()[1]);new=self.candidate();self.live.publish(new)
        # Old pipeline's later DEV candidate derives from stale retained snapshot.
        self.assertEqual(state.channels(self.live.files())['stable'],self.second)
        self.assertEqual(state.channels(old)['stable'],self.first)
        with self.assertRaises(ValueError):self.live.observe(old)
        stale=self.root/'old-pipeline-dev';state.write_tree(stale,old)
        f=self.fixture;state.site.package(f.src,f.build,stale,f.sha,self.third,'2026-11-09T02:00:00Z')
        state.site.channel(stale,'dev',self.third)
        for r in (stale/'releases').iterdir():state.site.verify(r)
        self.live.publish(state.read_tree(stale))  # Old workflow deploys its entire stale snapshot.
        self.assert_sites(self.first,self.first,False)  # Actual unintended STABLE rollback reproduced.
    def test_stable_published_state_save_fails_then_reconcile(self):
        r,c,o=self.begin();self.live.publish(c);self.store.fault='before'
        with self.assertRaises(OSError):self.t.finish(r,owner=o['transaction'])
        self.assert_sites(self.second,self.first,True);self.assert_blocked()
        self.recover(r,'adopt-candidate',self.second)
        self.assert_sites(self.second,self.second,False)
        self.assertEqual(self.t.export()[1],self.live.files())
    def test_failed_publication_guard_retained(self):
        r,c,o=self.begin();self.assert_sites(self.first,self.first,True);self.assert_blocked()
        self.recover(r,'retain-previous',self.first,step='failure',status='deployment_failed')
        self.assert_sites(self.first,self.first,False)
    def test_cancel_before_publication(self):
        r,c,o=self.begin();self.assert_blocked()
        self.recover(r,'retain-previous',self.first,step='skipped')
        self.assert_sites(self.first,self.first,False)
    def test_cancel_after_publication_before_finalization(self):
        r,c,o=self.begin();self.live.publish(c);self.assert_blocked()
        self.recover(r,'adopt-candidate',self.second,step='cancelled',status='succeed')
        self.assert_sites(self.second,self.second,False)
    def test_explicit_rollback_failed_save(self):
        r,c,o=self.begin();self.live.publish(c);self.t.finish(r,owner=o['transaction'])
        r,c,o=self.begin('rollback-stable',self.first);self.live.publish(c);self.store.fault='before'
        with self.assertRaises(OSError):self.t.finish(r,owner=o['transaction'])
        self.assert_sites(self.first,self.second,True);self.assert_blocked()
        with self.assertRaises(ValueError):self.recover(r,'adopt-candidate',self.second)
        self.recover(r,'adopt-candidate',self.first)
        self.assert_sites(self.first,self.first,False)
    def test_repeated_recovery_and_retry_never_select_old_stable(self):
        r,c,o=self.begin();self.live.publish(c)
        with self.assertRaises(ValueError):self.recover(r,'retain-previous',self.first)
        with self.assertRaises(ValueError):self.recover(r,'adopt-candidate',self.first)
        with self.assertRaises(ValueError):self.t.finish(r,owner='999-1')
        self.recover(r,'adopt-candidate',self.second)
        for _ in range(2):
            with self.assertRaises(ValueError):self.recover(r,'retain-previous',self.first)
        self.assert_sites(self.second,self.second,False)
    def test_normal_dev_and_stable_after_reconciliation(self):
        r,c,o=self.begin();self.live.publish(c);self.recover(r,'adopt-candidate',self.second)
        owner=self.owner();head,files=self.t.export();root=self.root/'next-dev';state.write_tree(root,files)
        f=self.fixture;state.site.package(f.src,f.build,root,f.sha,self.third,'2026-11-09T02:00:00Z');state.site.channel(root,'dev',self.third)
        r,c=self.t.begin(head,state.read_tree(root),'build-dev',self.third,owner);self.live.publish(c);self.t.finish(r,owner=owner['transaction'])
        self.assert_sites(self.second,self.second,False)
        r,c,o=self.begin('promote-stable',self.third);self.live.publish(c);self.t.finish(r,owner=o['transaction'])
        self.assert_sites(self.third,self.third,False)
    def test_guard_write_failure_never_publishes(self):
        owner=self.owner();head,files=self.t.export();candidate=self.candidate();self.store.fault='before'
        with self.assertRaises(OSError):self.t.begin(head,candidate,'promote-stable',self.second,owner)
        self.assert_sites(self.first,self.first,False)
    def test_lost_guard_ack_is_unresolved_and_blocks(self):
        owner=self.owner();head,files=self.t.export();candidate=self.candidate();self.store.fault='after'
        with self.assertRaises(OSError):self.t.begin(head,candidate,'promote-stable',self.second,owner)
        self.assert_sites(self.first,self.first,True);self.assert_blocked()
    def test_lost_final_ack_is_already_reconciled_not_rolled_back(self):
        r,c,o=self.begin();self.live.publish(c);self.store.fault='after'
        with self.assertRaises(OSError):self.t.finish(r,owner=o['transaction'])
        self.assert_sites(self.second,self.second,False)
        self.assertEqual(self.t.export()[1],self.live.files())
        with self.assertRaises(ValueError):self.recover(r,'retain-previous',self.first)
    def test_live_mixed_unknown_and_unavailable_observations_block(self):
        r,c,o=self.begin();self.live.publish(c)
        broken=self.live.files();broken['stable/build.json']=state.encode(dict(release=self.first));self.live.publish(broken)
        with self.assertRaises(ValueError):self.t.finish(r,owner=o['transaction'])
        self.assert_blocked()
        before=self.store.path.read_bytes()
        with patch.object(self.t,'observe',side_effect=OSError('network unavailable')):
            with self.assertRaises(OSError):self.recover(r,'adopt-candidate',self.second)
        self.assertEqual(before,self.store.path.read_bytes());self.assertIn(state.PENDING,self.store.read()[1])
    def test_stale_receipts_cas_and_immutable_guard(self):
        r,c,o=self.begin();bad=dict(r,guard_head='stale')
        with self.assertRaises(ValueError):self.t.finish(bad,owner=o['transaction'])
        head,files=self.store.read();files[state.CONTROL+'files/deployment.json']=b'corrupt';self.store.cas(head,files,'corrupt guard')
        with self.assertRaises(ValueError):self.recover(r,'adopt-candidate',self.second)
        self.assert_blocked()
    def test_recovery_waits_for_original_run_and_native_pages_terminal_state(self):
        r,c,o=self.begin();self.live.publish(c)
        for status in ['deployment_in_progress','deployment_attempt_error','deployment_lost','unknown_status','not_found']:
            with self.assertRaises(ValueError):self.recover(r,'adopt-candidate',self.second,status=status)
        with self.assertRaises(ValueError):self.recover(r,'adopt-candidate',self.second,run_status='in_progress')
        with self.assertRaises(ValueError):self.recover(r,'adopt-candidate',self.second,log_sha='f'*40)
        self.assert_blocked()
        self.recover(r,'adopt-candidate',self.second)
    def test_known_success_cannot_reconcile_to_stale_cached_previous(self):
        r,c,o=self.begin()
        # Deliberately serve the prior site although the authoritative API says succeed.
        with self.assertRaisesRegex(ValueError,'Known successful'):
            self.recover(r,'retain-previous',self.first,status='succeed')
        self.assert_sites(self.first,self.first,True)
    def test_missing_settlement_proof_cannot_clear_guard(self):
        r,c,o=self.begin();self.live.publish(c)
        with self.assertRaisesRegex(ValueError,'Missing settlement'):
            self.t.finish(r,recovery=dict(outcome='adopt-candidate',stable=self.second,dev=self.second),settle=lambda p:None)
        self.assert_sites(self.second,self.first,True)
    def test_recovery_failure_keeps_guard_and_can_repeat_same_explicit_choice(self):
        r,c,o=self.begin();self.live.publish(c);self.store.fault='before'
        with self.assertRaises(OSError):self.recover(r,'adopt-candidate',self.second)
        self.assert_blocked();self.recover(r,'adopt-candidate',self.second);self.assert_sites(self.second,self.second,False)
    def test_guard_is_durable_across_new_process_objects(self):
        r,c,o=self.begin();fresh=DiskStore.__new__(DiskStore);fresh.path=self.store.path;fresh.fault=None
        another=state.Transaction(fresh,self.live.observe)
        with self.assertRaises(ValueError):another.export()
        self.assert_sites(self.first,self.first,True)
    def test_bootstrap_unknown_existing_site_cannot_be_treated_as_blank(self):
        empty=DiskStore(self.root/'empty.json',{})
        with self.assertRaises(ValueError):state.Transaction(empty,self.live.observe).export()
    def test_dev_cannot_modify_stable_or_existing_release(self):
        o=self.owner();head,files=self.t.export();bad=self.candidate()
        with self.assertRaises(ValueError):self.t.begin(head,bad,'build-dev',self.second,o)
        redirect=dict(files);redirect['stable/index.html']+=b'<script>location="old"</script>'
        with self.assertRaises(ValueError):self.t.begin(head,redirect,'build-dev',self.second,o)
        files['releases/'+self.first+'/emu/felucca.wasm']=b'changed'
        with self.assertRaises(ValueError):self.t.begin(head,files,'build-dev',self.second,o)
        self.assert_sites(self.first,self.first,False)
    def test_production_remote_writer_is_disabled_in_local_tests(self):
        adapter=state.GitStore.__new__(state.GitStore)
        with patch.dict(state.os.environ,{'GITHUB_ACTIONS':'false','GITHUB_JOB':'publish'}),patch.object(state.subprocess,'run') as command:
            with self.assertRaises(ValueError):adapter.cas(None,{},'must not run')
            command.assert_not_called()
    def test_authenticated_settlement_api_failure_preserves_guard(self):
        r,c,o=self.begin();self.live.publish(c);before=self.store.path.read_bytes()
        with patch.object(state,'gh_json',side_effect=ValueError('API unavailable')):
            with self.assertRaises(ValueError):
                self.t.finish(r,recovery=dict(outcome='adopt-candidate',stable=self.second,dev=self.second),settle=state.settled)
        self.assertEqual(before,self.store.path.read_bytes());self.assert_sites(self.second,self.first,True)
    def test_workflow_order_no_failure_cleanup_and_recovery_permissions(self):
        # Structural contract over the actual workflow, no optional YAML dependency in CI.
        s=(HERE.parents[1]/'.github/workflows/coffee-grains-pages.yml').read_text()
        self.assertLess(s.index('deployment_state.py begin'),s.index('uses: actions/deploy-pages@'))
        self.assertLess(s.index('uses: actions/deploy-pages@'),s.index('deployment_state.py finalize'))
        self.assertIn("inputs.operation != 'reconcile'",s)
        recovery=s[s.index('  reconcile:'):]
        self.assertIn('pages: read',recovery);self.assertNotIn('pages: write',recovery)
        self.assertNotIn('id-token: write',recovery);self.assertNotIn('deploy-pages@',recovery)
        self.assertNotIn('if: failure()',s);self.assertIn('cancel-in-progress: false',s)
        self.assertNotIn('continue-on-error:',s)
        build=s[s.index('  build:'):s.index('  publish:')]
        self.assertLess(build.index('deployment_state.py check'),build.index('ref: ${{ inputs.revision }}'))

if __name__=='__main__':unittest.main(verbosity=2)
