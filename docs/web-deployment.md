# COFFEE GRAINS — Granular Synthesizer web deployment

## Status and addresses

This is a prepared deployment, **not a live-site claim**. No Pages settings have
been changed and no deployment has been executed by this task.

Expected persistent entry URLs (case-sensitive):

- STABLE: https://auberg1ne.github.io/Felucca/stable/
- DEV: https://auberg1ne.github.io/Felucca/dev/
- Channel selection: https://auberg1ne.github.io/Felucca/

GitHub Pages project hosting supports this static, import-free standalone WASM,
AudioWorklet, Dedicated Worker, same-origin MessageChannel and localStorage design.
No server API, SharedArrayBuffer or cross-origin isolation headers are required.
HTTPS and a deliberate user gesture are required for audio; Web MIDI permissions
and browser/device support vary. Test Chromium first, then other browsers separately.

## Safety boundary and release identity

Use a reviewed hosting change based on checkpoint
`29bc277a4c726b5b20b4881c6fa744e2d5b4cb9f`. The outstanding S9A/S9D/S9E compatibility
work is intentionally excluded. First commit/push/merge requires separate approval.
The workflow must be installed on the repository's default branch to appear in
Actions. Do not merge unfinished instrument work to accomplish that; apply only
this reviewed hosting patch on an approved checkpoint-based branch.

A build source is a complete immutable Git SHA, including its pinned Engines
submodule and committed WAV/font inputs. Never deploy a working directory or
untracked WASM. Emscripten 3.1.74 is pinned by official image digest. Pillow and
Playwright are pinned; OS/runners/browser bundles may evolve. This is a repeatable
source build with recorded output hashes, not a claim of bit-for-bit reproducibility
across every OS/font rasterizer revision. No proprietary FM-1 SDK is needed.
The packaged `manifest.json` hashes every runtime/license file; `provenance.json`
records source, assembly-tooling commit, compiler and factory input hashes. Factory audio is embedded in
WASM; no remote audio downloads are required at runtime or during generation.

Each build uses `releases/<source-sha>-<workflow-run-id>-<attempt>/` and is immutable.
Its page, JS, Worker, fonts, editor and WASM stay together. `/stable/` and `/dev/`
are tiny entry redirects, with corresponding `build.json` files. The footer and
editor display source SHA, UTC build time and release ID. Existing tabs keep their
old complete build. No service worker is installed. A cached channel redirect can
open an older complete build, but never mix new JS with old WASM; hard-refresh the
channel entry and check the footer. Pages does not permit arbitrary Cache-Control
headers, so immutable paths are the cache correctness mechanism (HTML meta tags
are advisory only). Do not edit files beneath an existing release ID.

STABLE and DEV share one origin but use separate `coffee-grains-stable-*` and
`coffee-grains-dev-*` simulator save/settings keys. This is a packaging-only
namespace adjustment; firmware storage formats and geometry are unchanged.
Old `felucca-emu-*` keys are left untouched and are not automatically imported.
Promoting DEV does not copy DEV saves into STABLE. Persistence is browser/profile/
origin local, can be blocked/cleared, is not a backup, and is not guaranteed in
private browsing. Clear browser data only after safely exporting valuable content.

## One-time GitHub setup (manual, only after approval)

1. Review and explicitly authorize the hosting-only commit and push. Make the
   workflow available on the default branch without including unfinished work.
2. In Settings → Environments, pre-create **coffee-grains-dev** and
   **coffee-grains-stable** with required reviewers and prevent self-review where
   supported. Add **github-pages** with a reviewer gate too. Confirm required
   reviewers are supported by the account/repository plan; if not, do not claim
   environment approval enforcement or enable unattended publication. Restrict
   deployment branches to the trusted workflow branch. The approval job checks
   the environment API for configured required reviewers and refuses publication
   if gates are missing or cannot be verified.
3. Settings → Pages → Source: **GitHub Actions**, enforce HTTPS. If Pages is
   already hosting another application in this repository, stop and review how
   to retain that site; this workflow owns the entire Pages root and must not
   silently replace an existing installer/site.
4. Permit the workflow's `contents:write`, `pages:write`, `id-token:write` only in
   the publish job. The build job has read-only repository permissions and no
   deployment credentials. No personal access token or secret is required.
5. Permit workflow writes to the dedicated `coffee-grains-pages-state` branch.
   Protect source branches normally; no source branch is pushed by deployment.
   Keep the state branch restricted to the workflow/maintainers. It retains
   previously approved, successfully deployed releases, channel pointers and
   their approval markers for rollback. It is not a source development branch.
6. Keep the workflow dispatch-only. Do not add a blanket push trigger.

## Approve DEV, then explicitly promote STABLE

In Actions → **COFFEE GRAINS controlled Pages deployment** → Run workflow:

- Choose the default (trusted) workflow branch; other refs are rejected.
- `operation=build-dev`, `revision=<full approved 40-character source SHA>`,
  `release` empty. Review the exact selected SHA before approving the DEV
  environment. This builds, runs required tests and packages a unique release.
- Review/approve the Pages deployment gate. A failed gate or test does not deploy.
- After success, check `/dev/build.json`, open DEV, check version footer and test
  audio, keyboard, engine selection, save/reload and editor read-only behavior.
  Check both STABLE and DEV over **public HTTPS** before claiming them verified.
- To promote, run `operation=promote-stable`, `release=<tested release ID>`,
  `revision` empty. Approve the STABLE and Pages gates. Promotion reuses identical
  bytes from the retained approved DEV release; it does not rebuild.

The first successful DEV deployment can precede any STABLE release. The STABLE
link will be unavailable until explicitly promoted; do not call it live earlier.
The two channels are independent release pointers in one atomic Pages site.
DEV updates never rebuild or modify STABLE's selected files. All workflow runs
are serialized and cannot cancel an in-progress publication. GitHub may replace
older pending concurrency runs: request one deployment at a time, not a queue of
promotions. No concurrent external/manual Pages deployments are supported.

## Rollback and failure handling

Run `rollback-dev` or `rollback-stable` with a retained previously approved
`release` ID and empty `revision`. The same environment approvals apply. Inspect
`coffee-grains-pages-state/releases/*/manifest.json` and commit history to choose
a revision. No rebuild, force-push, source reset or firmware migration occurs.
Application saves are not rolled back: incompatible newer saves may be rejected
by older code. Back up before experiments; rollback is code rollback, not a data
conversion tool. FUNB cannot read FUND; do not feed unfinished newer data to this
stable checkpoint. No automatic fallback or format/reclaim operation is added.

Every ordinary operation checks for an unresolved durable guard before building,
and checks it again before publication. An absent/unknown Git ref, HTTP error or
unreadable saved state is not silently treated as blank. Only the explicit absent
ref result permits bootstrap, and bootstrap also requires the public site to be
absent (HTTP404, not a timeout or authorization failure).

Before Pages can be called, `deployment_state.py begin` commits a pending guard
and the checksummed candidate delta to `coffee-grains-pages-state`, using an
expected-head comparison and a normal non-force push. It reads the exact guard
back before acknowledging permission to publish. The previous retained site stays
intact. Transaction identity is workflow-run-ID plus attempt; receipt, previous
head/digest, proposed channel selections and candidate hashes bind the operation.
The unchanged channel's files and every old release must remain byte-identical.
Candidate control data under `.deployment/` is never uploaded to Pages.

Pages publication is followed by strict public HTTPS checks of the unique deployment
marker, both channel pointers, selected manifests and runtime assets. Only then may
one compare-and-swap state update retain the matching site and remove the guard.
There is no unconditional cleanup. If artifact upload, Pages publication, cancellation,
public verification or state retention fails after guard creation, every later
ordinary DEV/STABLE/rollback/retry is blocked until explicit reconciliation. A
lost acknowledgement of an actually completed finalization can leave an already
reconciled state; the next operation must still verify that state against the
public site. It never chooses an older snapshot automatically.

A fresh transaction does not fix an unresolved old one. Do not delete the guard,
force-push the state branch, deploy from a separate workflow, or move the state
write before publication as an alleged fix. Concurrency serialization is an extra
safeguard, not the recovery mechanism. Source and state branch protection must
prevent other publishers, forced updates and state-branch deletion.

### Explicit reconciliation (no publication)

`operation=reconcile` requires **both the STABLE approval environment and the
Pages approval environment**, even if the interrupted operation changed DEV. The
job has contents-write, actions-read and pages-read, but no pages-write or OIDC
permission. It cannot publish, republish, roll back or choose another release.
Specify the exact pending transaction, `outcome=adopt-candidate` or
`outcome=retain-previous`, and the exact expected live STABLE and DEV release IDs
(use `none` only for an actually absent channel).

The helper verifies that the original workflow attempt and publisher have ended.
If the original Pages step is explicitly `skipped`, only retaining the previous
snapshot is permitted. Otherwise authenticated logs must identify exactly one
Pages deployment belonging to that workflow SHA, and the official Pages status
API must report a conclusively terminal status. A known `succeed` status permits
only adopting the candidate; stale CDN observations cannot justify reverting its
retained state. Terminal failure permits an explicitly chosen snapshot only if
its actual public bytes match. Pending, unknown, `deployment_lost`, unavailable
logs/API data and ambiguous requests leave the guard intact. If settlement cannot
be proved, stop for a separately reviewed administrative recovery; do not loosen
checks or guess from a page loading.

Recovery then verifies the selected public snapshot, preserves the other channel,
and atomically replaces state/removes the matching guard. Wrong transaction IDs,
stale heads, wrong channel expectations, altered candidate data and repeated
completed recoveries refuse mutation. A failed recovery can repeat the same
explicit request after fixing the external issue; it cannot silently switch to a
previous release. Recovery evidence is read-only; no GitHub settings are assumed
to be configured and the existing reviewer checks still apply.

```sh
# Inspect the pending record read-only on the state branch before choosing values.
# Execute only after review/approval; this command does not publish anything.
gh workflow run coffee-grains-pages.yml --repo Auberg1ne/Felucca --ref DEFAULT_BRANCH \
  -f operation=reconcile -f transaction=RUN-ATTEMPT -f outcome=adopt-candidate \
  -f expected_stable=EXACT_LIVE_STABLE_RELEASE -f expected_dev=EXACT_LIVE_DEV_RELEASE
```

The pinned Pages action is verified official v4.0.5 commit
`d6db90164ac5ed86f2b6aed7e0febac5b3c0c03e`; its status API/log handling is the
source for recovery settlement checks. The earlier prepared, nonexistent Pages
action reference has been corrected. Actual GitHub API/Actions execution remains
unverified until the separately approved first deployment.

No automatic deletion of history. The workflow refuses sites >=900 MiB (Pages
has service limits, including roughly1 GiB published site). Review a separate
retention/archive policy before reaching that bound. Action artifacts expire;
rollback depends on the retained state branch, not artifact lifetime.

## Local preparation and verification

Use an isolated checkpoint worktree. Populate `firmware/engines` at the gitlink
SHA (`git submodule update --init --recursive` in a fresh clone), install
`tools/hosting/requirements.txt` in a temporary Python environment, and use the
pinned Emscripten image. `sh tools/hosting/build.sh` generates real committed
assets and invokes the existing WASM script; no target SDK build is attempted.
Run `sh tools/hosting/check.sh` for required native/storage/codec/JS/WASM gates.
Set `S4A_DOM_MODULE` to jsdom26.1.0's `lib/api.js` for additional DOM checks.

Example packaging **for local inspection only** (do not deploy an uncommitted
candidate; the workflow requires a clean fetched commit):

```sh
python3 tools/hosting/site_test.py
python3 tools/hosting/deployment_state_test.py
python3 tools/hosting/site.py package --source . --build build/emu --out build/pages \
  --revision FULL_SOURCE_SHA --release FULL_SOURCE_SHA-1-1 \
  --timestamp 2026-11-09T00:00:00Z
python3 tools/hosting/browser_test.py --site build/pages --chromium /usr/bin/chromium
```

The packaged browser test serves an unchanged candidate copy at `/Felucca/`, binds
only127.0.0.1, uses sandboxed Chromium and real WASM, and checks default gesture
startup, one authority, branding/build paths, the real separate editor and channel
save isolation. The existing55-case startup regression checks cancellation,
corrupt saves, missing WASM and Worker opt-in without changing defaults. Neither
test establishes real-time deadlines or audible quality. Do not disable browser
sandboxing to force a green run; report unsupported CI sandbox environments.

## Existing functionality and known limits

- SAMPLE, GRAIN, SLICE and thirteen source IDs retain the checkpoint behavior.
  Five factory IDs currently use three distinct materials and aliases, not five
  distinct recordings. Generator tests must not silently fall back to assetless
  builds; required assets are committed and generated data is packaged in WASM.
- The editor's browser-to-WASM link remains S4A read-only. Sample uploads/erase,
  backup restore, engine changes and other mutations cannot be sent through it.
  Hardware Web MIDI and mock upload tests are separate from simulator capability.
  User-sample upload/replacement/recovery protections are tested natively; direct
  interactive browser sample upload is an unfinished writable-transport milestone.
- FUNB project save/load and legacy import are exercised via real native storage
  paths. Newer unfinished compatibility work is not part of this deployment.
- Ordinary Web MIDI/mock behavior and Worker default-off remain unchanged.
- FM-1 target linking, resource measurements, physical flashing/power-loss checks,
  real-time deadlines and listening evaluation remain outstanding.

Strict HTTPS observation is part of guard finalization. A failure leaves the
guard unresolved and blocks later deployments. It checks current channel/runtime
bytes, not audio behavior or the listening experience. GitHub administrators who
bypass the workflow or branch protections are outside this transaction protocol.

A successful local test is not evidence that either public URL exists. Record the
actual HTTPS check, deployed release ID, browser and date after first publication.

CLI equivalents (execute only after the separate approval gates):

```sh
# The workflow must already exist on DEFAULT_BRANCH.
gh workflow run coffee-grains-pages.yml --repo Auberg1ne/Felucca --ref DEFAULT_BRANCH \
  -f operation=build-dev -f revision=FULL_APPROVED_HOSTING_COMMIT
# After DEV succeeds and you have tested its public HTTPS build:
gh workflow run coffee-grains-pages.yml --repo Auberg1ne/Felucca --ref DEFAULT_BRANCH \
  -f operation=promote-stable -f release=SHA-RUN-ATTEMPT
# Explicit code rollback; does not roll back browser data:
gh workflow run coffee-grains-pages.yml --repo Auberg1ne/Felucca --ref DEFAULT_BRANCH \
  -f operation=rollback-stable -f release=PREVIOUS_APPROVED_RELEASE
```
