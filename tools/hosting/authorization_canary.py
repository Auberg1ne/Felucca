#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H9 isolated probe. No configurable remote/ref, no production mutation or retries.
Importing this module has no side effects. CLI probe is for approved Actions only.
Local tests inject fake Git and REST adapters; they never contact a remote.
"""
import argparse
import base64
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

REPOSITORY = 'Auberg1ne/Felucca'
REMOTE = 'https://github.com/Auberg1ne/Felucca.git'
REF = 'refs/heads/coffee-grains-state-auth-canary'
PRODUCTION = 'refs/heads/coffee-grains-pages-state' # Read only: absence is an invariant.
ZERO = '0' * 40
SHA = re.compile(r'^[0-9a-f]{40}$')
EVIDENCE = Path('canary-evidence/report.json')


class Inconclusive(Exception):
    """Fixed nonsecret error messages only; never embed remote output/credentials."""


class Failed(Inconclusive):
    """A measured invariant failed, rather than missing evidence."""


def require(condition, message):
    if not condition:
        raise Inconclusive(message)


def context(env):
    # This is a safety gate, NOT token authentication. GitHub enforces identity.
    require(env.get('GITHUB_REPOSITORY') == REPOSITORY and
            env.get('GITHUB_REF') == 'refs/heads/main' and
            env.get('GITHUB_EVENT_NAME') == 'workflow_dispatch' and
            env.get('GITHUB_ACTIONS') == 'true' and
            SHA.fullmatch(env.get('GITHUB_SHA', '')) and
            re.fullmatch(r'[1-9][0-9]*', env.get('GITHUB_RUN_ID', '')) and
            re.fullmatch(r'[1-9][0-9]*', env.get('GITHUB_RUN_ATTEMPT', '')),
            'Not an approved main workflow-dispatch context')


class Git:
    def __init__(self, root):
        self.root = Path(root)
        # Do not inherit keys, tokens, Git traces, helpers, hooks or host Git config.
        self.env = {'PATH': os.environ['PATH'], 'HOME': str(self.root),
                    'GIT_CONFIG_NOSYSTEM': '1', 'GIT_CONFIG_GLOBAL': '/dev/null',
                    'GIT_TERMINAL_PROMPT': '0'}
        for name in ('HTTPS_PROXY', 'HTTP_PROXY', 'NO_PROXY', 'https_proxy',
                     'http_proxy', 'no_proxy', 'SSL_CERT_FILE', 'GIT_SSL_CAINFO'):
            if name in os.environ:
                self.env[name] = os.environ[name]

    def command(self, args, token=None, data=None):
        env = self.env.copy()
        if token:
            env.update(GIT_CONFIG_COUNT='1',
                       GIT_CONFIG_KEY_0='http.https://github.com/.extraheader',
                       GIT_CONFIG_VALUE_0='AUTHORIZATION: basic ' +
                       base64.b64encode(('x-access-token:' + token).encode()).decode())
        try:
            return subprocess.run(['git', '-c', 'core.hooksPath=/dev/null', '-C', str(self.root), *args],
                                  env=env, input=data, capture_output=True, timeout=30)
        except (OSError, subprocess.TimeoutExpired):
            raise Inconclusive('Git execution/network timeout; no retry') from None

    def head(self, token, production=False):
        ref = PRODUCTION if production else REF
        r = self.command(['ls-remote', '--exit-code', REMOTE, ref], token)
        if r.returncode == 2 and not r.stdout:
            return ZERO
        require(r.returncode == 0, 'Remote ref unavailable; not treated as absent')
        fields = r.stdout.decode('ascii', errors='replace').split()
        require(len(fields) == 2 and SHA.fullmatch(fields[0]) and fields[1] == ref,
                'Ambiguous remote head')
        return fields[0]

    def commits(self, run):
        require(self.command(['init', '--quiet']).returncode == 0, 'Local Git init failed')
        r = self.command(['hash-object', '-w', '--stdin'], data=b'H9 isolated authorization witness\n')
        require(r.returncode == 0, 'Local blob creation failed')
        blob = r.stdout.strip()
        r = self.command(['mktree'], data=b'100644 blob ' + blob + b'\twitness.txt\n')
        require(r.returncode == 0, 'Local tree creation failed')
        tree = r.stdout.decode().strip()
        self.env.update(GIT_AUTHOR_NAME='COFFEE GRAINS authorization canary',
                        GIT_AUTHOR_EMAIL='canary@users.noreply.github.com',
                        GIT_COMMITTER_NAME='COFFEE GRAINS authorization canary',
                        GIT_COMMITTER_EMAIL='canary@users.noreply.github.com')
        result = []
        for label in ('A', 'B', 'C'):
            args = ['commit-tree', tree] + (['-p', result[0]] if label == 'B' else [])
            r = self.command(args, data=(run + ' ' + label + '\n').encode())
            sha = r.stdout.decode('ascii', errors='replace').strip()
            require(r.returncode == 0 and SHA.fullmatch(sha), 'Local commit creation failed')
            result.append(sha)
        return result

    def push(self, token, target, operation):
        # Every mutation has this one fixed destination. No caller branch/ref input.
        require(operation in ('create', 'update', 'force', 'delete'), 'Unknown operation')
        require(SHA.fullmatch(target) and (target == ZERO) == (operation == 'delete'),
                'Invalid canary commit')
        spec = ('' if operation == 'delete' else target) + ':' + REF
        args = ['push', '--porcelain'] + (['--force'] if operation == 'force' else [])
        return self.command([*args, REMOTE, spec], token)


class API:
    def __init__(self, token):
        self.token = token

    def get(self, suffix):
        # Read-only API, fixed origin/repository. No arbitrary URL or redirect follow.
        url = 'https://api.github.com/repos/' + REPOSITORY + suffix
        class NoRedirect(urllib.request.HTTPRedirectHandler):
            def redirect_request(self, *args, **kwargs):
                return None
        req = urllib.request.Request(url, headers={'Authorization': 'Bearer ' + self.token,
            'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28'})
        try:
            with urllib.request.build_opener(NoRedirect).open(req, timeout=20) as r:
                require(not r.headers.get('Link'), 'Paginated evidence is incomplete')
                data = r.read(2 * 1024 * 1024 + 1)
                require(len(data) <= 2 * 1024 * 1024, 'API evidence exceeds bound')
                return json.loads(data)
        except urllib.error.HTTPError as exc:
            raise Inconclusive('Read-only GitHub evidence HTTP ' + str(exc.code) + '; no permission workaround') from None
        except (urllib.error.URLError, TimeoutError, ValueError):
            raise Inconclusive('Read-only GitHub evidence unavailable or invalid') from None


def settings(api):
    env = api.get('/environments/github-pages')
    require(env.get('name') == 'github-pages' and any(
        r.get('type') == 'required_reviewers' and r.get('reviewers')
        for r in env.get('protection_rules', [])), 'Missing required-reviewer gate')
    require(env.get('deployment_branch_policy') ==
            {'protected_branches': False, 'custom_branch_policies': True},
            'Environment must use exact selected main branch policy')
    policies = api.get('/environments/github-pages/deployment-branch-policies?per_page=100')
    require(policies.get('total_count') == 1 and
            [(x.get('name'), x.get('type')) for x in policies.get('branch_policies', [])] ==
            [('main', 'branch')], 'Environment is not main-only (tags/wildcards denied)')
    rules = api.get('/rules/branches/coffee-grains-state-auth-canary?per_page=100')
    require(isinstance(rules, list) and len(rules) < 100, 'Incomplete active rule evidence')
    sources = {}
    for kind in ('creation', 'update', 'deletion', 'non_fast_forward'):
        ids = {r.get('ruleset_id') for r in rules if r.get('type') == kind}
        require(ids and all(type(x) is int and x > 0 for x in ids), 'Canary lacks required active rules')
        sources[kind] = ids
    # Detailed bypass lists require settings-owner verification; this endpoint is not proof of them.
    api.get('/rulesets/rule-suites?ref=' + urllib.parse.quote(REF, safe='') + '&per_page=100')
    return sources


def suite_evidence(api, before, after, actor, kind, sources):
    # No push retries. Only bounded read polling for asynchronously visible rule insights.
    for attempt in range(5):
        rows = api.get('/rulesets/rule-suites?ref=' + urllib.parse.quote(REF, safe='') + '&per_page=100')
        require(isinstance(rows, list) and len(rows) < 100, 'Ambiguous rule-suite history')
        matches = [r for r in rows if r.get('ref') == REF and r.get('before_sha') == before and
                   r.get('after_sha') == after and r.get('actor_name') == actor]
        if len(matches) == 1:
            row = matches[0]
            require(type(row.get('id')) is int and row['id'] > 0, 'Invalid rule-suite id')
            detail = api.get('/rulesets/rule-suites/' + str(row['id']))
            require(all(detail.get(k) == row.get(k) for k in
                        ('id', 'ref', 'before_sha', 'after_sha', 'actor_name', 'result')),
                    'Rule-suite detail mismatch')
            failures = [r for r in detail.get('rule_evaluations', [])
                        if r.get('enforcement') == 'active' and r.get('result') == 'fail']
            if kind:
                require(detail.get('result') == 'fail' and failures and all(
                    r.get('rule_type') == kind and r.get('rule_source', {}).get('id') in sources[kind]
                    for r in failures), 'Denial is not exclusively the expected active ruleset rule')
            else:
                require(detail.get('result') in ('pass', 'bypass') and not failures,
                        'App write has no passing ruleset evidence')
            return {'suite_id': row['id'], 'actor': actor, 'result': detail['result'],
                    'expected_rule': kind, 'active_failure_count': len(failures),
                    'failed_ruleset_ids': sorted({r['rule_source']['id'] for r in failures})}
        require(len(matches) < 2, 'Duplicate matching rule suites; inconclusive')
        if attempt < 4:
            time.sleep(1)
    raise Inconclusive('No matching server rule-suite evidence; no retry')


def probe(git, api, ordinary, app, slug, sources, report, save):
    require(ordinary and app and ordinary != app, 'Independent credentials required')
    require(re.fullmatch(r'[a-z0-9-]{1,100}', slug or ''), 'Missing App actor slug')
    require(git.head(ordinary, production=True) == ZERO, 'Production state must remain absent')
    require(git.head(ordinary) == ZERO, 'Canary already exists; no automatic rerun or cleanup')
    a, b, c = git.commits(report['run'])
    operations = [('ordinary-create', ordinary, 'create', ZERO, a, 'creation', 'github-actions[bot]'),
                  ('app-create', app, 'create', ZERO, a, None, slug + '[bot]'),
                  ('ordinary-update', ordinary, 'update', a, b, 'update', 'github-actions[bot]'),
                  ('app-update', app, 'update', a, b, None, slug + '[bot]'),
                  ('app-force', app, 'force', b, c, 'non_fast_forward', slug + '[bot]'),
                  ('app-delete', app, 'delete', b, ZERO, 'deletion', slug + '[bot]')]
    for name, token, operation, before, target, denied_rule, actor in operations:
        require(git.head(ordinary, production=True) == ZERO, 'Production state appeared; stop')
        require(git.head(ordinary) == before, 'Unexpected canary head before operation')
        entry = {'operation': name, 'before': before, 'proposed': target,
                 'started_at': datetime.now(timezone.utc).isoformat(), 'status': 'UNVERIFIED'}
        report['operations'].append(entry)
        save()
        try:
            r = git.push(token, target, operation)
        except Inconclusive:
            # A timeout can hide a successful remote write. Attempt a read, never retry/repair.
            try:
                entry['observed'] = git.head(ordinary)
            except Inconclusive:
                entry['observed'] = 'UNVERIFIED'
            save()
            raise
        # Observe head even after rejected/ambiguous response, before interpreting it.
        try:
            observed = git.head(ordinary)
        except Inconclusive:
            entry.update(returncode=r.returncode, observed='UNVERIFIED')
            save()
            raise
        entry.update(returncode=r.returncode, observed=observed)
        save()
        if denied_rule:
            if observed != before or r.returncode == 0:
                raise Failed('Forbidden operation accepted or changed canary head; no automatic repair')
            require(r.returncode != 0 and b'[remote rejected]' in r.stdout and b'GH013' in r.stderr,
                    'No specific server ruleset rejection; generic/local/network error is inconclusive')
            require(observed == before, 'Denied operation changed canary head; no automatic repair')
        else:
            require(r.returncode == 0 and observed == target, 'App write failed or head mismatch')
        entry.update(suite_evidence(api, before, target, actor, denied_rule, sources), status='PASS',
                     finished_at=datetime.now(timezone.utc).isoformat())
        save()
    require(git.head(ordinary) == b and git.head(ordinary, production=True) == ZERO,
            'Final ref invariant failed')
    report['status'] = 'PASS'
    save()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=['preflight', 'probe'])
    args = parser.parse_args()
    report = {'schema': 1, 'ref': REF, 'status': 'UNVERIFIED', 'operations': []}
    def save():
        EVIDENCE.parent.mkdir(exist_ok=True)
        EVIDENCE.write_text(json.dumps(report, indent=2) + '\n')
    try:
        context(os.environ)
        report['run'] = os.environ['GITHUB_RUN_ID'] + '-' + os.environ['GITHUB_RUN_ATTEMPT']
        report['controller_sha'] = os.environ['GITHUB_SHA']
        save()
        token = os.environ.get('GH_TOKEN', '')
        require(token, 'Ordinary write-capable workflow token missing')
        api = API(token)
        sources = settings(api)
        with tempfile.TemporaryDirectory(prefix='coffee-grains-canary-') as root:
            git = Git(root)
            require(git.head(token, production=True) == ZERO and git.head(token) == ZERO,
                    'Preflight requires absent production and canary refs')
            report['preflight'] = {'environment': 'github-pages', 'main_only': True,
                                   'active_rulesets': {k: sorted(v) for k, v in sources.items()},
                                   'production_absent': True, 'canary_absent': True}
            save()
            if args.phase == 'probe':
                probe(git, api, token, os.environ.get('CANARY_APP_TOKEN', ''),
                      os.environ.get('CANARY_APP_SLUG', ''), sources, report, save)
            else:
                report['status'] = 'PREFLIGHT_PASS'
                save()
        print('Canary phase completed; see nonsecret report.json')
    except Exception as exc:
        report['status'] = 'FAIL' if isinstance(exc, Failed) else 'INCONCLUSIVE'
        # Unexpected exceptions are summarized without exception text or traceback.
        report['reason'] = str(exc) if isinstance(exc, Inconclusive) else 'Unexpected local failure'
        save()
        print('Canary STOPPED: inconclusive or failed invariant; no automatic retry')
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
