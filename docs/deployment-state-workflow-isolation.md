> Historical H6/H7 analysis. H8 explicitly accepts repository-level trust and
> supersedes the external-broker gate. See [H8](deployment-state-h8.md).

# H7 — Secure state-writer workflow isolation design

**Status: DESIGN ONLY / BLOCKED for executable integration and deployment.**
Controller baseline remains `20d4bd366634f73d6e5256dfe037d16f0f28be89` (PR9).
No workflow, writer code, App registration, credential, GitHub setting, canary ref
or production state has been changed. The baseline still uses the ordinary token
for writes; this document is not a completed credential replacement.

## Confirmed configuration supplied by the repository owner

Treat these as user-confirmed facts, not measurements made by this task:

- App: **Coffee Grains State Writer**, numeric App ID **5678740**.
- Installed only on `Auberg1ne/Felucca`; Contents-write granted.
- State Writer Authorization ruleset: Active, restricts creations/updates, App
  bypass mode Always allow.
- Separate Deployment State ruleset: deletion/force-push blocked, no bypass.
- `coffee-grains-dev`: required reviewer Auberg1ne, branch main only.
- GitHub Pages source: GitHub Actions.

These facts supersede H6's uncertainty about whether an App has been provisioned.
They do not supply the installation ID, numeric repository/owner IDs, full bypass
lists and target patterns, other applicable rules, stable/Pages environment
protections, secret location, or any independently enforced workflow restriction.
Those gaps must not be filled with invented IDs or assumed settings.

## Why an environment-secret substitution is insufficient

GitHub documents that environment secrets are available to jobs using that
protected environment after its protection rules pass. The binding is to the
repository/environment, not exclusively to one workflow path. Main-only branch
restrictions do not distinguish two workflows both running from main.

For example, another main workflow could declare the same environment and,
**if its environment use is approved**, reference the same App private-key secret
and mint a token. This is not an allegation that a fork can obtain secrets without
approval. It is the precise limit of the proposed repository/environment boundary.

Consequently:

- A separate privileged YAML file with the same environment key is organization
  and code-review separation, not independent secret isolation.
- `workflow_call` alone is not an exclusive caller allowlist.
- A reusable workflow checking `GITHUB_JOB`, a workflow display name, an input
  caller name, or an environment variable does not authenticate its invoker.
- A job allowed to request OIDC does not thereby become an authorized state
  writer: the receiving service must verify the signed token and restrictive
  claims, independently of the requesting workflow.
- A state App can bypass its authorization ruleset, so releasing its key to a
  different approved job grants that identity to that job. Branch rules identify
  the App, not the particular workflow which obtained the App's token.

A repository-wide trust model could explicitly trust all reviewed main workflows
that receive environment approval. That is a weaker, legitimate policy choice,
but it is **not** workflow-specific isolation and is not adopted by this task.

## Preferred independent boundary: OIDC-authorized state broker

Use a small, separately administered state-write broker. Do not put the App key
in this repository's workflow, repository, organization or environment secrets.
The broker stores it in its own secret store, accepts narrowly authorized state
operations, mints short-lived installation tokens for App5678740, and performs
Git state writes. It never returns the App key or installation token to an Actions
job. No broad PAT is needed. No broker or credentials are created in this task.

Keep GitHub Pages publication inside Felucca's existing trusted controller; this
is a broker for **state writes**, not a claim that another repository can publish
Felucca Pages using its own Pages OIDC identity.

A pinned reusable privileged workflow can be the broker client. It contains no
App credential, runs no simulator/build code and requests an OIDC token only in
its approved publication/reconciliation job. The unprivileged build remains
read-only and supplies immutable, checksummed data, never code for the broker to
execute. Splitting YAML alone is insufficient; the broker's independently
administered authorization policy supplies the actual restriction.

### Signed identity policy

The broker must verify GitHub's OIDC signature, issuer/discovery/JWKS, algorithm,
expiry and not-before time before trusting any claim. It must pin at least:

- Numeric repository and owner IDs for `Auberg1ne/Felucca`, plus expected names.
  Obtain the real IDs from authorized metadata; none has been invented here.
- `ref=refs/heads/main`, branch ref type, and `event_name=workflow_dispatch`.
  Reject fork, PR, tag and unexpected event contexts.
- The exact **caller** `workflow_ref` path and reviewed `workflow_sha`.
- The exact **called** `job_workflow_ref` and pinned `job_workflow_sha` for the
  privileged reusable workflow. These fields are not interchangeable with the
  caller claims. Verify their actual values in an authorized future test.
- The expected protected environment/subject. Reject absent or unexpected values;
  an environment name is not proof that its reviewer policy is configured.
- Run ID and attempt matching the requested transaction, and a single-use `jti`.
- A broker-specific audience binding the operation/request digest, or another
  equivalently verified, replay-resistant request binding.

**Checking only the reusable-workflow identity is insufficient:** another caller
could invoke that same workflow. Both caller and callee must be constrained.
Likewise a branch-name check alone permits all main workflows. Changes to trusted
workflow hashes require separate broker-policy review, not an input override.

No existing broker, verifier, policy binding, numeric IDs, replay store, proof of
environment restrictions or live OIDC-token evidence has been provided. This is
where executable integration stops. Merely decoding a JWT or passing mock claims
must not be reported as a demonstrated isolation boundary.

### State operation contract

Retain H1's current state machine and publication order. The broker must expose
only the fixed `coffee-grains-pages-state` target, no arbitrary repository/ref or
raw shell/force-push endpoint. Bind each operation to:

- Protocol version, authorized caller/run/attempt and requested operation.
- Approved simulator source SHA and selected channel/release.
- Exact expected prior head (or explicit absence during bootstrap).
- Bounded, path-safe candidate/delta data and recomputed checksums/digest.
- H1 guard identity, prior/candidate channel selections and completion receipt.

Requests and responses must be authenticated and replay-safe. The broker validates
state transitions; it does not execute artifacts, scripts, hooks or untrusted
checkout content. Source SHA allowlisting, independent DEV/STABLE approval and
trusted-main restrictions remain mandatory in the reviewed caller.

The first successful write must remain the **durable pending guard**, followed by
readback before any Pages publication. Finalization must still prove matching
published bytes and retain state before clearing the guard. Recovery must retain
its explicit channel selection and original-publication settlement checks. Lost
acknowledgements, service failure, CAS conflicts, cancellation or unknown outcomes
must block/reconcile, not trigger fallback credentials or a fresh publication.

A minimally privileged installation token requests only Felucca and Contents-write
(with mandatory Metadata-read), never all installations/repositories. The private
key and tokens must not enter job outputs, source checkouts, artifacts, browser
assets, logs or caches. Expiry/revocation and broker compromise/rotation procedures
remain necessary. Existing App-only creation/update and no-bypass history rules
must enforce the Git server writes too.

GitHub job tokens stay Contents-read. Only the Pages job keeps Pages-write and
OIDC-write; recovery retains needed Actions-read/Pages-read and OIDC-write for
broker authentication, not Contents-write. Exact permissions and token lifetime
must be checked against the selected implementation before enabling it.

## Isolated authorization canary plan — not executed

Production state must stay absent until a separately authorized H1 bootstrap.
Use an explicitly approved non-production canary ref covered by **both** actual
state rulesets; do not assume the current state-only patterns cover it. Any target
pattern change requires separate settings approval. Never use production state as
an experiment or assume a dry-run invokes GitHub's receive-rule enforcement.

After that authorization, decisive tests are:

1. Confirm the complete active target/bypass/history rules. Confirm the canary is
   protected identically; no shared Actions App/admin-role bypass in the identity
   rule, and no App bypass in the separate deletion/force rule.
2. Use an ordinary Actions token with demonstrated Contents-write capability.
   It must be denied both initial canary creation and updates to an existing
   canary. Distinguish a ruleset rejection from expired credentials, read-only
   permissions, network denial or some unrelated protection.
3. Invoke the broker from the authorized caller/callee/environment only after the
   correct approvals. The App must create the canary, then perform a non-force
   fast-forward update. Verify actual remote heads and authenticated actor evidence.
4. Prove App non-fast-forward/force-push and deletion attempts are denied. Retain
   the canary if cleanup is prohibited; do not weaken history rules for cleanup.
5. Try another same-repository main workflow after its own environment approval,
   an untrusted branch/fork/PR/tag, wrong caller or callee SHA, wrong audience,
   expired/invalid signature, absent claims, and replayed transaction/`jti`.
   They must not obtain an App token or cause any remote-head change. Merely failing
   to read a local secret is not proof of broker isolation.
6. Verify source/build jobs never receive App credentials. Check redacted logs,
   artifacts and browser output; do not print secret values to perform that check.
7. Inject broker/token-mint/network/readback/persistence failures on the canary.
   Re-run all H1 failed-publication, cancellation, rollback and reconciliation
   cases using the resulting remote heads, not only manifest validity.
8. Only after that evidence is reviewed may a real approved transaction create
   `coffee-grains-pages-state` via its App-authenticated pending-guard commit.
   No manual copy of main, ordinary-token bootstrap, deletion or force update.

Local parser, policy and fake-state tests can validate a future client's mechanics.
They cannot certify cryptographic verification, GitHub authorization, environment
approval or the isolation of an unprovisioned service. No such claims are made here.

## Remaining decisions and next gate

The user-confirmed App/rulesets are useful prerequisites, not proof of a selected
workflow-isolation boundary. Select and authorize the independent broker/trust
service, provide verified repository/installation IDs and policy configuration,
confirm stable/Pages approvals, and authorize the canary plan before executable
integration. If only repository-wide environment trust is acceptable, explicitly
approve that different threat model first; do not silently relabel it as isolated.

PR9's existing shared-token writer must not be dispatched as the H7 solution.
No production deployment readiness or pre-merge security approval is asserted.

## Official documentation used

- [Environment protection and secret access](https://docs.github.com/en/actions/how-tos/deploy/configure-and-manage-deployments/manage-environments)
- [Reusable workflow semantics](https://docs.github.com/en/actions/how-tos/reuse-automations/reuse-workflows)
- [OIDC claim reference](https://docs.github.com/en/actions/reference/security/oidc)
- [OIDC with reusable workflows, including caller/callee distinction](https://docs.github.com/en/actions/how-tos/secure-your-work/security-harden-deployments/oidc-with-reusable-workflows)

Those sources establish mechanisms, not the existence/configuration of a broker
or the enforcement of the specific repository settings in this environment.
