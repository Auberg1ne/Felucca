# H9 — isolated GitHub state-authorization canary

Prepared locally from merged controller main
`3a94e3a60e7a8979025059c67ca1b0ad89bb0bde`. No live canary has run.
This is a temporary, manually dispatched workflow, not a deployment. Remove it
through a reviewed PR after collecting authorization evidence. Do not change the
production Pages controller or its permissions to run this test.

## Settings review: required before authorizing publication/dispatch

Read-only Git transport verified expected main and absence of both refs during
preparation and final review. The owner reports independent verification that
both active state rulesets now include the exact production and canary refs.
This records owner-supplied verification, not a new API inspection by this task.
The local execution environment still rejects api.github.com; live reviewer and
main-only policy remain unverified here and are checked again by runtime preflight.

An owner with ruleset write/admin visibility must review all applicable rulesets,
including complete bypass lists (lower-privilege API responses may omit them).
Two options preserve production protections:

1. Add **only** `refs/heads/coffee-grains-state-auth-canary` to the include list
   of each existing complementary state ruleset. Keep production includes,
   exclusions, enforcement, rules and bypass identities unchanged. This is an
   additive canary scope change, not a relaxation for production.
2. If changing those include lists is unsuitable, create the following two
   separate canary-only rulesets. Leave both production rulesets unchanged.

Exact canary-only configurations (manual setup after separate approval):

```json
{
  "name": "Coffee Grains Canary Writer Authorization",
  "target": "branch",
  "enforcement": "active",
  "conditions": {"ref_name": {
    "include": ["refs/heads/coffee-grains-state-auth-canary"], "exclude": []}},
  "bypass_actors": [
    {"actor_id": 5678740, "actor_type": "Integration", "bypass_mode": "always"}
  ],
  "rules": [{"type": "creation"}, {"type": "update"}]
}
```

```json
{
  "name": "Coffee Grains Canary History Retention",
  "target": "branch",
  "enforcement": "active",
  "conditions": {"ref_name": {
    "include": ["refs/heads/coffee-grains-state-auth-canary"], "exclude": []}},
  "bypass_actors": [],
  "rules": [{"type": "deletion"}, {"type": "non_fast_forward"}]
}
```

Do not combine these with an App bypass on both. Do not add the GitHub Actions
App, administrators, teams or broad roles to either bypass list for this test.
Check other overlapping rules for incompatible requirements. No settings were
changed during preparation.

Settings → Environments → github-pages must have required reviewers and selected
deployment branches containing exactly branch `main` (not a wildcard or tag).
Verify the owner-provisioned environment secret
`COFFEE_GRAINS_STATE_APP_PRIVATE_KEY` exists by name, without viewing its value.
Do not put it in repository secrets. The workflow job is gated by that environment;
its read-only preflight checks the reviewer and exact main-only API policies before
minting the App token. App installation remains only Auberg1ne/Felucca,
Contents-write plus mandatory Metadata-read. No new App permission is requested.

## Execution boundary (future approval required)

- Only workflow_dispatch, with **no inputs**; exact repository and main-only gate.
- Check out the triggering trusted main SHA, persisted credentials disabled.
- Shared `coffee-grains-pages` concurrency prevents overlapping controller work;
  cancellation is not used to start a replacement run. No Pages or OIDC permission.
- Ordinary GITHUB_TOKEN has explicit **Contents-write**, solely for the negative
  control. This deliberately differs from the Contents-read production controller.
- A distinct read-only audit App token uses Actions-read, Administration-read and
  mandatory Metadata-read, exact owner/repo and default post-job revocation.
- Protected App5678740 token uses the same pinned v2.1.4 action, exact owner/repo,
  Contents-write only and default post-job revocation, after audit preflight passes.
- No simulator code, source revisions, cache, downloaded executable or artifact
  input is executed. The workflow first runs offline tests, then read-only
  audit-token minting/preflight, then mints the writer and runs the six operations.
- Neither credentials nor raw Git/API output is logged or saved. Only fixed
  status messages and allowlisted JSON fields are emitted. Authorization headers
  are process-local, never Git config files or command-line arguments. Subprocess
  environments omit inherited tokens, keys and Git trace/helper configuration.

The only mutating destination compiled into the script is
`refs/heads/coffee-grains-state-auth-canary`. Production ref is used only for
absence checks; no command-line branch/remote override exists. Private temporary
Git storage holds witness commits A, B (child of A), and independent root C.

## Six operations and decisive evidence

| Operation | Credential | Before → proposed | Expected server result | Observed head |
|---|---|---|---|---|
| 1 create | ordinary Contents-write | absent → A | creation rule rejects | absent |
| 2 create | App | absent → A | allowed | A |
| 3 fast-forward | ordinary Contents-write | A → B | update rule rejects | A |
| 4 fast-forward | App | A → B | allowed | B |
| 5 non-fast-forward, force | App | B → C | non_fast_forward rule rejects | B |
| 6 delete | App | B → absent | deletion rule rejects | B |

Exact remote heads are checked before and after each attempted push. A denial
requires **all** of: nonzero push result, receive-side `[remote rejected]`, GH013
ruleset rejection, unchanged head, and a matching GitHub rule suite. Rule evidence
must match exact ref, before/after SHAs and actor (`github-actions[bot]` or the App
slug bot). Only active failures from the expected rule type/source are accepted.
An unrelated failing rule, evaluate-only rule, wrong actor or missing insight is
inconclusive. Allowed operations also require passing/bypassed rule-suite evidence.

These checks distinguish a valid ruleset negative control from read-only tokens,
unauthenticated requests, network failures and local Git rejection. Successful
playback, YAML permissions alone, generic 403s and dry-run pushes are not evidence.
The read-only rule-suite API must be accessible to the separate audit token;
no writer permission or ruleset bypass will be broadened to obtain that evidence. If a GitHub
version reports different actor/SHA/rule semantics, stop for review rather than
loosening matching. Official schema used: github/rest-api-description,
`GET /repos/{owner}/{repo}/rulesets/rule-suites[/{rule_suite_id}]` and
`GET /repos/{owner}/{repo}/rules/branches/{branch}`.

No mutation is retried. Only bounded rule-suite GET polling (five attempts,
one second apart) accommodates insight visibility delay. API pagination,
responses over 2 MiB, errors, redirects, malformed data, unexpected existing refs,
or changed heads stop execution. No force/reset/cleanup is performed afterward.

## Failure, cancellation and evidence

The JSON report is updated before and after every mutation. Artifact
`state-auth-canary-RUN-ATTEMPT` retains only `canary-evidence/report.json`,
including partial failures, for 30 days. GitHub cancellation may prevent artifact
upload/revocation; examine actual remote head and run status before any next step.
Preflight-only success is `PREFLIGHT_PASS`, **not** successful authorization.
Missing evidence is INCONCLUSIVE; a forbidden operation accepted or changing
head is FAIL. Only completion of all six checked operations is PASS.

On success, leave B intact. There is no automatic rerun or branch deletion.
An interrupted run may leave A or B; an existing canary blocks a new run until
review and separately authorized cleanup/recovery. Never use production state as
an alternate target. Never weaken rules to make a test pass.

A real deletion-denial probe is inherently destructive **if the deletion rule is
misconfigured**: it could delete this canary witness. A wrongly permitted force
probe could replace B with C. The script stops without repair; it cannot promise
witness retention when the protection under test is broken. This risk is limited
to the nonproduction canary and must be accepted before live authorization.
Production state is never created, modified or used as a probe.

## Review and live gate

Local tests use fake Git/HTTP adapters plus a real **local-only** commit graph.
No remote writes, dispatch, settings changes, secret access or deployment occurs.
Repository-level trust remains accepted: environment secrets are not workflow
specific; administrators, reviewed main code and pinned credential-bearing actions
remain trusted. App tokens are repository scoped, not cryptographically limited
to the canary ref. Isolation here is enforced by reviewed code and server rules.

**Live canary execution: NOT AUTHORIZED by this source-publication task.**
Canary coverage is independently verified according to the owner. Before dispatch,
confirm full bypass lists and github-pages reviewer/main-only policy, merge the
reviewed canary PR, and obtain explicit authorization for the one canary run.
Secret existence is owner-confirmed; actual
minting, token permissions, server rule decisions and API evidence remain
unverified until that run. Production DEV bootstrap is a separate later gate.

## Local validation recorded

From this isolated H9 worktree, with no credential or network dependency:

```sh
python3 -B tools/hosting/authorization_canary_test.py
export COFFEE_GRAINS_TEST_SOURCE=/workspace/coffee-grains-pages-source-test
export COFFEE_GRAINS_TEST_SHA=c2be0a28f0f0ed2304d7294dc731ad55b6dbae28
python3 -B tools/hosting/deployment_state_test.py
python3 -B tools/hosting/site_test.py
python3 -B tools/hosting/source_policy_test.py
python3 -B tools/hosting/state_authorization_test.py
```

Original H9 results: H9 **26**, H1 recovery **23**, assembly **12**, source policy **11**,
H8 authorization **8** passed (80 total). Python AST, YAML structure, action SHA
pin shape, 22 shell blocks and whitespace checks passed. No live GitHub workflow,
authorization result, key mint, ruleset modification or production deployment was
tested. These unit checks do not substitute for the pending server-side canary.


## H11 — separate read-only audit credential

The original preflight used the ordinary Contents-write workflow token for all
API requests. Official GitHub documentation for API version 2022-11-28 requires:

| Evidence endpoint (GET only) | Minimum repository permission |
|---|---|
| Environment and deployment branch policies | Actions-read |
| Active rules for the canary branch | Metadata-read |
| Rule-suite list and details | Administration-read |

Adding only `actions: read` to GITHUB_TOKEN is incomplete; Administration is not a
supported workflow GITHUB_TOKEN permission key. Do not use write-all or remove the
rule-suite checks. H11 leaves that ordinary token Contents-write for the genuine
negative controls and leaves State Writer App5678740 Contents-write unchanged.

### Exact owner setup (not performed by this implementation)

The owner reports provisioning Audit App **5686993** and the protected environment
private-key secret. Keep `COFFEE_GRAINS_AUDIT_APP_ID` as a github-pages environment
**variable** with value `5686993`; the PEM belongs only in the environment secret
`COFFEE_GRAINS_AUDIT_APP_PRIVATE_KEY`. This records owner-reported setup, not an
independent inspection of the App grant or a successful live API check. Required
grants remain Actions-read, Administration-read and Metadata-read, no Contents-write.

1. Create a **separate** GitHub App, for example **Coffee Grains Audit Reader**,
   owned by Auberg1ne. It must have a different numeric App ID from **5678740**.
   Disable webhooks if unused. No callback, OAuth flow or event subscription is
   needed for this installation-token-only reader.
2. Set Repository permissions to **Actions: Read-only** and
   **Administration: Read-only**. Keep mandatory **Metadata: Read-only**.
   Leave Contents and all other optional permissions **No access**. No account or
   organization permission is needed. Do not change State Writer App permissions.
3. Install the audit App only on **Auberg1ne/Felucca** using selected repositories.
   Do not add the audit App to either state ruleset bypass list. It needs no Git
   write capability, Pages/OIDC grant, secret-reading permission or settings-write
   permission. Read-only Administration is required for rule-suite evidence.
4. In Settings → Environments → **github-pages**, after verifying its required
   reviewer gate and exact main-only branch policy, add environment variable
   **COFFEE_GRAINS_AUDIT_APP_ID** containing the assigned positive numeric App ID.
   Add environment secret **COFFEE_GRAINS_AUDIT_APP_PRIVATE_KEY** containing its
   generated PEM key. Never put the key in repository-wide secrets or chat.
   Existing **COFFEE_GRAINS_STATE_APP_PRIVATE_KEY** remains unchanged.
5. Review installation scope and permission grants. The pinned token action
   explicitly requests only Actions-read, Administration-read and Metadata-read,
   with owner Auberg1ne and repositories Felucca. The configuration gate rejects
   missing/invalid IDs, App ID5678740 and missing keys without printing them.
6. Review and integrate H11 separately, then obtain explicit approval for any
   future canary dispatch. No App, key, secret, installation, ruleset or workflow
   run has been created by this local implementation.

The workflow mints the reader only after github-pages approval, performs all
read-only API preflight checks, and only then requests the existing writer token.
It supplies `CANARY_AUDIT_TOKEN` solely to preflight and probe. The reader remains
available for rule-suite evidence after each probe; it is never sent to Git.
Both token actions retain default post-job revocation. Cancellation can prevent
revocation, so normal short token expiry remains the backstop.

### Runtime routing and safety

| Credential | Allowed use |
|---|---|
| GH_TOKEN / ordinary workflow token | Git ref reads; negative-control create/update |
| CANARY_APP_TOKEN / State Writer | Authorized create/update and force/delete denial probes |
| CANARY_AUDIT_TOKEN / Audit Reader | GET-only security settings and rule-suite evidence |

A role container requires distinct nonempty ordinary/audit tokens. Probe also
requires a third distinct writer token; preflight rejects an early writer token.
API construction requires that container and selects only its audit member.
Git push and credentialed Git commands explicitly reject the audit member and
any unrecognized credential before subprocess execution. Git subprocesses use
sanitized environments; no audit token/key is inherited. There is no fallback to
ordinary or writer credentials for API errors. Headers, raw Git/API output and
credential values are never included in the allowlisted report.

These checks validate routing, not cryptographic token identity: identities and
scopes are authenticated/minted by GitHub and the pinned token action. The reviewed
workflow binds the exact three outputs to the correct roles. Repository-level
trust is still accepted; secrets are not workflow-specific. The audit reader can
read sensitive security metadata and Actions logs, so protect its key accordingly.
It has no repository mutation permission. Live API effectiveness remains unverified
until the approved run. A 403, missing permission,
ambiguous rule insight or network failure still stops without mutation retry.
Neither full bypass-list visibility nor settings-edit permission is assumed from
Administration-read; retain the owner's independent ruleset verification.

All six operations, exact ref, order, GH013/active-rule/actor/SHA evidence,
production absence checks and environment/concurrency guards are preserved.
The production Pages controller, rulesets, source policy and simulator are unchanged.

Official sources: [GitHub Apps permission tables](https://docs.github.com/en/rest/authentication/permissions-required-for-github-apps),
[workflow token permissions](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#permissions).
The tables were checked from github/docs revision
`fd4c5e6ee88cd56ec15cc7d71324b12bccc3b578`, generated
`src/github-apps/data/shared/entries.json` and `version-index.json`, fpt-2022-11-28.
The pinned token action supports all three explicit read-only permission inputs.

H11 local validation: canary/credential routing **36**, H1 recovery **23**,
assembly **12**, source-policy **11**, H8 authorization **8** passed (**90 total**).
Python AST, YAML/permissions, pinned action SHAs, 24 shell blocks and diff checks
passed. AST comparisons confirmed context, settings validation, rule-suite checks
and the six-operation probe are unchanged from the approved baseline. Tests cover
missing/identical/misrouted credentials, reader exclusion from Git, API audit-only
headers, sanitized child environments, HTTP403 fail-closed behavior, both CLI phases
and distinct audit App configuration. No live audit token was minted or tested.

Recommendation: **PASS for local integration review**. Provisioning is now
owner-reported complete. A live canary still requires reviewed integration and
separate explicit dispatch authorization; live token/API effectiveness is unverified.
