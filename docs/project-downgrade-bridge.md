# Project downgrade safety bridge B1

Base: `99325e7eef99e09b5565edf37ed9098ec5c2f5af`.
Branch: `safety/coffee-grains-project-downgrade-bridge`.

B1 establishes a project-storage ownership guard independently of WIN. No new
project magic, project/preset version, parameter, renderer, UI layout, MIDI
mapping or editor command layout is introduced. The original frozen I1 fixtures
and historical failing oracle are unchanged.

## Protection model

`storage.c` adds read-only `st_copy_state` and `st_erased_range` probes, using
existing main-loop `st_buf` scratch. `project.c` classifies physical ownership
separately from successful decoding and GRAIN-only engine compatibility:

| State | Meaning | Mutation policy |
|---|---|---|
| EMPTY | Both copies erased, or newest valid empty tombstone with no unowned peer | Writable |
| VALID | Recognized, validated supported project | Existing load/save behavior |
| INCOMPATIBLE | Recognized project whose engines the shipping policy refuses | Existing read-only compatibility policy |
| UNSUPPORTED | Storage-valid payload with unknown/unrecognized project format | Occupied, read-only |
| CORRUPT | Nonempty invalid envelope, failed checksums/truncation, malformed known project, unowned padding or post-import invalid patch bytes | Occupied, read-only |
| IO | Read failure or configured flash unavailable | Unverified/treated occupied, read-only |

Both physical A/B sectors are examined. Any unsupported, corrupt or unreadable
copy protects the pair, **even if an older/current other copy is readable**.
Known valid histories and valid empty tombstones remain writable. A logical
empty record cannot authorize erasing an opaque peer. Headerless non-erased
payload, unknown commit metadata, programmed gap/tail bytes and noncanonical
reserved storage-header fields are not treated as empty. Unrecognized storage
envelopes are conservatively classified CORRUPT/unowned; an unknown project
payload in a valid envelope is specifically UNSUPPORTED.

Every mutation re-probes physical ownership. There is no cached permission bit
or new persistent schema to become stale after reset. A zeroed retained header
never proves NOR empty. With flash enabled, verified physical ownership is
authoritative, including positive emptiness: random/stale `.noinit` bytes do not
lock genuinely erased NOR. Refetch can canonicalize such an empty cache. In a
RAM-only build the retained object is the owner: only a completely zero/erased
buffer is empty; all other unrecognized data, including a damaged first word,
is protected.

`proj_fetch` leaves retained bytes untouched when physical ownership is ambiguous.
Failed operations do not erase/program NOR, replace retained slots, change track
or song state, project name or current-slot association. Read-only probes use
transient scratch only. Status messages and transfer staging are not musical
retained state. Unknown slots report occupied and `UNSUPPORTED`, `CORRUPT` or
`READ ERROR` through the existing project-info/name interface; no new screen or
control is added. Late fetch errors do not present a known occupied slot as empty.

### Shared autosave staging owner

The autosave pair also serves FM6 voice-bank staging in diagnostic multi-engine
builds. A complete known-version, CRC-valid FM6 stage with canonical envelope
and erased padding is a recognized alternate owner. Diagnostic builds can reclaim
it as before. A future project or damaged stage in **either** copy still prevents
staging erasure. Shipping GRAIN-only builds retain their legacy FM6 read-only
policy. Partial/cancelled transfers without a valid commit remain unowned and
protected; no automatic repair is introduced.

## Audited write/erase entry points

| Entry point | Callers / protection |
|---|---|
| `project_save_as`, `project_save` in `project.c` | Device SAVE, GLO project SAVE, SAVE AS/name workflow, editor PROJECT save, copying the current/initialized/restored sound into a slot. Ownership guard runs before capture or flash/cache publication. |
| `project_rename` | Existing project name workflow. Same guard before editing the stored name or packing. |
| `project_erase` | Existing confirmed ERASE action. Same guard before zero-length tombstone commit or cache clearing. Confirmation does not bypass ownership. |
| `ed_bk_commit`, backup IDs2..5 in `editor_backup.c` | Nonempty replacement and zero-length erase both guard the destination before decoding/converting staged input. Existing result2 denotes refusal. Invalid incoming objects are rejected as before. |
| Backup runtime ID0 / `project_restore_runtime` | Changes runtime state only, never writes a project slot. A later SAVE/SAVE AS still probes its destination; restore/init cannot grant ownership. |
| `autosave_poll`, OBJ_AUTOSAVE | Probes before capture/write and latches protection on uncertainty. RESTORE LAST never reformats an unsupported object. |
| `fvb_begin` in `fm6_vbank.c` | Direct eraser of shared autosave staging sectors. New ownership check before erasing, including diagnostic builds; existing result4 on refusal. Subsequent WRITE/END require the existing successfully opened transaction. |
| `persist_boot`, `proj_fetch`, `project_load`, `project_used`, `project_info` | Read/cache paths, not NOR writers. Protection is re-established from physical bytes after reset/cache invalidation; occupancy is no longer equivalent to decode success. |
| `chain_prepare` | Requires a decoded usable source explicitly, because occupied now includes opaque objects. Never copies an unsupported slot into active song sources. |

The actual slot `st_save` calls are in save, rename, erase and backup commit.
Their A/B commit algorithm is unchanged. The low-level `st_save`/`st_save_to`
primitives remain generic internal storage operations, not raw-address user/editor
APIs; all production project-object callers above are guarded. Tests use them to
seed deliberately foreign objects, not as firmware entry points.

Other erasers were checked for overlap: sample writes are constrained to their
separate pool starting0xA0000; preset/settings/patch objects use distinct mapped
sectors; OTA staging is outside project slots and the autosave pair. FM6 staging
was the relevant shared-region eraser and is explicitly guarded. Project slots
remain0x97000..0x9EFFF; autosave remains0xE5000/0xE6000. No allocator, journal,
geometry or sample code changed.

## State transitions and recovery

- Erased/owned-empty -> supported project: existing explicit save/restore allowed.
- Supported project -> supported update/rename/empty tombstone: existing explicit
  action allowed when transport permits and the whole pair has known ownership.
- Unsupported/corrupt/unreadable -> anything destructive: refused. Retained cache
  invalidation, reset, refetch, initialization, choosing another source or backup
  input do not authorize a write to that pair.
- A transient read error can clear on a later successful slot probe; no cached
  false permission survives an error. Autosave protection is latched for the boot
  and rediscovered after reset.
- A verified known older autosave may still be **read** from a damaged pair, while
  the write/staging latch remains set. This preserves readable recovery without
  using it as permission to erase the ambiguous peer. An unknown selected payload
  is never decoded as a known project.

**No destructive recovery/force erase command is added.** Damaged/partial copies
remain read-only; use firmware that understands the object or a separately
reviewed recovery procedure. B1 deliberately refuses the old “retry a save over
an uncommitted damaged peer” behavior. A successful flash read of genuine empty
sectors is not considered damage and remains writable.

The existing archive protocol cannot represent arbitrary corrupt sector pairs
and metadata. Backup LIST capture therefore refuses ambiguous slot ownership
with existing result2; GET refuses an unrepresentable object rather than exporting
a zeroed/stale cache as project data. This protects data but is not a raw NOR
salvage facility. Export backups with compatible firmware before downgrading.
Supported stored projects are exported from physical storage through the existing
scratch buffer and canonical serializer, without changing retained slots or the
separate live-project snapshot. This also handles a missing cache. LIST computes
its bounded manifest before emitting a successful reply, so a late project read
failure returns existing result2/count0 rather than silently listing an empty
object. The manifest uses144 bytes of local stack, not a new payload buffer.
No new SysEx extension, capability or status number is allocated.

## Physical NOR validation

`tests/project_downgrade_bridge_test.c` uses the real existing
`persistence_test.c` physical1MiB NOR model: erase sets bytes to255, program is
bitwise AND. Optional test-only counters count program calls; read errors can be
injected. Every rejected operation checks the complete NOR image, erase/program
counters, retained slot buffers, all tracks, song, name and current-slot metadata.
Allowed writes to a different slot also compare the protected pair byte-for-byte.

Coverage includes frozen FUNB, historical FUNA with111 parameters, prospective
FUNC, other opaque magics including0/all-ones, recognized truncation, application
and storage checksums, invalid headers, orphan payload, programmed padding,
unknown reserved metadata, read failures, both A/B orderings, four-slot isolation,
cache loss/reset/refetch, save/save-as/rename/erase/load, initialized/restored
runtime copies, backup replacement/erase/capture, restore-last, repeated healthy
autosaves and known versus unknown FM6 staging. Supported engine incompatibility
is tested separately from opaque ownership. Layout assertions remain118 parameters,
E0=110,3840-byte project objects and192-byte preset records. WIN stays reserved.

`tests/project_downgrade_retained_test.c` separately checks RAM-only preservation
of nonempty retained objects with normal, zeroed and all-ones first words, all
four slots, plus writable empty slots. The original frozen binaries and
`grain_win_i1_downgrade_gate.c` were not altered.

### Before/after oracle

Same frozen prospective FUNC record, same original gate predicate:

| Observation | Exact I1 base99325e7 | Bridge |
|---|---:|---:|
| Setup/read assertions | 12 pass | 12 pass |
| Unknown slot read-only | 0 | 1 |
| Load result | 1 (refused) | 1 (refused) |
| SAVE result | 0 (succeeded) | 1 (refused) |
| Erases | 1 | 0 |
| Original bytes preserved | 0 | 1 |
| Stored magic afterward | FUNB | original FUNC |
| Gate exit | 1, historical failure retained | 0 |

This is an opaque future-format test; B1 does not implement FUNC/WIN storage.

## Test results and regression status

Final dedicated gates use **strict ASan+UBSan, signed-overflow checks enabled,
`-fno-sanitize-recover=all`, no exclusions**:

| Gate | Result |
|---|---:|
| Physical NOR, shipping GRAIN-only | **2,189 checks passed** |
| Physical NOR, diagnostic multi-engine | **2,183 checks passed** |
| RAM-only retained ownership | **62 checks passed** |
| Original gate applied to B1 | **12 setup/read checks passed**, preservation gate passed |
| Original gate against exact99325e7 | **12 setup/read checks passed**, preservation gate intentionally fails as historical evidence |

No sanitizer error occurred in these executed ownership paths. An exploratory
fixture using a raw FUNA patch byte128 was corrected: the unchanged legacy reader
intentionally masks that byte to seven bits, so it is not invalid after migration.
The rejection fixture now proves parser failure with a checksum-valid invalid
step count, keeping the same refusal and byte-preservation assertions. No legacy
reader/migration rule was altered to make the test pass. Tests assert
both refusal and raw-byte/counter atomicity, not merely the presence of a label.
Healthy repeated project saves and four successive autosave cycles pass, including
verification of the actually stored live parameter value. Recognized complete
FM6 staging remains reclaimable only in the diagnostic mode, and an unknown peer
blocks it in both modes. Differing check totals reflect that deliberate mode
policy and the number of refusal snapshots, not skipped safety cases.

Exact-base final-output audio comparison: **330 checks pass in each build**,
**57,344,000 bytes identical**, with explicit nonzero output. Retained navigation
comparison: **225 state/pixel snapshots,26,151,300 bytes identical**. These are
actual host renders, not a claim of target timing or a new whole-suite golden.

The broader host comparison uses ASan/UBSan with the repository's existing
historical integration exclusions for signed-overflow, shift, bounds,
object-size and pointer-overflow. Those exclusions are **not** used by the strict
B1 gates. Baseline is the exact I1 base, with identical existing generated tables
and engine interface. No assertion or golden was removed or relaxed.

| Suite / configuration | Base | Bridge |
|---|---:|---:|
| Project regression | 61 pass /0 fail | 61 pass /0 fail |
| FUN11/FUN10 format matrix | 41,146 checks pass | 41,146 checks pass |
| Generic A/B storage | 35 pass /0 fail | 35 pass /0 fail |
| GRAIN-only policy | 380 checks pass | 380 checks pass |
| GRAIN-only editor/storage | 70 checks pass | 70 checks pass |
| GRAIN-only physical persistence | 24 checks pass | 24 checks pass |
| Backup, diagnostic | 103 pass /0 fail | 103 pass /0 fail |
| Backup, shipping | 77 pass /26 fail | 77 pass /26 fail |
| Motion, shipping | 94 pass /30 fail | 94 pass /30 fail |
| MIDI controls, shipping | 122 pass /27 fail | 122 pass /27 fail |
| Broad UI, shipping | 503 pass /94 fail, then abort | same counts and same abort |
| Broad editor, shipping | 88 pass /9 fail, then abort | same counts and same abort |
| Broad persistence, diagnostic | 76 pass /0 fail | 64 pass /12 fail, policy differences below |
| Broad persistence, shipping | 45 pass /31 fail | 45 pass /31 fail, one label difference each way below |

Counts in the broad rows are printed assertions before termination, not invented
whole-suite totals. The broad UI stops at its pre-existing NULL SLICE fixture
(`ui_test.c:3911`); broad editor stops at its pre-existing NULL mapping input in
`core.h:110 params_by_count`. These match the base; B1 does not fix or hide them.

### Deliberate persistence-policy differences

The diagnostic persistence suite first injects a failed write that leaves a
nonempty uncommitted peer, then expects a retry to overwrite it. B1 protects that
peer. The following **12 existing assertions remain failing**; they are not
silently changed to new expectations:

- successful project save publishes new RAM;
- a project's name goes to flash and comes back;
- failed project rename reports SAVE ERROR;
- pending PLAY: no rename, no erase;
- project rename publishes the new name;
- failed project erase reports ERASE ERROR;
- project erase refused while playing/on an empty slot;
- project erase empties RAM and flash;
- an erased slot takes a new save;
- damaged FUN7 slot is refused **and considered unused**;
- loading the damaged slot says **EMPTY SLOT**;
- autosave wear test expects new writes after a torn autosave peer.

The first nine cascade from trying to reuse the damaged pair without an explicit
recovery step; readonly refusal supersedes later SAVE/ERASE/transport error
expectations in that sequence. The next two conflict with the required occupied
corrupt-slot status. The last conflicts with the intentional no-auto-repair policy:
the existing wear test continues editing over the previously torn pair. Dedicated
healthy NOR cycles verify that valid, undamaged pairs still save/load/rename/erase
and autosave successfully. B1 is not claiming the historical broad persistence
suite is green.

In shipping persistence the same damaged-load label expectation is newly failing;
the old cut-write/reboot assertion now passes because its fixture correctly clears
the boot-local protection latch on simulated power-on. `persistence_test.c` changes
only test-only optional read/program instrumentation and proper BSS-latch reset
in `reset`/`as_power_on`; **all assertions remain intact**.

## Reproduction

With existing generated host headers and the pinned engine submodule initialized:

```sh
BRIDGE_BASE_TREE=/path/to/clean-99325e7 \
BRIDGE_OUT=/tmp/coffee-grains-project-bridge \
  tests/run_project_downgrade_bridge.sh
```

The runner builds both physical modes, retained-only tests, the bridge oracle and
then the untouched historical oracle against the exact base. It verifies that the
historical gate fails specifically with its12 setup checks and lost future bytes,
not merely with any nonzero exit. No toolchain or dependency download is performed.

Existing regression sources can be compiled with their ordinary host flags and
`-Ibuild/gen -Ifirmware/src -lm`; `-DFELUCCA_GRAIN_ONLY=0` selects diagnostic
coverage. Audio/navigation comparisons use `grain_only_audio_test.c` and
`grain_only_navigation_test.c` with their existing `GRAIN_ONLY_UI_SOURCE` /
`GRID_UI_SOURCE` include overrides pointing at the exact base and this tree,
then compare generated binaries. Use `-ffp-contract=off` consistently. Broad
integration exclusions and aborts are stated above; do not substitute them for
the strict ownership gate.

## Changed files, limits and readiness

Production changes are limited to:

- `firmware/src/storage.c`: read-only physical envelope/erased-range probes;
- `firmware/src/project.c`: ownership classification and guarded/cache-safe project
  paths, independent occupied status, autosave protection;
- `firmware/src/editor_backup.c`: existing restore guard uses physical ownership;
  refuse unrepresentable backup capture/GET instead of inventing empty data;
- `firmware/src/fm6_vbank.c`: guard shared staging erasure.

Test changes are `persistence_test.c`, two new bridge test units and the dedicated
runner. This report is the only documentation addition. Frozen fixtures and old
oracle are unchanged. No audio engine, parameter descriptor, sample asset,
serialized record or storage geometry changed. There is no new persistent guard
record, buffer, framebuffer or allocation; probes reuse existing storage scratch
and local headers (plus the144-byte local LIST manifest). Work is on the
main/control path, not the audio callback.

No target compiler was installed, hardware flashed, deployment or target link
performed. Target flash fit, stack and real read/GUI latency remain unmeasured;
this is a source/host-qualified checkpoint, not a hardware release claim.
Repeated physical inspection favors correctness over an invalidatable permission
cache; hardware profiling is still required before release.

**B1 is the minimum supported source/host downgrade baseline for resuming WIN
I1.** The audited firmware entry points cannot replace an unsupported nonempty
project pair in the tested shipping or diagnostic modes. Old unpatched releases,
stock firmware, external flash writers and arbitrary memory corruption are not
made safe by this bridge. Keep backups and downgrade only to this checkpoint or a
newer compatible guard. Corrupt/partial objects have no destructive recovery path
in B1; introducing one is separate work.

Future WIN I1 must use B1 as its previous-revision oracle and still validate its
actual new format, retained schema, preset and bulk-import provenance. None of
that WIN work is activated here. No merge, deployment or flashing.
