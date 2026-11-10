#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic schema tests, separate from the redacted verified fixture.
No network or Git. These generated API values do not reconstruct redacted evidence.
"""
import copy
import hashlib
import json
import unittest

import rule_suite_diagnostic as d
import rule_suite_diagnostic_test as fixtures
import h13_bypass_proposal as proposal


def synthetic_schema_case(unknown_result='pass', unknown_source_type='ruleset'):
    # Arbitrary schema test inputs only; never fill the redacted verified fixture
    # with these labels or source types. Actual known facts are tested separately.
    data = fixtures.row()
    data.update(id=proposal.SUITE_ID, repository_id=proposal.REPOSITORY_ID,
                repository_name='Felucca', result='bypass', evaluation_result='fail')
    data['rule_evaluations'] = [
        dict(rule_type='creation', enforcement='active', result='fail',
             rule_source={'type':'ruleset', 'id':proposal.CREATION_RULESET_ID}),
        dict(rule_type='update', enforcement='active', result='pass',
             rule_source={'type':'ruleset', 'id':proposal.CREATION_RULESET_ID}),
        dict(rule_type='synthetic_unrecognized_rule', enforcement='active', result=unknown_result,
             rule_source={'type':unknown_source_type, 'id':None}, details='RAW FAILURE SECRET'),
        dict(rule_type='deletion', enforcement='active', result='pass',
             rule_source={'type':'ruleset', 'id':99}),
        dict(rule_type='non_fast_forward', enforcement='active', result='pass',
             rule_source={'type':'ruleset', 'id':99}),
        dict(rule_type='required_signatures', enforcement='active', result='pass',
             rule_source={'type':'ruleset', 'id':99})]
    return data


class H13(unittest.TestCase):
    def collect(self, data, detail=None):
        api = fixtures.FakeAPI()
        api.changes[d.SUITES] = [data]
        api.changes['/rulesets/rule-suites/' + str(data['id'])] = data if detail is None else detail
        api.changes['/rules/branches/coffee-grains-state-auth-canary?per_page=100'] = [
            dict(type='creation', ruleset_id=proposal.CREATION_RULESET_ID)]
        report = {}
        d.collect(api, report, lambda:None)
        return report

    def propose(self, data=None, listing=None, detail=None, operation='app-create', sources=None):
        data = synthetic_schema_case() if data is None else data
        return proposal.creation_bypass([data] if listing is None else listing,
            data if detail is None else detail,
            {proposal.CREATION_RULESET_ID} if sources is None else sources, operation=operation)

    def test_opaque_label_retains_all_results_without_authorizing(self):
        for result in ('pass', 'fail'):
            with self.subTest(result=result):
                data = synthetic_schema_case(result)
                report = self.collect(data)
                self.assertEqual(report['status'], 'COLLECTED')
                self.assertTrue(report['collection_complete'])
                self.assertEqual(report['h9_verdict'], 'NOT_REEVALUATED')
                self.assertFalse(report['rule_types_fully_recognized'])
                self.assertEqual(report['unrecognized_rule_type_count'], 1)
                entry = report['suites'][0]
                self.assertEqual(entry['failed_evaluation_count'], 1 + (result == 'fail'))
                unknown = entry['detail']['rule_evaluations'][2]
                self.assertIsNone(unknown['rule_type'])
                self.assertEqual(unknown['rule_type_status'], 'UNRECOGNIZED')
                self.assertEqual(unknown['result'], result)
                self.assertIsNone(unknown['rule_source_id'])
                self.assertEqual(unknown['enforcement'], 'active')
                self.assertEqual(unknown['rule_type_sha256'],
                                 hashlib.sha256(b'synthetic_unrecognized_rule').hexdigest())
                encoded = json.dumps(report)
                self.assertNotIn('synthetic_unrecognized_rule', encoded)
                self.assertNotIn('RAW FAILURE SECRET', encoded)

    def test_unknown_label_difference_is_not_hidden_by_null_projection(self):
        data = synthetic_schema_case()
        detail = copy.deepcopy(data)
        detail['rule_evaluations'][2]['rule_type'] = 'another_unrecognized_rule'
        with self.assertRaises(d.Unavailable):
            self.collect(data, detail)

    def test_unknown_label_never_licenses_unknown_result_enforcement_or_source(self):
        for change in (dict(result='unknown'), dict(enforcement='unknown'),
                       dict(rule_source={'type':'unknown','id':99}),
                       dict(rule_type=None), dict(rule_type=[]), dict(rule_type=''),
                       dict(rule_type='bad\nlabel'), dict(rule_type='x'*257)):
            data = synthetic_schema_case()
            data['rule_evaluations'][2].update(change)
            with self.subTest(change=change), self.assertRaises(d.Unavailable):
                self.collect(data)

    def test_opaque_value_is_not_logged_even_if_it_looks_sensitive(self):
        data = synthetic_schema_case()
        data['rule_evaluations'][2]['rule_type'] = 'Authorization: Bearer synthetic-secret'
        report = self.collect(data)
        self.assertNotIn('Authorization', json.dumps(report))
        self.assertNotIn('synthetic-secret', json.dumps(report))

    def test_proposed_exact_bypass_with_only_creation_failure(self):
        result = self.propose()
        self.assertTrue(result['proposal_only'])
        self.assertTrue(result['creation_bypass_evidence'])
        self.assertEqual(result['failed_creation_ruleset_id'], 26842285)
        self.assertEqual(result['h9_verdict'], 'NOT_REEVALUATED')

    def test_synthetic_six_evaluation_schema_variants_with_null_id(self):
        for source_type in d.SOURCE_TYPES:
            with self.subTest(source_type=source_type):
                data = synthetic_schema_case(unknown_source_type=source_type)
                self.assertEqual(len(data['rule_evaluations']), 6)
                self.assertEqual(sum(e['result'] == 'fail' for e in data['rule_evaluations']), 1)
                report = self.collect(data)
                self.assertEqual(report['status'], 'COLLECTED')
                self.assertEqual(report['h9_verdict'], 'NOT_REEVALUATED')
                self.assertEqual(len(report['suites'][0]['detail']['rule_evaluations']), 6)
                self.assertIsNone(report['suites'][0]['detail']['rule_evaluations'][2]['rule_source_id'])
                self.assertTrue(self.propose(data)['creation_bypass_evidence'])

    def test_no_other_passing_check_can_become_a_failure(self):
        for index in (1, 2, 3, 4, 5):
            data = synthetic_schema_case()
            data['rule_evaluations'][index]['result'] = 'fail'
            with self.subTest(index=index):
                self.assertEqual(self.collect(data)['status'], 'COLLECTED')
                with self.assertRaises(d.Unavailable):
                    self.propose(data)

    def test_null_id_is_never_sufficient_for_expected_creation_failure(self):
        for value in (None, False, '26842285', 99):
            data = synthetic_schema_case()
            data['rule_evaluations'][0]['rule_source']['id'] = value
            with self.subTest(value=value), self.assertRaises(d.Unavailable):
                self.propose(data)

    def test_duplicate_expected_creation_failure_is_not_the_verified_pattern(self):
        data = synthetic_schema_case()
        data['rule_evaluations'].append(copy.deepcopy(data['rule_evaluations'][0]))
        with self.assertRaises(d.Unavailable):
            self.propose(data)

    def test_proposal_rejects_any_additional_failure_including_opaque_evaluate(self):
        for kind in ('synthetic_unrecognized_rule', 'update', 'deletion', 'non_fast_forward',
                     'pull_request', 'required_signatures'):
            for enforcement in ('active', 'evaluate', 'deleted ruleset'):
                data = synthetic_schema_case()
                data['rule_evaluations'][2].update(rule_type=kind, result='fail', enforcement=enforcement)
                with self.subTest(kind=kind,enforcement=enforcement), self.assertRaises(d.Unavailable):
                    self.propose(data)

    def test_proposal_rejects_overall_fail_or_pass_with_creation_failure(self):
        for result in ('fail', 'pass', 'unknown', None):
            with self.subTest(result=result), self.assertRaises(d.Unavailable):
                self.propose(dict(synthetic_schema_case(),result=result))

    def test_proposal_is_not_a_force_delete_update_or_ordinary_exception(self):
        for operation in ('app-force','app-delete','app-update','ordinary-create','create'):
            with self.subTest(operation=operation), self.assertRaises(d.Unavailable):
                self.propose(operation=operation)
        data = synthetic_schema_case()
        data['before_sha'], data['after_sha'] = d.COMMIT, d.ZERO
        with self.assertRaises(d.Unavailable):
            self.propose(data)

    def test_proposal_preserves_exact_actor_repo_ref_sha_and_suite_id(self):
        for key,value in [('actor_name','github-actions[bot]'), ('repository_id',1),
                          ('repository_name','other'), ('ref',d.PRODUCTION), ('before_sha','a'*40),
                          ('after_sha','b'*40), ('id',proposal.SUITE_ID+1)]:
            with self.subTest(key=key), self.assertRaises(d.Unavailable):
                self.propose(dict(synthetic_schema_case(), **{key:value}))

    def test_proposal_preserves_exact_source_and_active_creation_enforcement(self):
        for change in (dict(rule_type='update'),dict(enforcement='evaluate'),
                       dict(rule_source={'type':'ruleset','id':99}),
                       dict(rule_source={'type':'protected_branch','id':proposal.CREATION_RULESET_ID}),
                       dict(rule_source={'type':'ruleset'})):
            data = synthetic_schema_case()
            data['rule_evaluations'][0].update(change)
            with self.subTest(change=change), self.assertRaises(d.Unavailable):
                self.propose(data)
        with self.assertRaises(d.Unavailable):
            self.propose(sources={99})

    def test_proposal_rejects_ambiguous_duplicate_missing_or_inconsistent_suites(self):
        data = synthetic_schema_case()
        for listing in ([], [data,data], [data,dict(data,id=data['id']+1)]):
            with self.assertRaises(d.Unavailable):
                self.propose(listing=listing)
        for key,value in [('actor_name','other[bot]'),('result','pass'),('repository_id',1),
                          ('before_sha','a'*40),('after_sha','b'*40)]:
            with self.subTest(key=key), self.assertRaises(d.Unavailable):
                self.propose(detail=dict(data, **{key:value}))

    def test_proposal_rejects_empty_or_malformed_evaluation_evidence(self):
        for evaluations in ([], None, [None]):
            with self.assertRaises(d.Unavailable):
                self.propose(dict(synthetic_schema_case(),rule_evaluations=evaluations))


if __name__ == '__main__':
    unittest.main(verbosity=2)
