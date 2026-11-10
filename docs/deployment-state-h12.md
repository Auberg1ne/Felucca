# H12 — read-only rule-suite diagnostic

This separate manual workflow inspects original H9 run **42700311095** and the
existing `refs/heads/coffee-grains-state-auth-canary` witness at
`1d1b9c5a63e7b0c65d16041dc657d8e4a36fa6c4`. There are no inputs or ref overrides.
It never invokes H9 or resumes its operations. `COLLECTED` means exactly one
consistent creation suite was collected; it is not an H9 authorization PASS.
`h9_verdict` always remains `NOT_REEVALUATED`.

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

Every returned suite record must have the exact canary ref, **40 zeroes before**,
the pinned witness **after**, and the exact State Writer actor. These identity
fields must be present in both list and detail records, which must agree on ID,
ref, SHAs, actor, result, timestamp and any supplied repository metadata.
Repository provenance comes from the fixed endpoint and validated original run;
if a suite supplies repository identity, it must also agree. A nested actor login
never substitutes for a missing `actor_name`.

Exactly **one** suite must remain after complete bounded history inspection. It
must fall within the original run window, have a pass/bypass result, and include
nonempty successful evaluations with an active creation rule from the current
creation-rule sources. This is deliberately strict: unrelated or malformed
records are rejected rather than silently ignored. Empty history, nonempty
history without the exact creation suite, wrong identities, failed/missing
creation evaluations, duplicate IDs, inconsistent records, and multiple matching
suites with distinct IDs all produce **INCONCLUSIVE**, exit 1. No first-match
selection is used. H9's predicate remains unchanged.

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
redaction, environment isolation and ref drift. Python AST, parsed workflow YAML,
permission/action pins, shell syntax and configuration gates are also checked
locally. A firmware build is unnecessary.

Local security acceptance: manual/main-only, protected environment, Audit-only
reads, GET-only collector, no repository mutation/deployment, fixed targets,
exact identity enforcement, fail-closed uniqueness, sanitized artifacts and
unchanged H9 are all checked offline. Local validation: **24 H12 tests and 36 H9
tests passed**. Live grant effectiveness, writer identity lookup and actual suite
availability remain unverified; owner-confirmed permission provisioning does not
establish successful collection. No workflow is dispatched by this implementation
or its tests. PR publication is separate from execution; no merge, canary
operation, App/secret/ruleset change or production mutation is authorized by it.
