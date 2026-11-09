#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline H9 tests. No HTTP requests or remote Git writes are performed."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('canary', HERE/'authorization_canary.py')
c = importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
A, B, C = 'a'*40, 'b'*40, 'c'*40
GIT_ROLES = c.Credentials('token', 'audit-test-value', 'private-test-value', require_writer=True)
SOURCES = {k: {1 if k in ('creation', 'update') else 2}
           for k in ('creation', 'update', 'deletion', 'non_fast_forward')}


class FakeGit:
    def __init__(self, outcomes=None):
        self.ref = c.ZERO
        self.production = c.ZERO
        self.calls = []
        self.outcomes = outcomes or {}

    def head(self, token, production=False):
        return self.production if production else self.ref

    def commits(self, run):
        return A, B, C

    def push(self, token, target, operation):
        self.calls.append((token, target, operation))
        number = len(self.calls)
        success = number in (2, 4)
        if number in self.outcomes:
            success = self.outcomes[number] == 'allow'
            if self.outcomes[number] == 'network':
                return subprocess.CompletedProcess([], 128, b'', b'network failure')
            if self.outcomes[number] == 'local':
                return subprocess.CompletedProcess([], 1, b'[rejected]', b'non-fast-forward')
        if success:
            self.ref = target
        return subprocess.CompletedProcess([], 0 if success else 1,
                                           b'ok' if success else b'[remote rejected]',
                                           b'' if success else b'remote: error: GH013: ruleset violation')


class FakeAPI:
    def __init__(self, changes=None):
        self.changes = changes or {}
        self.paths = []

    def get(self, path):
        self.paths.append(path)
        if path in self.changes:
            return self.changes[path]
        if path == '/environments/github-pages':
            return {'name': 'github-pages', 'protection_rules':
                    [{'type': 'required_reviewers', 'reviewers': [{'reviewer': {'login': 'owner'}}]}],
                    'deployment_branch_policy': {'protected_branches': False, 'custom_branch_policies': True}}
        if 'deployment-branch-policies' in path:
            return {'total_count': 1, 'branch_policies': [{'name': 'main', 'type': 'branch'}]}
        if path.startswith('/rules/branches/'):
            return [{'type': k, 'ruleset_id': next(iter(v))} for k, v in SOURCES.items()]
        if path.startswith('/rulesets/rule-suites'):
            return []
        raise AssertionError('Unknown read-only API path')


class Canary(unittest.TestCase):
    def run_probe(self, git=None, suite=None, ordinary='ordinary', app='app'):
        git = git or FakeGit()
        report = {'run': '123-1', 'operations': [], 'status': 'UNVERIFIED'}
        evidence = {'suite_id': 1, 'actor': 'test', 'result': 'fail'}
        with patch.object(c, 'suite_evidence', side_effect=suite) if suite else patch.object(c, 'suite_evidence', return_value=evidence):
            c.probe(git, FakeAPI(), ordinary, app, 'coffee-grains-state-writer', SOURCES, report, lambda: None)
        return git, report

    def test_six_operations_and_final_witness(self):
        git, report = self.run_probe()
        self.assertEqual(git.calls, [('ordinary', A, 'create'), ('app', A, 'create'),
                                   ('ordinary', B, 'update'), ('app', B, 'update'),
                                   ('app', C, 'force'), ('app', c.ZERO, 'delete')])
        self.assertEqual(git.ref, B)
        self.assertEqual(git.production, c.ZERO)
        self.assertEqual(report['status'], 'PASS')
        self.assertEqual([r['status'] for r in report['operations']], ['PASS']*6)

    def test_existing_canary_blocks_all_mutations(self):
        git = FakeGit();git.ref = A
        with self.assertRaises(c.Inconclusive):self.run_probe(git)
        self.assertFalse(git.calls)

    def test_existing_production_blocks_all_mutations(self):
        git = FakeGit();git.production = A
        with self.assertRaises(c.Inconclusive):self.run_probe(git)
        self.assertFalse(git.calls)

    def test_missing_or_same_credentials(self):
        for ordinary, app in [('', 'app'), ('ordinary', ''), ('ordinary', 'ordinary')]:
            git = FakeGit()
            with self.assertRaises(c.Inconclusive):self.run_probe(git, ordinary=ordinary, app=app)
            self.assertFalse(git.calls)

    def test_network_and_local_failures_not_denials(self):
        for mode in ('network', 'local'):
            git = FakeGit({1: mode})
            with self.assertRaises(c.Inconclusive):self.run_probe(git)
            self.assertEqual(len(git.calls), 1)

    def test_wrongly_allowed_denial_stops_without_repair(self):
        for number in (1, 3, 5, 6):
            git = FakeGit({number: 'allow'})
            with self.assertRaises(c.Inconclusive):self.run_probe(git)
            self.assertEqual(len(git.calls), number)
            self.assertEqual(git.production, c.ZERO)

    def test_app_rejection_stops(self):
        for number in (2, 4):
            git = FakeGit({number: 'reject'})
            with self.assertRaises(c.Inconclusive):self.run_probe(git)
            self.assertEqual(len(git.calls), number)

    def test_missing_server_evidence_stops_no_retry(self):
        git = FakeGit()
        with self.assertRaises(c.Inconclusive):
            self.run_probe(git, suite=c.Inconclusive('No rule suite'))
        self.assertEqual(len(git.calls), 1)

    def test_context_only_main_dispatch_exact_repository(self):
        env = dict(GITHUB_ACTIONS='true', GITHUB_REPOSITORY=c.REPOSITORY,
                   GITHUB_REF='refs/heads/main', GITHUB_EVENT_NAME='workflow_dispatch',
                   GITHUB_SHA=A, GITHUB_RUN_ID='123', GITHUB_RUN_ATTEMPT='1')
        c.context(env)
        for key, value in [('GITHUB_REPOSITORY','fork/Felucca'), ('GITHUB_REF','refs/tags/main'),
                           ('GITHUB_EVENT_NAME','pull_request'), ('GITHUB_SHA','main'),
                           ('GITHUB_RUN_ID','1;echo x')]:
            with self.assertRaises(c.Inconclusive):c.context(dict(env, **{key:value}))

    def test_environment_gate_and_main_only(self):
        c.settings(FakeAPI())
        bad = [('/environments/github-pages', {'name':'github-pages','protection_rules':[]}),
               ('/environments/github-pages/deployment-branch-policies?per_page=100',
                {'total_count':1,'branch_policies':[{'name':'*','type':'branch'}]}),
               ('/environments/github-pages/deployment-branch-policies?per_page=100',
                {'total_count':1,'branch_policies':[{'name':'main','type':'tag'}]})]
        for path, value in bad:
            with self.assertRaises(c.Inconclusive):c.settings(FakeAPI({path:value}))

    def test_all_four_active_rules_required(self):
        path = '/rules/branches/coffee-grains-state-auth-canary?per_page=100'
        rules = FakeAPI().get(path)
        for rule in rules:
            with self.assertRaises(c.Inconclusive):
                c.settings(FakeAPI({path:[r for r in rules if r != rule]}))

    def suite(self, kind='creation', **changes):
        detail = dict(id=12, ref=c.REF, before_sha=c.ZERO, after_sha=A,
                      actor_name='github-actions[bot]', result='fail',
                      rule_evaluations=[{'rule_type':kind,'enforcement':'active',
                                         'result':'fail','rule_source':{'id':1}}])
        detail.update(changes)
        api = FakeAPI()
        api.get = lambda path: detail if path.endswith('/12') else [detail]
        return api

    def test_exact_rule_suite_denial(self):
        result = c.suite_evidence(self.suite(), c.ZERO, A, 'github-actions[bot]', 'creation', SOURCES)
        self.assertEqual(result['suite_id'], 12)

    def test_unrelated_evaluate_or_wrong_rule_failure_is_inconclusive(self):
        for kind, enforcement, source in [('update','active',1), ('creation','evaluate',1), ('creation','active',99)]:
            api=self.suite(rule_evaluations=[{'rule_type':kind,'enforcement':enforcement,
                                            'result':'fail','rule_source':{'id':source}}])
            with self.assertRaises(c.Inconclusive):
                c.suite_evidence(api,c.ZERO,A,'github-actions[bot]','creation',SOURCES)

    def test_wrong_actor_head_or_ref_is_inconclusive(self):
        for changes in [dict(actor_name='other'),dict(after_sha=B),dict(ref=c.PRODUCTION)]:
            with patch.object(c.time,'sleep') as sleep:
                with self.assertRaises(c.Inconclusive):
                    c.suite_evidence(self.suite(**changes),c.ZERO,A,'github-actions[bot]','creation',SOURCES)
                self.assertEqual(sleep.call_count,4)

    def test_success_requires_passing_suite(self):
        with self.assertRaises(c.Inconclusive):
            c.suite_evidence(self.suite(),c.ZERO,A,'github-actions[bot]',None,SOURCES)
        api=self.suite(result='bypass',rule_evaluations=[])
        self.assertEqual(c.suite_evidence(api,c.ZERO,A,'github-actions[bot]',None,SOURCES)['result'],'bypass')

    def test_push_destination_always_fixed_never_production(self):
        with tempfile.TemporaryDirectory() as root:
            git=c.Git(root, GIT_ROLES)
            for operation, target in [('create',A),('update',B),('force',C),('delete',c.ZERO)]:
                with patch.object(git,'command') as run:
                    git.push('token',target,operation)
                    args=run.call_args.args[0]
                    self.assertEqual(args[-1].split(':',1)[1],c.REF)
                    self.assertNotIn(c.PRODUCTION,' '.join(args))
                    self.assertEqual(args[-2],c.REMOTE)
            for operation,target in [('oops',A),('create','main'),('delete',A)]:
                with self.assertRaises(c.Inconclusive):git.push('token',target,operation)

    def test_credentials_only_in_subprocess_header_no_tracing(self):
        with tempfile.TemporaryDirectory() as root,patch.dict(os.environ,{'GIT_TRACE':'1','CANARY_APP_TOKEN':'private-test-value','APP_KEY':'private-test-key'}):
            git=c.Git(root, GIT_ROLES)
            with patch.object(c.subprocess,'run',return_value=subprocess.CompletedProcess([],0,b'',b'')) as run:
                git.push('private-test-value',A,'create')
                args,kw=run.call_args
                self.assertNotIn('private-test-value',str(args))
                self.assertNotIn('private-test-key',str(kw))
                for key in ('GIT_TRACE','CANARY_APP_TOKEN','APP_KEY'):self.assertNotIn(key,kw['env'])
                self.assertIn('AUTHORIZATION: basic ',kw['env']['GIT_CONFIG_VALUE_0'])
                self.assertTrue(kw['capture_output'])

    def test_real_local_commit_graph(self):
        with tempfile.TemporaryDirectory() as root:
            git=c.Git(root, GIT_ROLES);a,b,other=git.commits('123-1')
            self.assertEqual(git.command(['rev-parse',b+'^']).stdout.decode().strip(),a)
            self.assertNotEqual(other,a)
            self.assertNotEqual(git.command(['merge-base','--is-ancestor',b,other]).returncode,0)

    def test_duplicate_rule_suites_inconclusive(self):
        api=self.suite(); original=api.get
        api.get=lambda path: original(path)*2 if not path.endswith('/12') else original(path)
        with self.assertRaises(c.Inconclusive):
            c.suite_evidence(api,c.ZERO,A,'github-actions[bot]','creation',SOURCES)

    def test_git_read_error_never_becomes_absence(self):
        with tempfile.TemporaryDirectory() as root:
            git=c.Git(root, GIT_ROLES)
            for result in [subprocess.CompletedProcess([],128,b'',b'error'),
                           subprocess.CompletedProcess([],0,b'',b'')]:
                with patch.object(git,'command',return_value=result):
                    with self.assertRaises(c.Inconclusive):git.head('reader')
            with patch.object(git,'command',return_value=subprocess.CompletedProcess([],2,b'',b'')):
                self.assertEqual(git.head('reader'),c.ZERO)

    def test_timeout_no_retry_no_raw_exception_text(self):
        with tempfile.TemporaryDirectory() as root:
            git=c.Git(root, GIT_ROLES)
            with patch.object(c.subprocess,'run',side_effect=subprocess.TimeoutExpired('private-test-value',30)) as run:
                with self.assertRaises(c.Inconclusive) as error:git.push('private-test-value',A,'create')
                self.assertNotIn('private-test-value',str(error.exception))
                self.assertEqual(run.call_count,1)

    def test_push_timeout_attempts_head_read_without_retry(self):
        git=FakeGit();events=[]
        original_head=git.head
        def head(*args,**kwargs):
            events.append('read')
            return original_head(*args,**kwargs)
        def timeout(*args):
            events.append('push')
            raise c.Inconclusive('Timeout')
        git.head=head;git.push=timeout
        with self.assertRaises(c.Inconclusive):self.run_probe(git)
        self.assertEqual(events[-2:],['push','read'])
        self.assertEqual(events.count('push'),1)

    def test_api_error_no_raw_body_or_authorization(self):
        api=c.API(c.Credentials('ordinary', 'private-test-value'))
        opener=unittest.mock.Mock()
        opener.open.side_effect=c.urllib.error.URLError('private-test-value')
        with patch.object(c.urllib.request,'build_opener',return_value=opener):
            with self.assertRaises(c.Inconclusive) as error:api.get('/environments/github-pages')
        self.assertNotIn('private-test-value',str(error.exception))

    def test_failure_report_contains_no_credentials(self):
        env=dict(GITHUB_ACTIONS='true',GITHUB_REPOSITORY=c.REPOSITORY,GITHUB_REF='refs/heads/main',
                 GITHUB_EVENT_NAME='workflow_dispatch',GITHUB_SHA=A,GITHUB_RUN_ID='123',
                 GITHUB_RUN_ATTEMPT='1',APP_KEY='private-test-key')
        with tempfile.TemporaryDirectory() as root,patch.dict(os.environ,env,clear=True), \
             patch.object(c,'EVIDENCE',Path(root)/'report.json'),patch('sys.argv',['canary','preflight']), \
             patch('builtins.print'):
            self.assertEqual(c.main(),1)
            data=c.EVIDENCE.read_text()
            self.assertNotIn('private-test-key',data)
            self.assertEqual(json.loads(data)['status'],'INCONCLUSIVE')

    def test_production_appearance_stops_further_operations(self):
        git=FakeGit()
        def suite(*args):
            git.production=A
            return {'suite_id':1}
        with self.assertRaises(c.Inconclusive):self.run_probe(git,suite=suite)
        self.assertEqual(len(git.calls),1)

    def test_missing_identical_and_early_writer_credentials_rejected(self):
        cases=[('', 'audit', '', False), ('ordinary', '', '', False),
               ('same', 'same', '', False), ('ordinary', 'audit', '', True),
               ('ordinary', 'audit', 'ordinary', True), ('ordinary', 'audit', 'audit', True),
               ('ordinary', 'audit', 'writer', False)]
        with patch.object(c.subprocess,'run') as run:
            for ordinary,audit,writer,required in cases:
                with self.assertRaises(c.Inconclusive):
                    c.Credentials(ordinary,audit,writer,require_writer=required)
            run.assert_not_called()

    def test_audit_token_rejected_by_push_and_direct_git_command(self):
        with tempfile.TemporaryDirectory() as root:
            git=c.Git(root,GIT_ROLES)
            with patch.object(c.subprocess,'run') as run:
                for operation,target in [('create',A),('update',B),('force',C),('delete',c.ZERO)]:
                    with self.assertRaises(c.Inconclusive):
                        git.push(GIT_ROLES.audit,target,operation)
                with self.assertRaises(c.Inconclusive):
                    git.command(['ls-remote',c.REMOTE],token=GIT_ROLES.audit)
                run.assert_not_called()

    def test_api_requires_explicit_audit_role_not_raw_writer_or_ordinary(self):
        for token in (GIT_ROLES.writer,GIT_ROLES.ordinary,GIT_ROLES.audit):
            with self.assertRaises(c.Inconclusive):c.API(token)
        self.assertEqual(c.API(GIT_ROLES).token,GIT_ROLES.audit)

    def test_api_authorization_is_only_audit_credential(self):
        response=unittest.mock.MagicMock()
        response.__enter__.return_value=response
        response.headers={}
        response.read.return_value=b'{}'
        opener=unittest.mock.Mock();opener.open.return_value=response
        with patch.object(c.urllib.request,'build_opener',return_value=opener):
            c.API(GIT_ROLES).get('/environments/github-pages')
        request=opener.open.call_args.args[0]
        self.assertEqual(request.get_header('Authorization'),'Bearer '+GIT_ROLES.audit)
        self.assertNotIn(GIT_ROLES.writer,request.get_header('Authorization'))
        self.assertEqual(request.get_method(),'GET')

    def test_api_http403_remains_fail_closed_with_auditor(self):
        import io
        opener=unittest.mock.Mock()
        opener.open.side_effect=c.urllib.error.HTTPError('https://api.github.com',403,'Forbidden',{},io.BytesIO())
        with patch.object(c.urllib.request,'build_opener',return_value=opener):
            with self.assertRaises(c.Inconclusive) as error:
                c.API(GIT_ROLES).get('/rulesets/rule-suites')
        self.assertIn('HTTP 403',str(error.exception))
        for token in (GIT_ROLES.audit,GIT_ROLES.writer):self.assertNotIn(token,str(error.exception))
        self.assertEqual(opener.open.call_count,1)

    def test_cli_rejects_missing_audit_before_api_or_git(self):
        env=dict(GITHUB_ACTIONS='true',GITHUB_REPOSITORY=c.REPOSITORY,GITHUB_REF='refs/heads/main',
                 GITHUB_EVENT_NAME='workflow_dispatch',GITHUB_SHA=A,GITHUB_RUN_ID='123',
                 GITHUB_RUN_ATTEMPT='1',GH_TOKEN='ordinary',CANARY_APP_TOKEN='writer')
        for phase in ('preflight','probe'):
            with tempfile.TemporaryDirectory() as root,patch.dict(os.environ,env,clear=True), \
                 patch.object(c,'EVIDENCE',Path(root)/'report.json'),patch('sys.argv',['canary',phase]), \
                 patch.object(c,'API') as api,patch.object(c.subprocess,'run') as git,patch('builtins.print'):
                self.assertEqual(c.main(),1)
                api.assert_not_called();git.assert_not_called()
                data=c.EVIDENCE.read_text()
                self.assertEqual(json.loads(data)['operations'],[])
                self.assertNotIn('Bearer',data)

    def test_audit_token_not_in_git_child_environment(self):
        with tempfile.TemporaryDirectory() as root,patch.dict(os.environ,{'CANARY_AUDIT_TOKEN':GIT_ROLES.audit}):
            git=c.Git(root,GIT_ROLES)
            with patch.object(c.subprocess,'run',return_value=subprocess.CompletedProcess([],0,b'',b'')) as run:
                git.push(GIT_ROLES.ordinary,A,'create')
            self.assertNotIn('CANARY_AUDIT_TOKEN',run.call_args.kwargs['env'])
            self.assertNotIn(GIT_ROLES.audit,str(run.call_args))

    def test_cli_api_and_git_routing_for_both_phases(self):
        for phase in ('preflight','probe'):
            ordinary,audit,writer='ordinary-secret-123','audit-secret-456','writer-secret-789'
            env=dict(GITHUB_ACTIONS='true',GITHUB_REPOSITORY=c.REPOSITORY,GITHUB_REF='refs/heads/main',
                     GITHUB_EVENT_NAME='workflow_dispatch',GITHUB_SHA=A,GITHUB_RUN_ID='123',
                     GITHUB_RUN_ATTEMPT='1',GH_TOKEN=ordinary,CANARY_AUDIT_TOKEN=audit,
                     CANARY_APP_SLUG='coffee-grains-state-writer')
            if phase=='probe':env['CANARY_APP_TOKEN']=writer
            git=FakeGit();api_tokens=[]
            def settings(api):
                api_tokens.append(api.token)
                return SOURCES
            def suite(api,*args):
                api_tokens.append(api.token)
                return {'suite_id':1}
            with tempfile.TemporaryDirectory() as root,patch.dict(os.environ,env,clear=True), \
                 patch.object(c,'EVIDENCE',Path(root)/'report.json'),patch('sys.argv',['canary',phase]), \
                 patch.object(c,'settings',side_effect=settings),patch.object(c,'suite_evidence',side_effect=suite), \
                 patch.object(c,'Git',return_value=git) as constructor,patch('builtins.print'):
                self.assertEqual(c.main(),0)
                self.assertEqual(set(api_tokens),{audit})
                self.assertEqual(constructor.call_args.args[1].audit,audit)
                report=c.EVIDENCE.read_text()
                for token in (ordinary,audit,writer):self.assertNotIn(token,report)
                if phase=='probe':
                    self.assertEqual([x[0] for x in git.calls],[ordinary,writer,ordinary,writer,writer,writer])
                    self.assertEqual(json.loads(report)['status'],'PASS')
                else:
                    self.assertFalse(git.calls)
                    self.assertEqual(json.loads(report)['status'],'PREFLIGHT_PASS')

    def test_audit_app_configuration_rejects_writer_id_or_missing_key(self):
        text=(HERE.parents[1]/'.github/workflows/coffee-grains-auth-canary.yml').read_text()
        section=text.split('      - name: Require distinct read-only audit App configuration')[1].split('      - name: Obtain repository-scoped')[0]
        script=section.split('        run: |\n')[1]
        script='\n'.join(line[10:] for line in script.splitlines())
        for app_id,key,expected in [('', 'key',1),('5678740','key',1),('05678740','key',1),
                                    ('other','key',1),('123456','',1),('123456','key',0),('5686993','key',0)]:
            result=subprocess.run(['bash','-c',script],env={'AUDIT_APP_ID':app_id,'APP_KEY':key},capture_output=True)
            self.assertEqual(result.returncode,expected)
            self.assertEqual(result.stdout+result.stderr,b'')

    def test_workflow_auditor_permissions_and_token_bindings(self):
        text=(HERE.parents[1]/'.github/workflows/coffee-grains-auth-canary.yml').read_text()
        audit=text.split('        id: auditor')[1].split('      - name: Verify reviewer')[0]
        grants=c.re.findall(r'^          permission-([a-z-]+): ([a-z]+)$',audit,c.re.M)
        self.assertEqual(set(grants),{('actions','read'),('administration','read'),('metadata','read')})
        self.assertIn('app-id: ${{ vars.COFFEE_GRAINS_AUDIT_APP_ID }}',audit)
        self.assertIn('private-key: ${{ secrets.COFFEE_GRAINS_AUDIT_APP_PRIVATE_KEY }}',audit)
        self.assertIn('owner: Auberg1ne',audit)
        self.assertIn('repositories: Felucca',audit)
        writer=text.split('        id: writer')[1].split('      - name: Six isolated')[0]
        self.assertEqual(c.re.findall(r'^          permission-([a-z-]+): ([a-z]+)$',writer,c.re.M),[('contents','write')])
        for label,end in [('Verify reviewer','Require protected'),('Six isolated','Retain nonsecret')]:
            section=text.split('      - name: '+label)[1].split('      - name: '+end)[0]
            self.assertIn('CANARY_AUDIT_TOKEN: ${{ steps.auditor.outputs.token }}',section)
            self.assertIn('GH_TOKEN: ${{ github.token }}',section)
        preflight=text.split('      - name: Verify reviewer')[1].split('      - name: Require protected')[0]
        self.assertNotIn('steps.writer',preflight)
        self.assertEqual(text.count('CANARY_APP_TOKEN: ${{ steps.writer.outputs.token }}'),1)
        self.assertEqual(text.count('uses: actions/create-github-app-token@'),2)

    def test_workflow_isolation_permissions_pins_and_order(self):
        text=(HERE.parents[1]/'.github/workflows/coffee-grains-auth-canary.yml').read_text()
        self.assertIn('workflow_dispatch:',text)
        self.assertNotIn('inputs:',text)
        self.assertIn("github.ref == 'refs/heads/main'",text)
        self.assertIn('environment: github-pages',text)
        self.assertEqual(len(c.re.findall(r'^      contents: write',text,c.re.M)),1)
        self.assertNotIn('pages: write',text)
        self.assertNotIn('id-token: write',text)
        self.assertNotIn('set -x',text)
        self.assertIn('group: coffee-grains-pages',text)
        self.assertIn('cancel-in-progress: false',text)
        self.assertIn('persist-credentials: false',text)
        self.assertIn("app-id: '5678740'",text)
        self.assertIn('owner: Auberg1ne',text)
        self.assertIn('repositories: Felucca',text)
        self.assertIn('permission-contents: write',text)
        self.assertLess(text.index('id: auditor'),text.index('authorization_canary.py preflight'))
        self.assertLess(text.index('authorization_canary.py preflight'),text.index('id: writer'))
        self.assertLess(text.index('uses: actions/create-github-app-token@'),text.index('authorization_canary.py probe'))
        self.assertNotIn('skip-token-revoke:',text)
        for pin in c.re.findall(r'uses: ([^\s]+)',text):self.assertRegex(pin,r'^[^@]+@[0-9a-f]{40}$')

if __name__=='__main__':unittest.main(verbosity=2)
