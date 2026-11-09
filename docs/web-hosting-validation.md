# COFFEE GRAINS hosting preparation — local validation

Prepared in `/workspace/coffee-grains-hosting`, a detached isolated worktree at
`29bc277a4c726b5b20b4881c6fa744e2d5b4cb9f`. The main compatibility worktree, its
uncommitted S9A/S9D/S9E work, its index and `firmware/bsp/` were left untouched.
The original playable checkpoint artifact was not replaced. No commit, push,
Pages setting change or deployment was performed. This document records local
verification, not public HTTPS availability.

## Build and scope

Public branding changes only in README, simulator HTML and editor HTML. Internal
firmware/product/protocol identifiers, credits/licenses, engine controls, DSP,
flash geometry, storage formats, startup defaults and S4A permissions unchanged.
The existing host build script is used. The Engines gitlink is pinned to
`c46e0086f69bdc26df1cd144dca9e683cc4531dd`. Local initialization used an offline
archive of that exact existing submodule commit; CI checks it out recursively.

Emscripten3.1.74, official digest-pinned image:
`emscripten/emsdk@sha256:af45409f3199d88db4b1b03af0098532c8fb33a375ac257463eeb0a622870d06`.
Local container was offline, non-root, capability-free, read-only-root, with an
isolated temporary source copy and output mount. Python Pillow13.0.0 generated
all seven headers from committed inputs; **every generated header was identical**
to the validated playtest, including factory samples.

Local branded WASM: `build/emu/felucca.wasm`,765889 bytes, SHA256
`4f43ba731c4b56d10af24f11ae5ccb670a5bf2fd61c88b475cbbab8e66d8be0d`.
It is an uncommitted local inspection build, NOT a publishable source revision.
The approved workflow rebuilds from a fetched clean commit and records both
source and assembly-tooling SHAs. It never uploads this local directory.

## Tests actually run

| Check | Result |
|---|---|
| Fresh asset generation + pinned offline WASM build | Passed |
| Packaging/manifest/immutable paths/promotion/rollback/isolation unit tests |12 passed |
| Packaged `/Felucca/` real-browser test |7 passed |
| Existing real-browser startup regression |55/55 passed |
| DOM/WASM startup |47 passed |
| Native/DOM/WASM read-only transport |62 passed |
| WASM simulator/audio render |22 passed |
| S3 browser compatibility/mocks |63 passed |
| JS backup compatibility |64 passed |
| Native sample upload/replacement/delete/quarantine/header protections |261 passed |
| Native storage/fault gate/RAM-only SLICE store |35 /6 /10 passed |
| Native project codec/A-B cuts/legacy import |11 groups passed |
| Native backup capture/LIST/restore |109 checks passed |
| Native eight-slot SAMPLE/GRAIN/SLICE compatibility |24 cases passed |
| Native firmware editor bridge |9 cases passed |
| Additional deployment factory-presence gate |Passed; no silent assetless fallback |
| Python/HTML/JS/shell syntax, workflow YAML +11 embedded shell blocks, diff checks |Passed |

Exact main test command:
`S4A_DOM_MODULE=/tmp/s4a-dom/node_modules/jsdom/lib/api.js sh tools/hosting/check.sh`.
The same script was rerun from `/tmp` with an explicit source directory to verify
its workflow-tooling/source-checkout path handling. Commands/results are retained
in `build/hosting-evidence/` (ignored build output, excluded from a commit).

The browser checks used system Chromium155.0.8059.79, Python Playwright1.62.0,
sandbox enabled, localhost-only serving and a temporary site copy. The packaged
check rejects external requests and records console/page errors and HTTP>=400.
No such errors were observed. It verified title/branding, channel redirect below
`/Felucca/`, no context before the gesture, exactly one authoritative context/node,
real read-only editor metadata responses and channel-local persistence namespaces.

The existing55-case browser report is
`/tmp/coffee-grains-browser-startup-4b932mv4/report.json`.
Native engine tests cover finite/nonzero SAMPLE/GRAIN audio, SLICE sources and
eight user slots. Native project/backup tests exercise real storage paths. Those
are NOT evidence of writable browser-to-WASM sample upload: that transport is
still intentionally unavailable. No mutation permission was added.

Initial local preparation detected an uninitialized isolated Engines submodule;
using its exact pinned existing commit resolved generation. A unit test exposed
inconsistent missing-manifest exception handling; it now rejects uniformly before
changing channel pointers. Neither issue remains in the final local checks.

## Unverified and remaining gates

- Actual GitHub Actions orchestration and environment protection/API access.
  YAML and embedded scripts were checked, not an authenticated workflow run.
- Existing Pages settings/site ownership: manually confirm before replacing the
  repository Pages root. The proposed site does not include the hardware installer.
- Public STABLE/DEV HTTPS URLs, CDN propagation and post-deployment checksum step.
- Other browsers, mobile output, Web MIDI devices, user listening and audio deadlines.
- Physical FM-1 linking/resource use/flashing/power-loss behavior.
- The unrelated full historical editor/user-preset/PIANO suites were not rerun;
  no claim that the entire repository test suite is green.

The first next action is review/approval of this isolated hosting patch. A later
explicitly authorized commit/push installs the dispatch-only workflow. Configure
required reviewers and Pages Actions, approve a full source SHA for DEV, inspect
its actual HTTPS instrument, and only then explicitly promote the same release
bytes to STABLE. See [deployment guide](web-deployment.md).

## Complete hosting-only file inventory (16 files after H1)

Modified:
- README.md
- web/emu/index.html
- web/editor.html

New:
- .github/workflows/coffee-grains-pages.yml
- docs/web-deployment.md
- docs/web-hosting-validation.md
- tools/hosting/build.sh
- tools/hosting/check.sh
- tools/hosting/factory_gate.c
- tools/hosting/requirements.txt
- tools/hosting/site.py
- tools/hosting/site_test.py
- tools/hosting/browser_test.py
- tools/hosting/verify_public.py
- tools/hosting/deployment_state.py
- tools/hosting/deployment_state_test.py

Generated WASM, generated headers, reports/logs in build/, Python bytecode,
SDK/toolchain binaries and the original compatibility worktree are excluded.
All hosting changes are unstaged/uncommitted in the isolated worktree.

## H1 deployment-state correction (supersedes the initial checkpoint review)

The initial12 passing assembly tests did not cover a successful STABLE publication
followed by a failed retained-state write. A later DEV assembly could use the old
STABLE pointer. This was reproduced locally before implementing the fix.

H1 adds `tools/hosting/deployment_state.py` and
`tools/hosting/deployment_state_test.py`, changes the workflow to persist/read back
a pending guard before publication, and requires public/state reconciliation before
clearing it. `verify_public.py` now strictly checks presence/absence, channel
redirects, deployment marker and active runtime assets. Missing/failed/ambiguous
evidence never authorizes fallback. Read the updated deployment guide for recovery.

The local transaction tests execute the real assembly, guard, finalization, recovery
and public-verifier code against separate durable simulated Pages/state stores.
They compare actual simulated published STABLE/DEV files with retained state and
exercise fault injection before/after persistence. No Git staging, commits, pushes,
GitHub API calls or real Pages publication occur in those tests. The production Git
CAS and authenticated GitHub recovery adapter have not been executed remotely.
The official pinned Pages action source was read to verify API terminal statuses
and source-run deployment ID logging. The prior invalid SHA is corrected to the
verified official v4.0.5 commit. No factory, DSP, storage, startup or editor asset
changes are part of H1, so the earlier browser/native playback results are historical
results, not rerun or reattributed to this safety-only patch.

With the two new H1 helper/test files the total hosting patch is now16 files;
H1 itself changes only the workflow, two deployment documents, HTTPS verifier
and those two new files. Generated evidence remains excluded under build/.

### H1 final local results

- `python3 -B tools/hosting/deployment_state_test.py`: **23 passed**.
- `python3 -B tools/hosting/site_test.py`: **12 passed**.
- Python AST, workflow YAML,14 embedded Bash blocks, permissions/concurrency
  contracts and `git diff --check`: **passed**. Staging index remains empty.
- Evidence: `build/hosting-evidence/h1-tests-final.log` and
  `build/hosting-evidence/h1-assembly-final.log` (ignored, not commit inputs).

The original failure test actually publishes the unsafe stale DEV snapshot into
its independent simulated Pages store, demonstrates STABLE changing from release2
back to release1, and compares retained state. The corrected path publishes STABLE2,
injects a failed state save, confirms retained STABLE1 plus a durable guard, blocks
all ordinary operations, and explicitly reconciles both stores to STABLE2. Other
cases cover failed publication, cancellations before/after publication, rollback
with failed save, durable guard reopening, guard/final acknowledgement loss,
stale receipts, CAS conflicts, mixed public bytes, unprovable API settlement,
wrong recovery selections, repeated recovery and normal DEV/STABLE after recovery.
No passing assertion relies solely on manifest validity.

Failure/cancellation windows remain intentionally fail-closed: before a guard is
acknowledged, no publication is allowed; after it is durable, interruption can
leave the site old or new while the guard blocks subsequent publication. Only
verified settlement/public bytes permit finalization. Loss of an acknowledgement
after the final atomic state update may leave both stores already reconciled;
future operations reverify them and do not roll back. Missing expired logs,
unknown Pages outcomes, source-run activity, API permission failures or unavailable
HTTPS keep the guard unresolved and may require separately reviewed administrative
recovery. Do not clear it merely to regain deployment availability. This protocol
assumes protected state/source branches and no out-of-band Pages publishers.

H1 is ready for a **hosting-only local checkpoint after review**, not for an
unverified production rollout. Actual GitHub branch CAS, deployment cancellation,
recovery API/log availability, environment protections and public HTTPS have not
been tested remotely. Nothing was staged, committed, pushed, published or configured.
