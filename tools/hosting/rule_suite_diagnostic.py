#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H12: bounded GET-only inspection, never a replacement H9 authorization verdict."""
import json
import hashlib
from datetime import datetime
import os
from pathlib import Path
import re
import urllib.error
import urllib.parse
import urllib.request

REPOSITORY = 'Auberg1ne/Felucca'
RUN = 42700311095
COMMIT = '1d1b9c5a63e7b0c65d16041dc657d8e4a36fa6c4'
WRITER_ACTOR = 'coffee-grains-state-writer[bot]' # App5678740; H9's pinned writer identity.
WRITER_APP_PATH = '/apps/coffee-grains-state-writer'
ZERO = '0' * 40
REF = 'refs/heads/coffee-grains-state-auth-canary'
PRODUCTION = 'refs/heads/coffee-grains-pages-state'
REPORT = Path('build/h12-evidence/report.json')
SHA = re.compile(r'[0-9a-f]{40}')
ACTOR = re.compile(r'[a-zA-Z0-9-]{1,100}(?:\[bot\])?')
STAMP = re.compile(r'\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ')
BASE = 'https://api.github.com/repos/' + REPOSITORY
MAX_BYTES = 2 * 1024 * 1024
MAX_PAGES = 10
RESULTS = ('pass', 'fail', 'bypass')
# Known labels aid interpretation; the rule-suite schema permits other string labels.
RULE_TYPES = ('creation', 'update', 'deletion', 'non_fast_forward', 'required_linear_history',
              'merge_queue', 'required_deployments', 'required_signatures', 'pull_request',
              'required_status_checks', 'commit_message_pattern', 'commit_author_email_pattern',
              'committer_email_pattern', 'branch_name_pattern', 'tag_name_pattern', 'workflows',
              'code_scanning', 'code_quality', 'code_coverage', 'copilot_code_review',
              'license_compliance_scanning', 'file_path_restriction', 'max_file_path_length',
              'file_extension_restriction', 'max_file_size')
ENFORCEMENTS = ('active', 'evaluate', 'deleted ruleset')
SOURCE_TYPES = ('ruleset', 'protected_branch')
SUITES = '/rulesets/rule-suites?ref=' + urllib.parse.quote(REF, safe='') + '&time_period=month&per_page=100'


class Unavailable(Exception):
    """Only fixed nonsecret messages may cross the reporting boundary."""


def require(value, message):
    if not value:
        raise Unavailable(message)


def endpoint(path):
    fixed = {f'/actions/runs/{RUN}', f'/actions/runs/{RUN}/jobs?filter=all&per_page=100',
             f'/actions/runs/{RUN}/artifacts?per_page=100',
             '/environments/github-pages',
             '/environments/github-pages/deployment-branch-policies?per_page=100',
             '/git/ref/heads/coffee-grains-state-auth-canary',
             '/git/ref/heads/coffee-grains-pages-state', '/git/commits/' + COMMIT,
             '/rules/branches/coffee-grains-state-auth-canary?per_page=100'}
    return (path in fixed or path in (SUITES, WRITER_APP_PATH) or
            bool(re.fullmatch(re.escape(SUITES) + r'&page=([2-9]|10)', path)) or
            bool(re.fullmatch(r'/rulesets/rule-suites/[1-9][0-9]{0,18}', path)))


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


class API:
    def __init__(self, token):
        require(isinstance(token, str) and bool(token), 'Protected audit token required')
        self._token = token
        self._opener = urllib.request.build_opener(NoRedirect)

    def get(self, path, absent=False, paginated=False):
        require(endpoint(path), 'Endpoint outside fixed diagnostic scope')
        # 404 means absent only for the two exact ref endpoints, never other evidence.
        require(not absent or path in ('/git/ref/heads/coffee-grains-state-auth-canary',
                                       '/git/ref/heads/coffee-grains-pages-state'),
                'Absence handling outside fixed refs')
        url = 'https://api.github.com' + path if path == WRITER_APP_PATH else BASE + path
        request = urllib.request.Request(url, method='GET', headers={
            'Authorization': 'Bearer ' + self._token,
            'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28'})
        try:
            with self._opener.open(request, timeout=20) as response:
                # Pagination URLs are never followed; fixed numbered paths only.
                require(paginated or not response.headers.get('Link'), 'Incomplete paginated evidence')
                raw = response.read(MAX_BYTES + 1)
                require(len(raw) <= MAX_BYTES, 'Response exceeds diagnostic bound')
                data = json.loads(raw)
                if paginated and response.headers.get('Link'):
                    require(isinstance(data, list) and len(data) == 100,
                            'Short page advertises more history; incomplete evidence')
                return data
        except urllib.error.HTTPError as exc:
            if exc.code == 404 and absent:
                return None
            raise Unavailable('GitHub GET HTTP ' + str(exc.code)) from None
        except (urllib.error.URLError, TimeoutError, OSError, ValueError):
            raise Unavailable('GitHub GET unavailable or invalid JSON') from None


def identity(value, pattern):
    return value if isinstance(value, str) and pattern.fullmatch(value) else None


def positive(value):
    return value if type(value) is int and 0 < value < 10**19 else None


def timestamp(value):
    require(identity(value, STAMP), 'Missing or invalid evidence timestamp')
    try:
        return datetime.strptime(value, '%Y-%m-%dT%H:%M:%SZ')
    except ValueError:
        raise Unavailable('Missing or invalid evidence timestamp') from None


def choice(value, options):
    return value if isinstance(value, str) and value in options else None


def object_data(value):
    require(isinstance(value, dict), 'Expected evidence object')
    return value


def rows(value):
    require(isinstance(value, list) and all(isinstance(x, dict) for x in value),
            'Expected evidence object list')
    return value


def rule_label(value):
    """Preserve opaque labels without copying arbitrary remote text into artifacts."""
    valid = (isinstance(value, str) and 0 < len(value) <= 256 and value.strip() == value
             and value.isprintable())
    known = valid and value in RULE_TYPES
    return {'rule_type': value if known else None,
            'rule_type_status': 'RECOGNIZED' if known else 'UNRECOGNIZED' if valid else 'MALFORMED',
            'rule_type_sha256': hashlib.sha256(value.encode('utf-8')).hexdigest()
                                if valid and not known else None,
            'rule_type_length': len(value) if valid and not known else None}


def suite(value):
    """Allowlist projection: no names, messages, URLs, headers or raw payloads."""
    value = object_data(value)
    result = {'id': positive(value.get('id')),
              'ref': choice(value.get('ref'), (REF, PRODUCTION)),
              'before_sha': identity(value.get('before_sha'), SHA),
              'after_sha': identity(value.get('after_sha'), SHA),
              'actor_name': identity(value.get('actor_name'), ACTOR),
              'result': choice(value.get('result'), RESULTS),
              'evaluation_result': choice(value.get('evaluation_result'), RESULTS),
              'pushed_at': identity(value.get('pushed_at'), STAMP)}
    # Record schema differences without copying arbitrary keys or values.
    result['field_types'] = {key: type(value[key]).__name__ if key in value else 'missing'
                             for key in ('actor_name', 'actor', 'before_sha', 'after_sha',
                                         'result', 'rule_evaluations')}
    actor = value.get('actor')
    result['actor_login'] = identity(actor.get('login'), ACTOR) if isinstance(actor, dict) else None
    if 'rule_evaluations' in value:
        evaluations = rows(value['rule_evaluations'])
        require(len(evaluations) <= 1000, 'Too many rule evaluations')
        result['rule_evaluations'] = [{
            **rule_label(r.get('rule_type')),
            'enforcement': choice(r.get('enforcement'), ENFORCEMENTS),
            'result': choice(r.get('result'), ('pass', 'fail')),
            'rule_source_id': positive(r.get('rule_source', {}).get('id'))
                              if isinstance(r.get('rule_source'), dict) else None,
            'rule_source_type': choice(r.get('rule_source', {}).get('type'),
                                       SOURCE_TYPES)
                               if isinstance(r.get('rule_source'), dict) else None,
            'field_types': {key: type(r[key]).__name__ if key in r else 'missing'
                            for key in ('rule_type', 'enforcement', 'result', 'rule_source')},
        } for r in evaluations]
    return result


def validate_suite(record, repository_id, detail=False):
    """Validate shape/identity fields, never whether a rule allowed the operation."""
    require(record.get('ref') == REF and identity(record.get('before_sha'), SHA) and
            identity(record.get('after_sha'), SHA) and identity(record.get('actor_name'), ACTOR),
            'Suite identity missing, malformed or outside canary scope')
    if 'repository' in record:
        repository = object_data(record['repository'])
        require(repository.get('full_name') == REPOSITORY and
                positive(repository.get('id')) == repository_id, 'Suite repository mismatch')
    if 'repository_id' in record:
        require(positive(record['repository_id']) == repository_id, 'Suite repository mismatch')
    if 'repository_name' in record:
        require(record['repository_name'] == 'Felucca', 'Suite repository mismatch')
    if 'actor_id' in record:
        require(positive(record['actor_id']), 'Suite actor ID malformed')
    require(choice(record.get('result'), RESULTS), 'Unknown or missing suite result')
    if record.get('evaluation_result') is not None:
        require(choice(record['evaluation_result'], RESULTS), 'Unknown evaluation summary result')
    timestamp(record.get('pushed_at'))
    if detail or 'rule_evaluations' in record:
        evaluations = rows(record.get('rule_evaluations'))
        require(0 < len(evaluations) <= 1000, 'Evaluation evidence empty or incomplete')
        for evaluation in evaluations:
            require(rule_label(evaluation.get('rule_type'))['rule_type_status'] != 'MALFORMED' and
                    choice(evaluation.get('enforcement'), ENFORCEMENTS) and
                    choice(evaluation.get('result'), ('pass', 'fail')),
                    'Unknown or malformed rule evaluation')
            source = object_data(evaluation.get('rule_source'))
            require(choice(source.get('type'), SOURCE_TYPES), 'Unknown rule source type')
            # Diagnostic schema permits null/missing source IDs. This is not source verification.
            # Authorization predicates must independently require their exact expected source ID.
            if source.get('id') is not None:
                require(positive(source['id']), 'Malformed rule source ID')


def collect(api, report, save):
    report.update(status='INCONCLUSIVE', collection_complete=False, h9_verdict='NOT_REEVALUATED')
    writer = object_data(api.get(WRITER_APP_PATH))
    require(writer.get('id') == 5678740 and writer.get('slug') == 'coffee-grains-state-writer' and
            object_data(writer.get('owner')).get('login') == 'Auberg1ne',
            'Pinned writer App identity mismatch')
    report['writer'] = {'app_id': 5678740, 'actor': WRITER_ACTOR}
    run = object_data(api.get(f'/actions/runs/{RUN}'))
    require(run.get('id') == RUN and run.get('event') == 'workflow_dispatch' and
            run.get('head_branch') == 'main' and
            run.get('path') == '.github/workflows/coffee-grains-auth-canary.yml' and
            object_data(run.get('repository')).get('full_name') == REPOSITORY,
            'Original H9 run identity mismatch')
    report['original_run'] = {'id': RUN, 'attempt': positive(run.get('run_attempt')),
        'head_sha': identity(run.get('head_sha'), SHA),
        'status': choice(run.get('status'), ('queued', 'in_progress', 'completed', 'waiting', 'pending')),
        'conclusion': choice(run.get('conclusion'), ('success', 'failure', 'cancelled', 'skipped',
                                                   'timed_out', 'action_required', 'neutral', 'stale')),
        'created_at': identity(run.get('created_at'), STAMP),
        'updated_at': identity(run.get('updated_at'), STAMP)}
    require(all(report['original_run'].values()) and run.get('status') == 'completed' and
            positive(run['repository'].get('id')), 'Original run evidence incomplete')
    require(timestamp(run['created_at']) <= timestamp(run['updated_at']), 'Invalid original run window')
    save()
    environment = object_data(api.get('/environments/github-pages'))
    require(environment.get('name') == 'github-pages' and any(
        x.get('type') == 'required_reviewers' and x.get('reviewers')
        for x in rows(environment.get('protection_rules'))), 'Required reviewer gate missing')
    require(environment.get('deployment_branch_policy') ==
            {'protected_branches': False, 'custom_branch_policies': True}, 'Selected branch policy missing')
    policy = object_data(api.get('/environments/github-pages/deployment-branch-policies?per_page=100'))
    require(policy.get('total_count') == 1 and
            [(x.get('name'), x.get('type')) for x in rows(policy.get('branch_policies'))] ==
            [('main', 'branch')], 'Environment is not exactly main-only')
    report['environment'] = {'required_reviewers': True, 'main_only': True}
    rules = rows(api.get('/rules/branches/coffee-grains-state-auth-canary?per_page=100'))
    require(0 < len(rules) < 100, 'Active rule evidence empty or truncated')
    creation_sources = {r.get('ruleset_id') for r in rules if r.get('type') == 'creation'}
    require(creation_sources and all(positive(x) for x in creation_sources),
            'Active creation rules missing')
    require(all(choice(r.get('type'), RULE_TYPES) and positive(r.get('ruleset_id')) for r in rules),
            'Unknown or malformed active rule')
    report['active_rules'] = [{'type': choice(r.get('type'), RULE_TYPES), 'ruleset_id': positive(r.get('ruleset_id'))}
        for r in rules]
    for label in ('canary', 'production'):
        path = '/git/ref/heads/' + ('coffee-grains-state-auth-canary' if label == 'canary'
                                  else 'coffee-grains-pages-state')
        ref = api.get(path, absent=True)
        if ref is None:
            report[label] = {'exists': False}
        else:
            ref = object_data(ref)
            require(ref.get('ref') == (REF if label == 'canary' else PRODUCTION), 'Ref identity mismatch')
            sha = identity(object_data(ref.get('object')).get('sha'), SHA)
            require(sha, 'Ref SHA missing')
            report[label] = {'exists': True, 'sha': sha}
        save()
    require(report['canary'].get('sha') == COMMIT, 'Canary differs from fixed witness; no repair')
    commit = object_data(api.get('/git/commits/' + COMMIT))
    require(commit.get('sha') == COMMIT, 'Witness commit identity mismatch')
    require(rows(commit.get('parents')) == [], 'Creation witness must be an independent root')
    report['witness'] = {'sha': COMMIT, 'parents': [identity(p.get('sha'), SHA)
                         for p in rows(commit.get('parents'))]}
    # Metadata only: never download logs or artifacts (which may contain raw responses).
    for kind, field, extra in (('jobs', 'jobs', '?filter=all&per_page=100'),
                               ('artifacts', 'artifacts', '?per_page=100')):
        data = object_data(api.get(f'/actions/runs/{RUN}/{kind}' + extra))
        items = rows(data.get(field))
        require(type(data.get('total_count')) is int and data['total_count'] == len(items)
                and len(items) < 100, 'Incomplete run metadata')
        report[kind] = [{'id': positive(x.get('id')),
                         'conclusion': choice(x.get('conclusion'), ('success', 'failure', 'cancelled',
                                                                  'skipped', 'timed_out')),
                         'expired': x.get('expired') if type(x.get('expired')) is bool else None}
                        for x in items]
    save()
    report['suites'] = []
    report['unrelated_suites'] = []
    report['pending_suites'] = [] # Partial evidence remains explicitly unvalidated after a stop.
    seen = set()
    for page in range(1, MAX_PAGES + 1):
        listing = rows(api.get(SUITES + (f'&page={page}' if page > 1 else ''), paginated=True))
        require(len(listing) <= 100, 'Oversized rule-suite page')
        for row in listing:
            sid = positive(row.get('id'))
            require(sid and sid not in seen, 'Invalid or duplicate rule-suite identity')
            seen.add(sid)
            require(row.get('ref') == REF, 'Unexpected suite ref')
            detail = object_data(api.get('/rulesets/rule-suites/' + str(sid)))
            require(positive(detail.get('id')) == sid, 'Rule-suite detail identity mismatch')
            entry = {'list': suite(row), 'detail': suite(detail),
                     'h9_identity_fields_equal': all(row.get(k) == detail.get(k) for k in
                         ('id', 'ref', 'before_sha', 'after_sha', 'actor_name', 'result')),
                     'fixed_witness_after': row.get('after_sha') == COMMIT,
                     'zero_before': row.get('before_sha') == '0' * 40,
                     'writer_actor_exact': row.get('actor_name') == WRITER_ACTOR}
            pushed = identity(row.get('pushed_at'), STAMP)
            start, end = (report['original_run'][k] for k in ('created_at', 'updated_at'))
            entry['within_run_window'] = (timestamp(start) <= timestamp(pushed) <= timestamp(end)
                                          if start and end and pushed else None)
            entry['classification'] = 'UNVERIFIED'
            report['pending_suites'].append(entry)
            save()
            validate_suite(row, run['repository']['id'])
            validate_suite(detail, run['repository']['id'], detail=True)
            require(entry['h9_identity_fields_equal'] and
                    all(row.get(k) == detail.get(k) for k in
                        ('pushed_at', 'repository', 'repository_id', 'repository_name',
                         'actor_id', 'evaluation_result')),
                    'Suite list and detail identities disagree')
            if 'rule_evaluations' in row:
                require(suite(row)['rule_evaluations'] == suite(detail)['rule_evaluations'],
                        'Suite list and detail evaluations disagree')
            evaluations = rows(detail.get('rule_evaluations'))
            entry['current_creation_source_present'] = any(r.get('rule_type') == 'creation' and
                    r.get('enforcement') == 'active' and
                    isinstance(r.get('rule_source'), dict) and
                    r['rule_source'].get('id') in creation_sources for r in evaluations)
            entry['failed_evaluation_count'] = sum(r['result'] == 'fail' for r in evaluations)
            entry['unrecognized_rule_type_count'] = sum(
                rule_label(r['rule_type'])['rule_type_status'] == 'UNRECOGNIZED' for r in evaluations)
            target = (row['before_sha'] == ZERO and row['after_sha'] == COMMIT and
                      row['actor_name'] == WRITER_ACTOR)
            entry['classification'] = 'TARGET_CREATION' if target else 'UNRELATED'
            if target:
                require(entry['within_run_window'] is True, 'Target suite outside original run window')
            report['pending_suites'].pop()
            report['suites' if target else 'unrelated_suites'].append(entry)
            save()
        if len(listing) < 100:
            break
    else:
        raise Unavailable('Rule-suite page bound reached; history incomplete')
    report['target_suite_count'] = len(report['suites'])
    report['unrelated_suite_count'] = len(report['unrelated_suites'])
    report['unrecognized_rule_type_count'] = sum(entry['unrecognized_rule_type_count']
        for group in ('suites', 'unrelated_suites') for entry in report[group])
    report['rule_types_fully_recognized'] = report['unrecognized_rule_type_count'] == 0
    save()
    require(len(report['suites']) == 1, 'Exactly one matching creation suite required; missing or ambiguous')
    # Detect concurrent drift without repairing it or asserting production absence.
    for label, branch in (('canary', 'coffee-grains-state-auth-canary'),
                          ('production', 'coffee-grains-pages-state')):
        ref = api.get('/git/ref/heads/' + branch, absent=True)
        expected = report[label]
        if ref is None:
            unchanged = not expected['exists']
        else:
            ref = object_data(ref)
            unchanged = (ref.get('ref') == (REF if label == 'canary' else PRODUCTION) and
                         object_data(ref.get('object')).get('sha') == expected.get('sha'))
        require(unchanged, 'Ref changed during diagnostic; no repair')
    # Absence of evidence (including the API month window) never becomes H9 PASS.
    report['status'] = 'COLLECTED'
    report['collection_complete'] = True
    report['h9_verdict'] = 'NOT_REEVALUATED'
    report['history_window'] = 'month; suites are not proof of run attribution'
    save()


def main():
    report = {'schema': 3, 'status': 'INCONCLUSIVE', 'repository': REPOSITORY,
              'original_run_id': RUN, 'expected_canary_sha': COMMIT,
              'h9_verdict': 'NOT_REEVALUATED', 'collection_complete': False}
    def save():
        REPORT.parent.mkdir(parents=True, exist_ok=True)
        data = json.dumps(report, indent=2) + '\n'
        for name in ('CANARY_AUDIT_TOKEN', 'APP_KEY'):
            secret = os.environ.get(name)
            if secret:
                data = data.replace(secret, '[REDACTED]')
        REPORT.write_text(data)
    try:
        require(os.environ.get('GITHUB_ACTIONS') == 'true' and
                os.environ.get('GITHUB_REPOSITORY') == REPOSITORY and
                os.environ.get('GITHUB_REF') == 'refs/heads/main' and
                os.environ.get('GITHUB_EVENT_NAME') == 'workflow_dispatch' and
                identity(os.environ.get('GITHUB_SHA'), SHA), 'Main-only dispatch context required')
        require(os.environ.get('AUDIT_APP_ID') == '5686993', 'Fixed Audit App required')
        require(not os.environ.get('CANARY_APP_TOKEN'), 'Writer token forbidden')
        save()
        collect(API(os.environ.get('CANARY_AUDIT_TOKEN')), report, save)
    except Exception as exc:
        report['status'] = 'INCONCLUSIVE'
        report['reason'] = str(exc) if isinstance(exc, Unavailable) else 'Unexpected diagnostic failure'
        save()
    print('H12 diagnostic finished; inspect sanitized report.json. H9 verdict unchanged.')
    return 0 if report['status'] == 'COLLECTED' else 1


if __name__ == '__main__':
    raise SystemExit(main())
