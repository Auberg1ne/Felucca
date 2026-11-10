#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline H12 checks. All network calls are mocked; no Git commands."""
import copy
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, MagicMock, patch

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('h12', HERE / 'rule_suite_diagnostic.py')
d = importlib.util.module_from_spec(spec)
spec.loader.exec_module(d)


def row():
    return dict(id=12, ref=d.REF, before_sha='0'*40, after_sha=d.COMMIT,
                actor_name=d.WRITER_ACTOR, result='bypass', pushed_at='2026-11-01T12:00:00Z',
                rule_evaluations=[dict(rule_type='creation', enforcement='active', result='pass',
                                       rule_source={'id': 3, 'type': 'ruleset'})])


class FakeAPI:
    def __init__(self):
        self.calls = []
        self.changes = {}

    def get(self, path, **kwargs):
        self.calls.append((path, kwargs))
        if path in self.changes:
            return copy.deepcopy(self.changes[path])
        if path == d.WRITER_APP_PATH:
            return dict(id=5678740, slug='coffee-grains-state-writer', owner={'login':'Auberg1ne'})
        if path == f'/actions/runs/{d.RUN}':
            return dict(id=d.RUN, run_attempt=1, event='workflow_dispatch', head_branch='main',
                        path='.github/workflows/coffee-grains-auth-canary.yml',
                        repository={'full_name': d.REPOSITORY, 'id': 1445456236}, head_sha='a'*40, status='completed',
                        conclusion='failure', created_at='2026-11-01T12:00:00Z',
                        updated_at='2026-11-01T12:10:00Z', raw_secret='DO NOT COPY')
        if path == '/environments/github-pages':
            return dict(name='github-pages', protection_rules=[dict(type='required_reviewers', reviewers=[{}])],
                        deployment_branch_policy=dict(protected_branches=False, custom_branch_policies=True))
        if 'deployment-branch-policies' in path:
            return dict(total_count=1, branch_policies=[dict(name='main', type='branch')])
        if path.startswith('/rules/branches/'):
            return [dict(type=k, ruleset_id=3) for k in ('creation', 'update', 'deletion', 'non_fast_forward')]
        if path == '/git/ref/heads/coffee-grains-state-auth-canary':
            return dict(ref=d.REF, object={'sha': d.COMMIT})
        if path == '/git/ref/heads/coffee-grains-pages-state':
            return None
        if path == '/git/commits/' + d.COMMIT:
            return dict(sha=d.COMMIT, parents=[], message='DO NOT COPY')
        if '/jobs?' in path:
            return dict(total_count=1, jobs=[dict(id=1, conclusion='failure', name='DO NOT COPY')])
        if '/artifacts?' in path:
            return dict(total_count=0, artifacts=[])
        if path == d.SUITES:
            return [row()]
        if path == '/rulesets/rule-suites/12':
            return row()
        raise AssertionError('Unexpected endpoint')


class Diagnostic(unittest.TestCase):
    def set_suite(self, api, data):
        api.changes[d.SUITES] = [data]
        api.changes['/rulesets/rule-suites/' + str(data['id'])] = data

    def assert_inconclusive(self, api):
        env = dict(GITHUB_ACTIONS='true', GITHUB_REPOSITORY=d.REPOSITORY,
                   GITHUB_REF='refs/heads/main', GITHUB_EVENT_NAME='workflow_dispatch',
                   GITHUB_SHA='a'*40, AUDIT_APP_ID='5686993', CANARY_AUDIT_TOKEN='audit-secret')
        with tempfile.TemporaryDirectory() as tmp, patch.dict(os.environ, env, clear=True), \
             patch.object(d, 'REPORT', Path(tmp)/'report.json'), \
             patch.object(d, 'API', return_value=api), patch('builtins.print'):
            self.assertEqual(d.main(), 1)
            data = json.loads(d.REPORT.read_text())
            self.assertEqual(data['status'], 'INCONCLUSIVE')
            self.assertFalse(data['collection_complete'])
            self.assertEqual(data['h9_verdict'], 'NOT_REEVALUATED')
            return data

    def collect(self, api=None):
        report = {}
        api = api or FakeAPI()
        d.collect(api, report, lambda: None)
        return report, api

    def test_complete_fixed_diagnostic_without_h9_verdict(self):
        report, api = self.collect()
        self.assertEqual(report['status'], 'COLLECTED')
        self.assertEqual(report['h9_verdict'], 'NOT_REEVALUATED')
        self.assertTrue(report['suites'][0]['h9_identity_fields_equal'])
        self.assertTrue(report['suites'][0]['fixed_witness_after'])
        self.assertTrue(all(d.endpoint(p) for p, _ in api.calls))
        self.assertNotIn('DO NOT COPY', json.dumps(report))

    def test_overall_fail_is_complete_collection_not_authorization(self):
        api = FakeAPI()
        data = row()
        data['result'] = 'fail'
        data['evaluation_result'] = 'fail'
        data['rule_evaluations'][0]['result'] = 'fail'
        data['rule_evaluations'][0]['details'] = 'RAW FAILURE SECRET'
        self.set_suite(api, data)
        report, _ = self.collect(api)
        self.assertEqual(report['status'], 'COLLECTED')
        self.assertTrue(report['collection_complete'])
        self.assertEqual(report['h9_verdict'], 'NOT_REEVALUATED')
        self.assertEqual(report['suites'][0]['detail']['result'], 'fail')
        self.assertEqual(report['suites'][0]['detail']['evaluation_result'], 'fail')
        self.assertEqual(report['suites'][0]['failed_evaluation_count'], 1)
        self.assertNotIn('RAW FAILURE', json.dumps(report))

    def test_bypass_with_failed_creation_and_other_protections_is_collected(self):
        api = FakeAPI()
        data = row()
        data['evaluation_result'] = 'fail'
        data['rule_evaluations'] = [
            dict(rule_type='creation', enforcement='active', result='fail',
                 rule_source={'type':'ruleset','id':3}),
            dict(rule_type='pull_request', enforcement='active', result='fail',
                 rule_source={'type':'protected_branch'}, details='RAW FAILURE SECRET'),
            dict(rule_type='required_signatures', enforcement='evaluate', result='fail',
                 rule_source={'type':'ruleset','id':99,'name':'RAW RULE NAME'}),
            dict(rule_type='commit_message_pattern', enforcement='deleted ruleset', result='fail',
                 rule_source={'type':'ruleset','id':None})]
        self.set_suite(api, data)
        report, _ = self.collect(api)
        entry = report['suites'][0]
        self.assertEqual(report['status'], 'COLLECTED')
        self.assertEqual(report['h9_verdict'], 'NOT_REEVALUATED')
        self.assertEqual(entry['detail']['result'], 'bypass')
        self.assertEqual(entry['failed_evaluation_count'], 4)
        self.assertEqual([r['rule_type'] for r in entry['detail']['rule_evaluations']],
                         ['creation','pull_request','required_signatures','commit_message_pattern'])
        self.assertIsNone(entry['detail']['rule_evaluations'][1]['rule_source_id'])
        self.assertNotIn('RAW', json.dumps(report))

    def test_unrelated_history_is_classified_and_failures_remain_visible(self):
        api = FakeAPI()
        unrelated = dict(row(), id=13, actor_name='github-actions[bot]', result='fail')
        unrelated['rule_evaluations'] = [dict(rule_type='required_deployments',
            enforcement='active', result='fail', rule_source={'type':'ruleset','id':99})]
        api.changes[d.SUITES] = [unrelated, row()]
        api.changes['/rulesets/rule-suites/13'] = unrelated
        report, _ = self.collect(api)
        self.assertEqual(report['status'], 'COLLECTED')
        self.assertEqual(report['target_suite_count'], 1)
        self.assertEqual(report['unrelated_suite_count'], 1)
        self.assertEqual(report['suites'][0]['classification'], 'TARGET_CREATION')
        self.assertEqual(report['unrelated_suites'][0]['classification'], 'UNRELATED')
        self.assertEqual(report['unrelated_suites'][0]['failed_evaluation_count'], 1)
        self.assertEqual(report['unrelated_suites'][0]['detail']['rule_evaluations'][0]['rule_type'],
                         'required_deployments')
        api.changes[d.SUITES] = [unrelated]
        self.assert_inconclusive(api)

    def test_multiple_failing_target_suites_remain_inconclusive(self):
        api = FakeAPI()
        first = dict(row(), result='fail')
        second = dict(first, id=13)
        api.changes[d.SUITES] = [first, second]
        api.changes['/rulesets/rule-suites/12'] = first
        api.changes['/rulesets/rule-suites/13'] = second
        self.assert_inconclusive(api)

    def test_noncreation_failures_do_not_obscure_exact_creation_event(self):
        api = FakeAPI()
        data = row()
        data['rule_evaluations'] = [dict(rule_type='pull_request', enforcement='active',
            result='fail', rule_source={'type':'protected_branch'})]
        self.set_suite(api, data)
        report, _ = self.collect(api)
        self.assertEqual(report['status'], 'COLLECTED')
        self.assertFalse(report['suites'][0]['current_creation_source_present'])
        self.assertEqual(report['h9_verdict'], 'NOT_REEVALUATED')

    def test_unknown_or_malformed_evaluations_fail_closed(self):
        changes = [dict(rule_type=None), dict(rule_type=''), dict(rule_type=7),
                   dict(rule_type='x'*257), dict(rule_type='bad\nlabel'), dict(result='unknown'),
                   dict(enforcement='unknown'), dict(rule_source={'type':'unknown','id':3}),
                   dict(rule_source={'type':'ruleset','id':False}),
                   dict(rule_source={'id':3}), dict(rule_source=None)]
        for change in changes:
            with self.subTest(change=change):
                api = FakeAPI()
                data = row()
                data['rule_evaluations'][0].update(change)
                self.set_suite(api, data)
                report = self.assert_inconclusive(api)
                self.assertEqual(report['pending_suites'][0]['classification'], 'UNVERIFIED')
        api = FakeAPI()
        data = dict(row(), evaluation_result='unknown')
        self.set_suite(api, data)
        self.assert_inconclusive(api)

    def test_unrelated_malformed_or_inconsistent_records_are_not_ignored(self):
        for change in (dict(actor_name=None), dict(before_sha='malformed'), dict(result='unknown')):
            api = FakeAPI()
            unrelated = dict(row(), id=13, actor_name='github-actions[bot]')
            unrelated.update(change)
            api.changes[d.SUITES] = [row(), unrelated]
            api.changes['/rulesets/rule-suites/13'] = unrelated
            self.assert_inconclusive(api)
        api = FakeAPI()
        unrelated = dict(row(), id=13, actor_name='github-actions[bot]')
        api.changes[d.SUITES] = [row(), unrelated]
        api.changes['/rulesets/rule-suites/13'] = dict(unrelated, after_sha='c'*40)
        self.assert_inconclusive(api)

    def test_failed_collection_cli_exit_zero_keeps_h9_not_reevaluated(self):
        env = dict(GITHUB_ACTIONS='true', GITHUB_REPOSITORY=d.REPOSITORY,
                   GITHUB_REF='refs/heads/main', GITHUB_EVENT_NAME='workflow_dispatch',
                   GITHUB_SHA='a'*40, AUDIT_APP_ID='5686993', CANARY_AUDIT_TOKEN='audit-secret')
        api = FakeAPI()
        data = dict(row(), result='fail')
        data['rule_evaluations'][0]['result'] = 'fail'
        self.set_suite(api, data)
        with tempfile.TemporaryDirectory() as tmp, patch.dict(os.environ, env, clear=True), \
             patch.object(d, 'REPORT', Path(tmp)/'report.json'), \
             patch.object(d, 'API', return_value=api), patch('builtins.print'):
            self.assertEqual(d.main(), 0)
            report = json.loads(d.REPORT.read_text())
            self.assertEqual(report['schema'], 3)
            self.assertEqual(report['status'], 'COLLECTED')
            self.assertEqual(report['h9_verdict'], 'NOT_REEVALUATED')
            self.assertEqual(report['suites'][0]['detail']['result'], 'fail')

    def test_full_history_and_page_bound_with_unrelated_rows(self):
        api = FakeAPI()
        listing = [row()] + [dict(row(), id=i, actor_name='github-actions[bot]') for i in range(13,112)]
        api.changes[d.SUITES] = listing
        for record in listing:
            api.changes['/rulesets/rule-suites/' + str(record['id'])] = record
        api.changes[d.SUITES + '&page=2'] = []
        report, _ = self.collect(api)
        self.assertEqual(report['status'], 'COLLECTED')
        self.assertEqual(report['unrelated_suite_count'], 99)
        with patch.object(d, 'MAX_PAGES', 1):
            self.assert_inconclusive(api)
        api.changes[d.SUITES + '&page=2'] = [dict(row(), id=112)]
        api.changes['/rulesets/rule-suites/112'] = dict(row(), id=112)
        self.assert_inconclusive(api)

    def test_wrong_run_identity_fails_closed(self):
        for field, value in [('id', 1), ('head_branch', 'other'), ('event', 'push'),
                              ('repository', {'full_name':'other/repo'}), ('path', 'other.yml')]:
            api = FakeAPI()
            run = api.get(f'/actions/runs/{d.RUN}')
            run[field] = value
            api.changes[f'/actions/runs/{d.RUN}'] = run
            with self.subTest(field=field), self.assertRaises(d.Unavailable):
                self.collect(api)

    def test_wrong_or_missing_suite_identity_is_inconclusive(self):
        changes = [('actor_name','github-actions[bot]'), ('actor_name','other[bot]'),
                   ('before_sha','b'*40), ('after_sha','c'*40), ('after_sha','main'),
                   ('ref',d.PRODUCTION), ('repository',{'full_name':'other/repo','id':1}),
                   ('pushed_at','2026-99-01T12:00:00Z'),
                   ('repository_id',1), ('result','unknown'), ('pushed_at','2020-01-01T00:00:00Z')]
        changes += [(k,None) for k in ('actor_name','before_sha','after_sha','ref','result','pushed_at')]
        for key, value in changes:
            with self.subTest(key=key,value=value):
                api = FakeAPI()
                bad = dict(row(), **{key:value})
                api.changes[d.SUITES] = [bad]
                api.changes['/rulesets/rule-suites/12'] = bad
                self.assert_inconclusive(api)

    def test_every_list_detail_identity_mismatch_is_inconclusive(self):
        for key, value in [('id',13), ('actor_name','other[bot]'), ('ref',d.PRODUCTION),
                           ('before_sha','b'*40), ('after_sha','c'*40), ('result','pass'),
                           ('pushed_at','2026-11-01T12:00:01Z'), ('repository_id',1445456236)]:
            with self.subTest(key=key):
                api = FakeAPI()
                api.changes['/rulesets/rule-suites/12'] = dict(row(), **{key:value})
                self.assert_inconclusive(api)

    def test_zero_duplicate_and_distinct_multiple_matches_are_inconclusive(self):
        for listing in ([], [row(),row()], [row(),dict(row(),id=13)]):
            api = FakeAPI()
            api.changes[d.SUITES] = listing
            api.changes['/rulesets/rule-suites/13'] = dict(row(),id=13)
            self.assert_inconclusive(api)

    def test_missing_or_malformed_evaluations_are_inconclusive(self):
        for evaluations in (None, [], 'malformed', [dict(rule_type='creation', enforcement='unknown',
                                                        result='pass', rule_source={'id':3,'type':'ruleset'})]):
            api = FakeAPI()
            api.changes['/rulesets/rule-suites/12'] = dict(row(), rule_evaluations=evaluations)
            self.assert_inconclusive(api)

    def test_original_writer_app_id_and_owner_are_verified(self):
        for key,value in [('id',5686993),('slug','other'),('owner',{'login':'other'})]:
            api = FakeAPI()
            api.changes[d.WRITER_APP_PATH] = dict(api.get(d.WRITER_APP_PATH), **{key:value})
            self.assert_inconclusive(api)

    def test_incomplete_run_and_history_are_inconclusive(self):
        for key in ('run_attempt','head_sha','created_at','updated_at'):
            api = FakeAPI()
            api.changes[f'/actions/runs/{d.RUN}'] = dict(api.get(f'/actions/runs/{d.RUN}'), **{key:None})
            self.assert_inconclusive(api)
        api = FakeAPI()
        api.changes[d.SUITES] = [row()]*100
        self.assert_inconclusive(api)

    def test_head_drift_and_missing_canary_never_repaired(self):
        for value in (None, dict(ref=d.REF, object={'sha':'a'*40})):
            api = FakeAPI()
            api.changes['/git/ref/heads/coffee-grains-state-auth-canary'] = value
            with self.assertRaises(d.Unavailable):
                self.collect(api)
            self.assertFalse(any('rule-suites/' in p for p, _ in api.calls))

    def test_production_presence_recorded_without_mutation_or_absence_assumption(self):
        api = FakeAPI()
        api.changes['/git/ref/heads/coffee-grains-pages-state'] = dict(ref=d.PRODUCTION, object={'sha':'b'*40})
        report, _ = self.collect(api)
        self.assertTrue(report['production']['exists'])
        self.assertEqual(report['production']['sha'], 'b'*40)

    def test_environment_policy_failures(self):
        for value in [dict(total_count=1, branch_policies=[dict(name='*', type='branch')]),
                      dict(total_count=1, branch_policies=[dict(name='main', type='tag')])]:
            api = FakeAPI()
            api.changes['/environments/github-pages/deployment-branch-policies?per_page=100'] = value
            with self.assertRaises(d.Unavailable):
                self.collect(api)

    def test_missing_and_duplicate_evidence_not_success(self):
        api = FakeAPI()
        api.changes[d.SUITES] = []
        with self.assertRaises(d.Unavailable):
            self.collect(api)
        api.changes[d.SUITES] = [row(), row()]
        with self.assertRaises(d.Unavailable):
            self.collect(api)

    def test_incomplete_history_fails_before_unique_acceptance(self):
        api = FakeAPI()
        api.changes[d.SUITES] = [row()] * 100
        with patch.object(d, 'MAX_PAGES', 1), self.assertRaises(d.Unavailable):
            self.collect(api)
        api = FakeAPI()
        api.changes[d.SUITES] = [row()] + [dict(row(), id=13)]
        api.changes['/rulesets/rule-suites/13'] = dict(row(), id=13)
        with self.assertRaisesRegex(d.Unavailable, 'Exactly one'):
            self.collect(api)

    def test_missing_actor_does_not_fall_back_to_nested_actor(self):
        api = FakeAPI()
        detail = row()
        detail.pop('actor_name')
        detail['actor'] = {'login': d.WRITER_ACTOR}
        api.changes['/rulesets/rule-suites/12'] = detail
        with self.assertRaises(d.Unavailable):
            self.collect(api)

    def test_sanitizer_drops_unknown_values_and_messages(self):
        data = row()
        data.update(actor_name='private value\nsecret', result='secret', ref='secret', url='secret')
        data['rule_evaluations'][0].update(details='secret', rule_source={'id':'secret','name':'secret'})
        self.assertNotIn('secret', json.dumps(d.suite(data)))
        with self.assertRaises(d.Unavailable):
            d.suite(dict(data, rule_evaluations='malformed'))

    def test_metadata_truncation_stops(self):
        api = FakeAPI()
        api.changes[f'/actions/runs/{d.RUN}/jobs?filter=all&per_page=100'] = dict(total_count=101, jobs=[])
        with self.assertRaises(d.Unavailable):
            self.collect(api)

    def test_final_ref_drift_fails_without_repair(self):
        api = FakeAPI()
        original = api.get
        reads = 0
        def get(path, **kwargs):
            nonlocal reads
            if path == '/git/ref/heads/coffee-grains-state-auth-canary':
                reads += 1
                if reads == 2:
                    return dict(ref=d.REF, object={'sha':'b'*40})
            return original(path, **kwargs)
        api.get = get
        with self.assertRaisesRegex(d.Unavailable, 'Ref changed'):
            self.collect(api)

    def test_cli_collected_report_redacts_credential_shaped_actor(self):
        env = dict(GITHUB_ACTIONS='true', GITHUB_REPOSITORY=d.REPOSITORY,
                   GITHUB_REF='refs/heads/main', GITHUB_EVENT_NAME='workflow_dispatch',
                   GITHUB_SHA='a'*40, AUDIT_APP_ID='5686993',
                   CANARY_AUDIT_TOKEN='audit-secret')
        api = FakeAPI()
        data = dict(row(), actor_name='audit-secret')
        api.changes[d.SUITES] = [data]
        api.changes['/rulesets/rule-suites/12'] = data
        with tempfile.TemporaryDirectory() as tmp, patch.dict(os.environ, env, clear=True), \
             patch.object(d, 'REPORT', Path(tmp)/'report.json'), \
             patch.object(d, 'API', return_value=api), patch('builtins.print'):
            self.assertEqual(d.main(), 1)
            text = d.REPORT.read_text()
            self.assertNotIn('audit-secret', text)
            self.assertIn('[REDACTED]', text)
            self.assertEqual(json.loads(text)['h9_verdict'], 'NOT_REEVALUATED')

    def response(self, raw=b'{}', headers=None):
        response = MagicMock()
        response.__enter__.return_value = response
        response.read.return_value = raw
        response.headers = headers or {}
        return response

    def test_transport_get_only_credential_routing_and_no_fallback(self):
        api = d.API('audit-secret')
        api._opener = Mock()
        api._opener.open.return_value = self.response()
        for path in (d.WRITER_APP_PATH, '/environments/github-pages', '/git/commits/' + d.COMMIT,
                     '/git/ref/heads/coffee-grains-state-auth-canary', f'/actions/runs/{d.RUN}',
                     '/rules/branches/coffee-grains-state-auth-canary?per_page=100',
                     d.SUITES, '/rulesets/rule-suites/12'):
            api.get(path)
            request = api._opener.open.call_args.args[0]
            self.assertEqual(request.get_method(), 'GET')
            self.assertEqual(request.full_url, ('https://api.github.com' if path == d.WRITER_APP_PATH
                                               else d.BASE) + path)
            expected = 'audit-secret'
            self.assertEqual(request.get_header('Authorization'), 'Bearer ' + expected)
        for code in (301, 302, 401, 403, 404, 429, 500):
            api._opener.reset_mock()
            api._opener.open.side_effect = d.urllib.error.HTTPError('secret',code,'secret',{},io.BytesIO(b'secret'))
            with self.assertRaises(d.Unavailable) as error:
                api.get('/environments/github-pages')
            self.assertEqual(str(error.exception), 'GitHub GET HTTP ' + str(code))
            self.assertEqual(api._opener.open.call_count, 1)

    def test_transport_scope_bounds_redirect_and_pagination(self):
        api = d.API('audit')
        api._opener = Mock()
        for path in ('https://evil.test', '/../../dispatches', '/actions/runs/1',
                     '/git/refs', d.SUITES + '&page=11', '/rulesets/rule-suites/1?next=evil'):
            with self.assertRaises(d.Unavailable):
                api.get(path)
        api._opener.open.assert_not_called()
        for response in (self.response(b'x'*(d.MAX_BYTES+1)), self.response(b'not json'),
                         self.response(headers={'Link':'<https://evil>; rel="next"'})):
            api._opener.open.return_value = response
            with self.assertRaises(d.Unavailable):
                api.get('/environments/github-pages')
        api._opener.open.return_value = self.response(b'[]', {'Link':'<https://evil>; rel="next"'})
        with self.assertRaisesRegex(d.Unavailable, 'Short page'):
            api.get(d.SUITES, paginated=True)
        self.assertIsNone(d.NoRedirect().redirect_request(None,None,None,None,None,None))

    def test_404_only_ref_absence(self):
        api = d.API('audit')
        api._opener = Mock()
        api._opener.open.side_effect = d.urllib.error.HTTPError('',404,'',{},io.BytesIO())
        self.assertIsNone(api.get('/git/ref/heads/coffee-grains-state-auth-canary', absent=True))
        with self.assertRaises(d.Unavailable):
            api.get('/environments/github-pages', absent=True)
        with self.assertRaises(d.Unavailable):
            api.get('/git/ref/heads/coffee-grains-state-auth-canary')

    def test_missing_audit_token(self):
        for audit in ('', None):
            with self.assertRaises(d.Unavailable):
                d.API(audit)

    def test_cli_partial_sanitized_failure(self):
        env = dict(GITHUB_ACTIONS='true', GITHUB_REPOSITORY=d.REPOSITORY,
                   GITHUB_REF='refs/heads/main', GITHUB_EVENT_NAME='workflow_dispatch',
                   GITHUB_SHA='a'*40, AUDIT_APP_ID='5686993', CANARY_AUDIT_TOKEN='audit-secret')
        for change in ({}, {'AUDIT_APP_ID':'5678740'}, {'CANARY_APP_TOKEN':'writer-secret'},
                       {'GITHUB_REF':'refs/heads/other'}):
            with tempfile.TemporaryDirectory() as tmp, patch.dict(os.environ, dict(env, **change), clear=True), \
                 patch.object(d, 'REPORT', Path(tmp)/'report.json'), patch.object(d, 'API') as api, \
                 patch('builtins.print'):
                api.return_value.get.side_effect = RuntimeError('audit-secret raw response')
                self.assertEqual(d.main(), 1)
                report = d.REPORT.read_text()
                for secret in ('audit-secret','read-secret','writer-secret','raw response'):
                    self.assertNotIn(secret, report)
                self.assertEqual(json.loads(report)['status'], 'INCONCLUSIVE')
                if change:
                    api.assert_not_called()

    def test_workflow_fixed_scope_read_only_and_pins(self):
        text = (HERE.parents[1]/'.github/workflows/coffee-grains-rule-suite-diagnostic.yml').read_text()
        self.assertIn('environment: github-pages', text)
        self.assertIn("github.ref == 'refs/heads/main'", text)
        self.assertIn('group: coffee-grains-pages', text)
        self.assertIn('cancel-in-progress: false', text)
        self.assertIn('persist-credentials: false', text)
        self.assertIn("app-id: '5686993'", text)
        for bad in ('inputs:', 'contents: write', 'pages: write', 'id-token:', 'STATE_APP',
                    'CANARY_APP_TOKEN:', 'skip-token-revoke:', 'authorization_canary.py probe'):
            self.assertNotIn(bad, text)
        grants = d.re.findall(r'^          permission-([a-z-]+): ([a-z]+)\b', text, d.re.M)
        self.assertEqual(set(grants), {('actions','read'),('administration','read'),('metadata','read'),('contents','read')})
        self.assertNotIn('github.token', text)
        self.assertNotIn('GH_TOKEN:', text)
        self.assertIn('token: ${{ steps.auditor.outputs.token }}', text)
        self.assertIn('    permissions: {}', text)
        for action in d.re.findall(r'uses: (\S+)', text):
            self.assertRegex(action, r'^[^@]+@[0-9a-f]{40}$')
        self.assertLess(text.index('id: auditor'), text.index('uses: actions/checkout@'))
        self.assertLess(text.index('Offline diagnostic checks'), text.index('run: python3 -B tools/hosting/rule_suite_diagnostic.py'))
        self.assertIn('if: always()', text)


if __name__ == '__main__':
    unittest.main(verbosity=2)
