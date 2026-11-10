# H13 — local integration preparation

Original H9 run **42700311095**, H12 diagnostic **42778248967**, suite
**5005936145**, repository **Auberg1ne/Felucca**. Independent artifact inspection,
reported by the owner, verified exactly six evaluations (five recognized labels,
one unknown string label), list/detail identity agreement and these results:

- Overall **bypass**, actor **coffee-grains-state-writer[bot]**.
- Before SHA: 40 zeroes; after SHA:
  **1d1b9c5a63e7b0c65d16041dc657d8e4a36fa6c4**.
- Sole failure: **active creation**, source type **ruleset**, source ID **26842285**.
- Unknown-label evaluation: **active**, **pass**, source ID **null**. Its label
  and source type are redacted; no substitute value is claimed as original data.
- All other evaluations pass. H12 was **INCONCLUSIVE** and its H9 verdict was
  **NOT_REEVALUATED**.

This records independent inspection supplied by the owner, not a fresh download.
The previous artifact API attempt was denied by cloud network policy. No H9/H12
run has been dispatched, rerun or resumed by this task.

## Interpretation and separate H12 handling

The official [rule-suite schema and example](https://github.com/github/rest-api-description/blob/main/descriptions/api.github.com/api.github.com.json)
define `rule_type` and `rule_source.type` as strings, not closed enums. Overall
`result` is pass/fail/bypass; individual results are pass/fail. Overall bypass can
coexist with failed evaluations. `rule_source.id` is a source ruleset ID rather
than an individual rule-instance ID.

H12 remains a diagnostic, separate from H9's authorization predicate. Schema 3
retains valid unknown rule labels as `rule_type: null`, status UNRECOGNIZED,
SHA-256 fingerprint and length, without raw labels. It preserves outcome,
enforcement and source metadata, including null IDs. Unknown-label counts make
incomplete interpretation explicit. Malformed labels and unknown outcomes still
stop collection; H12's source-type allowlist remains unchanged. Its ability to
collect evidence never grants permission, and `h9_verdict` stays NOT_REEVALUATED.
H9 does not import H12 or consume sanitized reports as raw authorization evidence.

## Integrated, creation-only H9 exception

`authorization_canary.py` now unconditionally calls `_validated_app_create_evidence`
for every positive authorization result. There is no legacy no-failure fallback. It
requires explicit operation **app-create**, before zero, the exact writer actor,
repository ID **1445456236** and name **Felucca**, exact canary ref, pinned after
SHA, suite ID **5005936145**, and active creation source **26842285**.
It does not generalize the exception to other creation witnesses or runs.

The predicate requires list/detail agreement on identities, result, repository
metadata, actor ID when supplied, timestamp and evaluation summary. When the list
includes evaluations, they must also match the detail. Exactly one target suite
is required; duplicate or distinct matching suites remain inconclusive.

The suite timestamp must be valid UTC, identical in list/detail and within the
actual operation's start/observed-head window. The lower boundary is truncated to
seconds to match GitHub's timestamp precision. Missing, malformed, stale, reversed
or non-UTC windows reject. `probe` now supplies explicit operation name and both
boundary timestamps; no recorded timestamp is invented for the historical suite.

Exactly six structurally valid evaluations are required for this pinned event.
A completely passing, fully validated suite remains acceptable under the original
pass/bypass policy. If any evaluation fails, overall result must be **bypass**
and exactly **one** may fail: active `creation`, source type exactly `ruleset`, source
ID exactly **26842285**, also present in verified active creation sources. Every
other evaluation must be **pass**, including any unknown label. Raw labels and
source types must be present, nonempty printable bounded strings; null source
IDs are permitted for passing evaluations and never substitute for the failed
creation rule's required ID. Arbitrary strings are validated as schema values,
not assigned new authorization semantics or copied into evidence reports.

Overall fail, multiple creation failures, any other failed rule (including
opaque/evaluate/deleted rules), malformed/missing required fields, wrong actors,
wrong source IDs/types, mismatched SHAs/refs/repos, ambiguous suites and missing
or extra evaluations reject. Update, force, delete and ordinary-token operations
cannot use this exception. A redaction marker does not satisfy a raw required
string or timestamp.

The original negative-probe denial predicate is unchanged: overall fail, specific
expected active failures, receive-side GH013 rejection and unchanged head are
still required. No overall bypass can count as a successful denial. The existing
App-write success/head checks, production/canary absence guards, six-operation
order, no mutation retries, fixed Git destinations, credential routing,
main/environment gates and workflow permissions remain unchanged. The existing
canary continues to block a live rerun. Positive update authorization is disabled
while continuation remains unauthorized. The negative-probe operation definitions
and denial code for later review remain intact, but are unreachable in this stage.

## Mandatory operation-2 hold

The enclosing probe now has an explicit execution boundary. It requires a fresh,
empty operation report and still requires the canary and production refs to be
absent before any mutation. It refuses to resume a held/nonempty report. Each
iteration requires its operation number to be at most 2, before attempting a push.

After operation 2 succeeds, its evidence passes mandatory validation, and the
final canary/production ref checks succeed, the report records
`HOLD_AFTER_OPERATION_2`, `continuation_authorized: false`, original run/attempt,
canary witness head and next operation number 3. It saves and returns without
entering operation 3. The CLI exits **2**, intentionally preventing a successful
workflow step from implying completion or enabling ordinary downstream steps;
the existing artifact upload uses `always()`. There is no full H9 PASS fallback.
Any validation, ref-read or save failure stops without retry, repair or further
mutation. An original write may already have occurred before its evidence check;
failure does not authorize undoing or retrying it.

No continuation mechanism, input, recovery or cleanup command is implemented.
Any future mechanism requires separate explicit authorization, must authenticate
the original run identity and operation-2 evidence, and must read and verify the
existing canary head against that original witness before attempting operation 3.
It must never restart/recreate the canary through the old `probe` entry point.

`h13_bypass_proposal.py` is now only an offline adapter to the integrated predicate;
its fixture timestamp window is explicitly not a live operation window. It has
no HTTP/Git adapter or CLI and is not imported by a workflow.

## Sanitized fixture and test limits

`fixtures/h13-suite-5005936145.sanitized.json` records the verified six-evaluation
shape. The unknown label/source type remain null with redaction markers. The
four undisclosed passing labels/source fields and unprovided timestamp are
marked unavailable too. It contains no invented original values. The outcome-only
unit predicate recognizes the verified failure pattern, while the full raw
predicate correctly rejects this redacted fixture as insufficient API evidence.

Separate, explicitly synthetic complete API payloads exercise integration and
schema behavior. They are not reconstructions of redacted artifact fields. The
H13 schema tests likewise use arbitrary generated inputs, kept separate from the
sanitized owner-verified fixture. H9 tests verify forwarding of actual operation
context, all original denials, overall-fail rejection, every unrelated failure,
multiple creation failures, source/actor/identity mismatch, ambiguity, timestamp
freshness and complete raw fields. H12/H13 tests preserve opaque handling and
collection/authorization separation.

Regression tests exercise the full `main -> probe -> suite_evidence -> mandatory
validator` call path with fake IO. They reject missing/empty/incomplete evaluations,
wrong repository, missing timestamp, wrong actor and incomplete bypass evidence.
Both valid complete passing and exact-bypass cases end at HOLD with exactly two
fake writes. Later writes remain impossible even if their fake outcomes would
allow them. Failures never reach HOLD, retry, repair or enter operation 3. Existing
negative predicates and the requirement for specific GH013 rejection remain tested.

Remaining uncertainty before live use: raw redacted strings and the original
operation timestamp window have not been supplied for full fixture replay.
The exception stays fail-closed for missing values; passing local tests do not
retroactively mark the original H9 run PASS. Generalizing the pinned exception,
changing preflight or authorizing operations 3–6 requires separate review.

```sh
python3 -B tools/hosting/authorization_canary_test.py
python3 -B tools/hosting/rule_suite_diagnostic_test.py
python3 -B tools/hosting/h13_evidence_test.py
git diff --check
```

Local validation: **59 H9, 33 H12 and 16 H13 tests**. Python AST, parsed YAML,
shell syntax, action pins, credential isolation, unchanged denial logic and
whitespace checks also pass. No commit, push, PR operation, merge, workflow
execution, protected-state mutation or deployment is part of this work.
