# H8 — GitHub-only protected state writer

H8 supersedes the H6/H7 broker recommendation. The owner explicitly accepts
repository-level trust: environment secrets are not workflow-specific. No external
broker is required or implemented. This is local implementation, not live proof.

## Credentials and permissions

App **5678740**, Coffee Grains State Writer, installed only on
`Auberg1ne/Felucca`, needs Contents-write and mandatory Metadata-read. The pinned
`actions/create-github-app-token` v2.1.4 requests only that repository and
`permission-contents: write`. Do not broaden the installation or permissions.
The action masks the short-lived token and normally revokes it at job end;
cancellation may prevent revocation, so expiry (normally one hour) remains the
backstop. Rotate the private key after suspected disclosure.

In Settings → Environments → **github-pages**, add environment secret
`COFFEE_GRAINS_STATE_APP_PRIVATE_KEY` containing the App PEM. Do not add it to
repository/organization secrets. No key has been created or uploaded by this task.
Configure required reviewer(s), prevent self-review where appropriate, restrict
branches to **main**, and disable administrator bypass where supported. Protect
main and review controller/action-pin changes. Review every existing workflow able
to request this environment; approvals trust all code in the approved job.

The workflow defaults to Contents-read. `publish` adds Pages-write/ID-token-write;
`reconcile` adds Actions-read/Pages-read. Neither has Contents-write. The read-only
`GH_TOKEN` performs reads/evidence queries. Only begin/finalize/reconcile receive
`STATE_WRITER_TOKEN`; only Git push receives its authorization header. No token
is placed in Git config files, artifacts, caches, site assets, or job outputs.
Local presence checks are not authentication; server rulesets authorize the App.

Source compilation/tests run in a separate unprivileged job. Credentialed jobs
check out trusted controller code only, with persisted Git credentials disabled.
Release artifacts are validated as data and never executed in those jobs.
Pinned third-party actions and trusted controller code remain credential-bearing
trusted dependencies; review them accordingly.

## Approvals and rulesets (verify before dispatch)

- build-dev / rollback-dev: `coffee-grains-dev` approval, then `github-pages`.
- promote-stable / rollback-stable: `coffee-grains-stable`, then `github-pages`.
- reconcile: `coffee-grains-stable`, then `github-pages`; exact transaction,
  explicit outcome and both expected channels remain mandatory.
- All three environments: required reviewers and main-only deployment branch.
  The private key is needed only in github-pages. Do not rely on secret naming
  to isolate one workflow from another approved workflow.

Keep two Active rulesets targeting `refs/heads/coffee-grains-pages-state`:
1. restrict creation/update; sole necessary bypass is Integration App5678740,
   Always allow. Ordinary GitHub Actions App must not bypass.
2. deny deletion and non-fast-forward updates; **no bypass**, including our App.
Review other rulesets and bypass actors for conflicts. App ID is not installation
ID or bot user ID. Configuration is owner-reported, not independently verified here.
The App Contents-write token is repository scoped, not branch scoped: other branch
protections must constrain unintended writes; trusted code uses a fixed destination.

## Nonproduction authorization canary (NOT executed)

Use only `refs/heads/coffee-grains-state-auth-canary`. Before running, explicitly
cover this exact ref with both rulesets above, or identical isolated canary rules.
Do not run against production state; do not change the production destination in
`deployment_state.py`. Review existing canary head before any operation. Stop on
unexpected history. Never accept a caller-supplied destination ref.

A separately approved test workflow must run on trusted main, behind github-pages
approval, with no source/artifact execution. The negative control needs an ordinary
GITHUB_TOKEN explicitly granted Contents-write **only in this isolated test**;
a Contents-read token denial is not proof of ruleset enforcement. This temporary
test is not part of the deployment workflow and requires separate review before
publication/dispatch. Use the same pinned App token action, repository/permission
scope and protected environment secret as production. Never print either token.

Use a disposable local Git directory with no hooks, credential persistence or
host socket. Read exact remote canary head. Build commits A (root), B (parent A),
and C (independent root), with only a text witness. For each push supply credentials
through process-local Git authorization, never URLs or command-line tokens:

| Step | Credential | Canary operation | Required result |
|---|---|---|---|
| 1 | ordinary write-capable Actions token | create A | denied by creation rule |
| 2 | App | create A | allowed; read back exact A |
| 3 | ordinary write-capable Actions token | A → B | denied by update rule; still A |
| 4 | App | A → B normal push | allowed; read back exact B |
| 5 | App | B → C non-fast-forward push with force | denied; still B |
| 6 | App | delete ref | denied; still B |

Record ruleset-specific rejection (e.g. GH013), HTTP/Git result and exact head
before/after. Network errors, missing permission, dry-run or local Git rejection
are NOT successful authorization evidence. Do not retry failures. Leave the canary
witness branch intact: cleanup needs separate authorization, not weakened rules.
This procedure can create/update only the canary, never production state. It is
prepared documentation, not a claim that an executable test workflow is approved.

## First production bootstrap and recovery

After merge approval, independently verify settings and run the approved canary.
Verify no existing Pages owner/site conflicts, production state absence and expected
public absence. Only then explicitly approve build-dev using the allowlisted full
source SHA. The App creates the first state commit containing the durable pending
guard BEFORE Pages publication; no manual empty state branch is needed. An absent
state ref is not permission to overwrite an existing unknown public site.

Missing key/token, rejected push, expired token, cancellation or uncertain
acknowledgment never authorizes publication or clears a pending guard. Failed
finalization requires explicit approved reconciliation with live-site and original
Actions/Pages settlement evidence. Do not delete the guard or bootstrap over it.
No automatic fallback, force push, retry, or previous-STABLE selection is added.

## Gates

Local tests cannot establish live ruleset enforcement. Controller merge may proceed
only after code review; merging the dispatch-only workflow itself does not deploy.
First DEV dispatch remains BLOCKED pending environment secret provisioning,
settings verification, canary evidence, Pages ownership/bootstrap checks and explicit
approval. No repository settings or remote state were changed here.
