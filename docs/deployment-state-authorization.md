> Historical H6/H7 analysis. H8 explicitly accepts repository-level trust and
> supersedes the external-broker gate. See [H8](deployment-state-h8.md).

# H6 — Dedicated deployment-state authorization proposal

**H7 update:** the owner has confirmed App5678740 and the two state rulesets.
The remaining blocker is workflow/credential isolation, not App existence. The
[H7 design](deployment-state-workflow-isolation.md) supersedes this document's
environment-private-key suggestion when workflow-specific isolation is required.
Do not provision that secret as an alleged workflow-exclusive credential.

**Status: BLOCKED at the repository-authorization proof gate.**
No executable controller change, App registration, key/token creation, ruleset
creation, state write, deployment or remote PR update has been performed. PR9 at
`20d4bd366634f73d6e5256dfe037d16f0f28be89` still uses its original writer and must
not be treated as satisfying H6. This document specifies the proposed correction;
it is not evidence that the required GitHub settings exist or work.

## Documented platform guarantees versus unverified prerequisites

GitHub documents that:

- `GITHUB_TOKEN` is an installation token of the GitHub Actions App, not a unique
  identity belonging to this controller workflow.
- Branch rulesets can restrict creations and updates to actors in their bypass
  list. GitHub Apps are eligible bypass actors.
- All applicable rulesets and branch protections are aggregated. Bypassing one
  ruleset does not authorize bypassing a separate matching ruleset.
- Installation tokens support Git-over-HTTPS with Contents permission, expire
  after one hour, and can be limited to selected repositories/permissions.
- Branch rulesets are available for public repositories on Free plans, and private
  repositories on qualifying Pro/Team/Enterprise plans. Actual eligibility here
  has not been verified.
- The repository ruleset GET response exposes `bypass_actors` **only when the
  requester has write access to the ruleset**. A read-only response omitting this
  field is not evidence that the bypass list is empty or App-only.

The current environment excludes `api.github.com`. Git transport can read source
refs but cannot establish the active rulesets, repository plan, bypass actors,
environment-secret policy or actual token authorization. The prohibited App/
settings creation and state-write tests cannot be substituted with mocks. Do not
add Administration-write to the state-writer App merely to inspect bypass actors.
An authorized administrator's configuration inspection and real authorization
checks are required before claiming the separation is enforced.

## Proposed identity and credential boundary

Use a new, private, dedicated GitHub App, installed **only** for
`Auberg1ne/Felucca`. Repository permissions:

- Contents: read/write.
- Metadata: mandatory read.
- All other permissions: none; in particular no Administration-write, Workflows-
  write, Actions-write or Pages-write. No user/PAT authorization is needed.

The App's numeric App/Integration ID is the bypass identity. It is **not** the
installation ID, bot user ID, workflow actor, commit author name or job name.
Record App ID, installation ID and both ruleset IDs in a reviewed controller
configuration. None is a credential. Do not invent placeholder IDs in runnable
configuration or silently fall back to the shared Actions App.

Store the private key only as an **environment secret** in the protected
`github-pages` environment, e.g. `COFFEE_GRAINS_STATE_APP_PRIVATE_KEY`. Do not copy
it into repository/organization secrets, outputs between jobs, artifacts, Git,
logs or deployment-state data. Public App/installation IDs can be protected
environment variables or reviewed configuration values.

Retain the existing approval chain:

1. DEV operations require the `coffee-grains-dev` approval.
2. STABLE promotion/rollback and reconciliation require `coffee-grains-stable`.
3. The publication/reconciliation job then requires `github-pages` approval before
   it can read the key or mint the state-writer token.

All three environments must have required reviewers. The secret-bearing environment
must allow only branch `main`, not a similarly named tag or arbitrary branch.
Protect main and workflow/config changes with required review; prevent bypass of
those controls where supported. Another job named `publish` is not authorization.
A different approved main workflow could obtain an environment secret if humans
approve that environment use: therefore environment approval and trusted-main
workflow governance remain part of the security boundary.

Mint a short-lived installation token inside that approved job, scoped explicitly
to owner `Auberg1ne`, repository `Felucca` and Contents-write. A verified official
implementation option is `actions/create-github-app-token` v2.1.4 at
`67018539274d69449ef7c02e8e71183d1719ab42`. Keep default end-of-job revocation;
cancellation may prevent cleanup, so one-hour expiry, limited repository access
and key-rotation procedures remain important.

The eventual code change must:

- Set ordinary job `GITHUB_TOKEN` Contents permission to read, including publish
  and reconcile. Keep Pages-write/OIDC only for publication and Actions-read/
  Pages-read for recovery as required by the existing controller.
- Use the ordinary token only for read operations. Supply the independent App
  token only to state-branch create/update operations. Missing App credentials
  must refuse mutation; there must be no token fallback.
- Remove `GITHUB_ACTIONS`/`GITHUB_JOB` as alleged authentication. They may remain
  audit/context data, but GitHub's authenticated App principal and enforced
  branch rules must authorize the actual Git push.
- Preserve normal non-force pushes, expected-head comparison, guard readback,
  candidate hashes, concurrency, source allowlisting, explicit channel approvals
  and fail-closed H1 recovery. Do not alter state format or publication order.
- Avoid exposing the private key/token to the simulator source build or tests.

Possession of an environment-variable token string, an App slug, a local mock or
an API response lacking bypass information is not proof of server authorization.

## Required repository rulesets

Configure **two separate active repository branch rulesets** before production
state-branch creation. Both must target exactly:

- `refs/heads/coffee-grains-pages-state`
- A deliberately approved canary, e.g.
  `refs/heads/coffee-grains-state-auth-canary`, if used for authorization testing.

Use no exclusion that removes either target. Inspect organization/enterprise
rules and classic protections as well: additional restrictions can block the App.
They cannot be assumed absent. Do not alter main's protections to make state
updates work.

### A — App-only state creation and update

- Target: branch; enforcement: Active, never Evaluate/Disabled.
- Rules: **Restrict creations** and **Restrict updates**.
- Bypass list: exactly the dedicated App, mode **Always** (direct Git writes).
- No GitHub Actions shared App, admin/repository role, team, user, deploy key or
  other integration in this bypass list.

Illustrative REST shape, **not executable configuration**. Replace the string
placeholder with the verified numeric App ID only after separate approval:

```json
{
  "name": "COFFEE GRAINS state writer identity",
  "target": "branch",
  "enforcement": "active",
  "bypass_actors": [
    {"actor_id": "REPLACE_WITH_NUMERIC_APP_ID", "actor_type": "Integration", "bypass_mode": "always"}
  ],
  "conditions": {"ref_name": {"include": ["refs/heads/coffee-grains-pages-state", "refs/heads/coffee-grains-state-auth-canary"], "exclude": []}},
  "rules": [{"type": "creation"}, {"type": "update"}]
}
```

### B — Immutable history and branch retention

- Same target refs; enforcement Active.
- Rules: **Restrict deletions** and **Block force pushes**.
- Bypass list: empty, **including no exception for the state App**.

```json
{
  "name": "COFFEE GRAINS state history retention",
  "target": "branch",
  "enforcement": "active",
  "bypass_actors": [],
  "conditions": {"ref_name": {"include": ["refs/heads/coffee-grains-pages-state", "refs/heads/coffee-grains-state-auth-canary"], "exclude": []}},
  "rules": [{"type": "deletion"}, {"type": "non_fast_forward"}]
}
```

Do not combine A and B with the App bypassing both: an Always bypass would then
also exempt the App from force-push/deletion protection. No administrator can be
technically prevented from editing the ruleset itself; trusted settings owners
and audited changes are outside the ordinary-workflow-token threat boundary.

Inspect other matching rules for required PRs, status checks, signatures and
creation restrictions. The current transaction writer creates unsigned, direct,
single-parent commits; an incompatible rule must fail closed, not be bypassed by
a broad PAT or weakened main protections. Any exception must be separately reviewed
and limited to the dedicated identity/state refs.

## Explicit first-branch bootstrap and authorization tests

No production branch is created in this task. A future authorized setup must:

1. Verify repository visibility/plan supports Active branch rulesets and protected
   environment reviewers. If not, stop; do not substitute advisory checks.
2. Separately approve/register/install the App and provision the environment-only
   secret. No key values should be supplied in chat or committed.
3. Configure and inspect A/B before the production state branch exists. Use an
   authorized administrator's UI/API to verify the full bypass lists, exact refs,
   Active enforcement, environment restrictions and all overlapping rules. A
   lower-privilege API response missing bypass actors is insufficient.
4. Run an explicitly authorized, non-publishing canary authorization test under
   the same two rulesets. Test **actual** Git server decisions:
   - An ordinary Actions installation token with confirmed Contents-write is
     denied canary creation.
   - The dedicated App token, obtained only after environment approval, creates
     the canary with an isolated test commit.
   - The ordinary token is denied an update to that existing canary.
   - The dedicated App succeeds at a normal fast-forward update.
   - The App is denied non-fast-forward/force updates and deletion by B.
   - Wrong/expired credentials and missing approvals cannot obtain a write.
5. Distinguish rule-based denial from an invalid/read-only token, network block or
   unrelated rule failure. Verify token capabilities with an approved positive
   control and inspect the actual rejection/audit evidence. `git push --dry-run`
   does not prove server receive-rule enforcement; it is not a passing test.
   No production-state ref may be used as a disposable negative-test target.
6. Retain the canary as an audit witness, or obtain separate administrative cleanup
   approval. The App must not be given deletion bypass merely to clean it up.
7. After the controller correction and these checks are reviewed, allow the first
   approved DEV transaction to create the production branch using the App. Its
   **first commit is the existing H1 pending guard**, with an absent expected ref
   and verified guard readback before publication. Do not initialize it by copying
   main, force-pushing, bypassing creation restrictions or using `GITHUB_TOKEN`.
8. Subsequent guard/finalization commits use the same App and ordinary non-force
   compare-and-swap updates. Re-run H1 failure, cancellation and recovery tests.
   A rejected/uncertain guard write must never authorize publication. A failure
   after guard creation must retain the unresolved guard, not change credentials.

Creation/update/cancellation testing against the real server requires separate
approval: it changes GitHub refs, so it cannot be performed under the current
stop conditions. Unit tests with fake actors can validate routing/failure logic,
but cannot certify the repository's authorization model.

## Minimum next authorization decision

Before executable implementation is called an H6 fix, obtain an authorized
configuration readout and a reviewed setup/negative-test plan. API access here is
still blocked and no independent writer has been verified in this environment. Do not assume hidden bypass
entries, the account plan or first-branch behavior, and do not enlarge the App to
Administration-write simply to make self-inspection possible.

Once that gate is satisfied, implement credential routing and fail-closed profile
validation, add focused missing/wrong-token and failed-write tests, rerun H1's
23 recovery tests plus assembly/source-policy suites, and repeat a read-only
security gate. PR9 and the first DEV dispatch remain blocked meanwhile.

## Official evidence consulted

- [Ruleset creation/update restrictions](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets)
- [GitHub App bypass actors](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/creating-rulesets-for-a-repository)
- [Ruleset layering](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/about-rulesets)
- [GITHUB_TOKEN identity](https://docs.github.com/en/actions/concepts/security/github_token)
- [Installation-token authentication](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/authenticating-as-a-github-app-installation)
- [Repository rules API](https://docs.github.com/en/rest/repos/rules#get-a-repository-ruleset): bypass actors require ruleset-write access to be returned.
- [Pinned App-token action](https://github.com/actions/create-github-app-token/tree/67018539274d69449ef7c02e8e71183d1719ab42)
