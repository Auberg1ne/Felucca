# H12 — read-only rule-suite diagnostic

This separate manual workflow inspects original H9 run **42700311095** and the
existing `refs/heads/coffee-grains-state-auth-canary` witness at
`1d1b9c5a63e7b0c65d16041dc657d8e4a36fa6c4`. There are no inputs or ref overrides.
It never invokes H9 or resumes its operations. `COLLECTED` means exactly one
consistent creation suite was collected; it is not an H9 authorization PASS.
`h9_verdict` always remains `NOT_REEVALUATED`.
An overall `fail`, or failed evaluations within an overall `bypass`, is diagnostic
evidence and does not prevent complete collection. Exit 0 describes collection
completeness only, never the safety or authorization of the original operation.

## Credential and workflow boundary

`.github/workflows/coffee-grains-rule-suite-diagnostic.yml` is manual-only,
main-only in `Auberg1ne/Felucca`, and gated by the existing protected
**github-pages** environment. Shared `coffee-grains-pages` concurrency does not
cancel other runs. Actions are pinned to full SHAs. Existing environment variable
`COFFEE_GRAINS_AUDIT_APP_ID` must equal **5686993**; the existing environment secret
`COFFEE_GRAINS_AUDIT_APP_PRIVATE_KEY` is the only private key referenced.

The job and workflow ordinary-token permissions are empty. There is no
`github.token`/`GH_TOKEN` input, fallback, writer credential, Pages permission or
OIDC permission. The existing Audit App token is minted after the environment
approval and used explicitly for checkout and **every API read**. Checkout is
pinned to the triggering SHA, with persisted credentials disabled. Offline tests
run after checkout, before collection; they receive no token environment binding.

The token requests only repository-scoped **Actions-read, Administration-read,
Metadata-read and Contents-read**, for owner `Auberg1ne`, repository `Felucca`.
Contents-read is necessary for the existing ref/commit endpoints and checkout.
The owner now reports that Audit App **5686993** has all four read-only grants,
including Contents-read. These grants cover the requested repository API reads
and checkout. This records owner-confirmed provisioning, not a live token test.
No App or installation is changed by this code: if a grant is unavailable,
token minting fails closed. Do not restore an ordinary-token fallback. This task
has no protected audit credential and does not mint or test one locally.

The collector issues only GET requests to compiled repository endpoint paths
under `https://api.github.com/repos/Auberg1ne/Felucca`, plus the one fixed public
App metadata endpoint `/apps/coffee-grains-state-writer`. That identity check
requires App ID **5678740**, the exact slug and owner **Auberg1ne** before accepting
actor **coffee-grains-state-writer[bot]**. It does not mint a writer token, inspect
keys, change the App, or execute any code returned by an API.

Installation-token mint/revoke and artifact upload are permitted workflow
lifecycle operations, separate from GET-only repository inspection. Default
post-job token revocation remains enabled. No Git push or repository mutation,
branch creation/update/deletion, deployment, ruleset change or secret change is
implemented. Existing production/canary refs are observed before and after
collection; drift stops without repair. Production presence is recorded rather
than assumed absent.

## Exact evidence and ambiguity handling

Collection requires the original run ID, repository identity (including a valid
repository ID), dispatch event, main branch, H9 workflow path, completed run,
valid head SHA, attempt and time window. The current environment must retain
required reviewers and exactly the selected `main` branch policy. Active creation
rule source IDs must be available. The canary head and independent-root commit
must match the pinned witness.

Every target suite record must have the exact canary ref, **40 zeroes before**,
the pinned witness **after**, and the exact State Writer actor. For both target
and unrelated records, identity fields must be present in list and detail and
agree on ID, ref, SHAs, actor, result, timestamp and supplied repository metadata.
Repository provenance comes from the fixed endpoint and validated original run;
if a suite supplies repository identity, it must also agree. A nested actor login
never substitutes for a missing `actor_name`.

Exactly **one target suite** must remain after complete bounded history
inspection. Target creation identity is the exact writer actor, canary ref and
zero-before/pinned-after transition; the target timestamp must be in the original
run window. It must have a recognized overall result (`pass`, `fail` or `bypass`)
and nonempty, structurally valid evaluations. Evaluation outcomes may be `pass`
or `fail`; failures are retained, not used as a collection-success predicate.
The current creation-source comparison is an observation only: a historical
suite containing other protections or sources can still be collected.

Well-formed, consistent suites with different actors or SHA transitions are
classified as **UNRELATED** and retained separately, including their failed
evaluations. They neither count as target matches nor substitute for a missing
target. Unknown/malformed unrelated records still stop collection. An unexpected
ref or repository from the fixed scoped endpoint is rejected rather than ignored.

Empty history, nonempty history without the exact target, missing identities or
evaluations, unknown results/rule types/enforcement/source types, duplicate IDs,
inconsistent records, and multiple matching target IDs all produce
**INCONCLUSIVE**, exit 1. Both failed and successful target suites count toward
ambiguity. There is no first-match selection and no outcome-based exclusion.
List/detail evaluation fields must also agree when the list includes them.
H9's predicate remains unchanged; collection never executes it.

History uses the API's **month** window, up to ten pages of 100. Requests use
fixed numbered paths, never server-supplied URLs. A short page advertising more
history, incomplete job/artifact metadata, page-bound exhaustion, malformed or
oversized responses, redirects and HTTP failures stop collection. There are no
request retries. Each response is limited to 2 MiB, each request to 20 seconds,
and the workflow to ten minutes. Evidence outside the month window may be
unavailable. Timestamp/tuple agreement alone is not proof of run attribution.
Current rule sources are not a historical ruleset snapshot.

## Sanitized artifacts

Only `build/h12-evidence/report.json` is uploaded, including partial failures,
with 30-day retention. The projection retains allowlisted IDs, exact SHA/ref
identities, enumerated statuses/rules, field types, safe actor identities and
timestamps. It excludes raw response bodies, logs, messages, URLs, headers,
private keys and tokens. Known credential strings are additionally redacted.
Unexpected exception text is replaced by a fixed message. No logs/artifacts from
the original run are downloaded or executed. Failure before script execution can
leave no report; cancellation can prevent upload or token revocation.

Report schema **2** separates target `suites`, `unrelated_suites`, and partially
validated `pending_suites`. Each validated entry includes classification and a
failed-evaluation count. Structured evaluation fields retain rule type,
enforcement, result and source type/ID for **every** evaluation, including failed
protections such as pull requests, signatures and status checks. Overall
`result` and optional `evaluation_result` are both retained. Protected-branch
sources may omit IDs; deleted rulesets may have null IDs. Unknown values become
null in the sanitized partial projection and explicitly fail validation;
unvalidated entries cannot yield `COLLECTED`. No free-text failure details,
source names or metadata are copied.

Schema and example reference:
[GitHub rule-suite API](https://docs.github.com/en/rest/repos/rule-suites#get-a-repository-rule-suite).
The documented bypass example contains failing protected-branch and ruleset
evaluations; these are evidence to retain, not reasons to reject collection.

## Local validation and review

From `/workspace/Felucca`:

```sh
python3 -B tools/hosting/rule_suite_diagnostic_test.py
python3 -B tools/hosting/authorization_canary_test.py
git diff --check
```

H12 tests reproduce the three review findings and cover valid unique evidence,
Audit-only GET authorization on all endpoint classes, missing credentials,
wrong App/actor/repository/ref/SHAs, list/detail mismatches, zero/duplicate/distinct
multiple matches, incomplete history/run metadata, creation evaluation checks,
overall failure, bypass with failed creation evaluations, unrelated history and
failed protections, recognized source variants and unknown/malformed evaluations,
redaction, environment isolation and ref drift. Python AST, parsed workflow YAML,
permission/action pins, shell syntax and configuration gates are also checked
locally. A firmware build is unnecessary.

Local security acceptance: manual/main-only, protected environment, Audit-only
reads, GET-only collector, no repository mutation/deployment, fixed targets,
exact identity enforcement, fail-closed uniqueness, sanitized artifacts and
unchanged H9 are all checked offline. Local validation: **33 H12 tests and 36 H9
tests passed**. Live grant effectiveness, writer identity lookup and actual suite
availability remain unverified; owner-confirmed permission provisioning does not
establish successful collection. No workflow is dispatched by this implementation
or its tests. PR publication is separate from execution; no merge, canary
operation, App/secret/ruleset change or production mutation is authorized by it.
