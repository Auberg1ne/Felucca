# GRAIN WIN production integration design

Status: proposal only, no shipping implementation or format change authorized by
this document. Source baseline: `27de71d9d0f48719e8e131ecfcfae08e44ac9ce5`.
Branch: `design/coffee-grains-window-integration`.

## Recommendation

Use **parameter ID 109**, currently `P_GFILTER_RSVD2`, as an engine-aware GRAIN
`WIN`, range 0–127, default 64. Preserve its numeric identifier, P_COUNT=118,
P_E0=110 and every existing engine parameter ID. Non-GRAIN descriptors remain
reserved 0–0. Do not reuse ID108 (STRETCH), 115 (grain spray), or 117 (TONE).

**There is no safe unversioned direct reinterpretation of 109.** Existing
118-parameter records already contain that slot, typically zero, and zero must
mean Gaussian in the proposed new interface. Historical raw bytes cannot prove
that a user explicitly selected a window. Recommend explicit new project/preset
semantic versions, with legacy imports unconditionally setting GRAIN WIN=64 in
runtime state. Keep old stored bytes untouched until an explicit save. Protocol
extensions and new versions require implementation approval first.

Smallest first implementation slice: version-aware conversion/default tests and
headless ID109 plumbing (descriptor, initialization, versioned project/preset
round trips), with WIN=64 audio identity as a gate. Add device/browser controls
only after provenance and downgrade protection pass. Hybrid fidelity remains
experimental and default-off in every stage.

## Verified source inventory

References below are baseline file/line locations; functions are the durable
references if subsequent edits move lines.

| Area | Verified implementation and implication |
|---|---|
| IDs/storage | `firmware/src/core.h:76–108`: reserved slots before eight engine values; `params_by_count` fills missing common IDs from defaults but copies present IDs. `track_t.p` at :256 is `int16_t[P_COUNT]`. `tests/grain_stretch_test.c:9`, `tests/upreset_test.c:290` assert count118/E0=110; enum makes RSVD2=109. |
| Descriptors | `params.c:181–182` has reserved 108/109, both min=max=default0. `param_desc_of` :242 supplies engine-aware STRETCH108; `track_desc` :251 additionally handles personality labels. Extend the canonical engine-aware resolver, not just screen labels. |
| Grid | `params.c:555–571` has two 3×4 grids and shared row state. Page1 row3 is STRETCH/empty/empty/empty; page2 is TYPE/A/B/empty, CUTOFF/RESON/empty/empty, VCE/DTUNE/V-SPRD/empty. `page_param_id` :598 routes page-relative consumers; empty sentinel255. |
| Rendering | `grain_edit_grid.c:13` resolves every cell, formats its real descriptor/value and caches per-cell signatures. `ui_draw.c:1756` dispatches the grid; `ui_input.c` and `ui_knob_motion.c` consume the authoritative page resolver. No new page/row navigation required. |
| Initialization | `ui.c:1375 apply_preset_to` resets common non-kept values using **TP defaults**, not canonical engine descriptors; `track_defaults` :1546 does likewise. Other initialization/load/reset loops must be audited together; an engine-aware descriptor alone does not fix factory/default WIN. |
| Projects | `project.c:103–105` uses FUNB serialized format11 but **retains FUNA internal/cache magic**. `PROJ_STORE_SIZE` :162=3840, FM6 :171–179 is112 bytes/track. `proj_unpack` :745 maps by count without a WIN provenance distinction. `proj_pack` :659 writes all118 parameters as value+64 bytes. |
| User presets | `upreset.c:51–65`: versions4/5, fixed192-byte records,144 packed-value bytes, count stored in record. `up_value` :128 decodes value+64; `up_values` :286 maps by count then engine-aware clamps. `up_parse` :230 accepts current-count arrays. A count of118 alone cannot distinguish WIN-aware records. |
| Factory presets | `eng_grain.c`'s `ENG_GRAIN` preset records carry eight engine values plus common preset attributes, not an explicit109 field. `apply_preset_to` must explicitly establish Hann for existing factory templates. Factory audio/personality records need no edits. |
| Editor | `editor.c:489 ed_tdesc`, `ed_desc`, ED_GET/SET :713, ED_DESC :753 use canonical descriptors. ED_INFO :683 reports counts/E0 and appended capability tags; :711 uses tag0x47 for GRAIN-only. TRACK_PARAM :1077 targets a specific track; TRACK_DUMP :1054 and UP_GET/PUT :919 onward carry full parameter arrays. |
| Browser editor | `web/editor.html:2892 knownLayout` recognizes older layouts, not118/110; current layout uses generic descriptor-driven controls via `buildGroups` :3166. `rereadEngineDesc` :3069 refreshes STRETCH and E0..E7, **not109**. Its mock at :1830 still models111/103; do not mistake it for current WASM state. Library format at :1061 is version1 with parameter arrays/layout metadata. |
| Motion/MIDI | `motion.c:34 motion_param` explicitly permits STRETCH108 but not109. `motion_put` :165 uses canonical descriptor fitting. `midi_control.c:232` uses `param_desc_of`; `midi_learn.c:55 ml_learn` stores IDs <P_COUNT, with existing persistent coding. No new MIDI CC assignment is necessary. |
| Modulation | `mod.c:53–68` uses stable destination enums and maps engine destinations only to E0..E7; WIN109 is **not** an existing destination. `mod_begin` temporarily changes parameters; its descriptor lookups :211/:226 are not generally engine-aware for common slots. Do not repurpose SHP or an existing matrix destination. |
| DSP | `eng_grain.c:108` already has a conditional window byte in grain-record padding. :376–381 latches experimental discrete/morph controls at spawn. `gr_morph_win` :424 explicitly routes morph64 to the original Hann lookup; `GR_RENDER_WIN` :433–443 is the evaluator switch. `grain_morph.h` uses fixed64/63 denominators, Q15 interpolation. |
| Threading | `audio.c:97` is the hardware audio ISR. `fx.c:988–1077` owns per-track rendering, dispatches C1 jobs and waits before returning; `events_block` runs before the split; `mix_part` :885 calls `mod_begin` before rendering voices. CPU1 executes own-track state, not a full copied track snapshot. `fm1_cpu1.h:150–162` publishes jobs with `FM1_C1_SYNC`. Browser `web/emu/worklet.js:143` invokes WASM `web_render` within the audio worklet. |
| Flags/resources | `eng_grain.c:44–60` gates window/morph/fidelity/hybrid separately. `build.sh:12–16` needs JieLi pi32v2 and AC79 SDK. `firmware/app.ld:16–21` declares XIP0x8DFBC=581564 bytes, RAM72KiB, POOL360KiB, plus distinct RAMTEXT/NOINIT. Native experimental sizes are not a target link proof. |

## Compatibility architecture (approval required)

### Why defaults alone fail

Changing the descriptor's default to64 fixes only absent fields. FUN11 and modern
user records include109 already, so `params_by_count` copies their zero (or any
validly encoded historical reserved value). A heuristic such as “zero means old
Hann” would prevent genuine WIN=0 Gaussian and would corrupt round trips.
Do not use source ID, personality, parameter count118, preset name, or firmware
version strings as a substitute for explicit storage provenance.

### Versioned storage proposal

- Propose project **format12**, distinct magic (e.g. `FUNC`, following current
  FUNA/FUNB numbering), with unchanged3840-byte allocation, offsets and checksum.
  Only semantic interpretation changes. Preserve readers for all previous formats
  and all their existing migrations. Version numbers/magic must be checked for
  collisions again at implementation time.
- All pre-WIN serialized project readers: first perform existing count/engine/
  historical migrations, then set109=64 **for GRAIN only**, regardless of its raw
  reserved byte. Validate new-format WIN normally; reject malformed out-of-range
  new values rather than treating corrupt data as an old record.
- Also version the **internal retained/noinit project schema magic** or explicitly
  migrate it with known provenance. Current `PROJ_MAGIC` remains FUNA despite
  FUNB serialization; updating only the disk reader misses retained slots and
  RESTORE LAST/runtime backup paths. Old runtime schemas must import as Hann;
  new schemas must not masquerade as old to downgraded firmware.
- Propose user record versions **6/7** for normal/grid records, preserving192-byte
  layout and bank shape. `up_grid` must explicitly recognize3/5/7; the old
  `ver>=4` packed-value rule can remain only after version validity checks.
  Versions1..5 always import GRAIN WIN=64. New versions persist all0..127 exactly.
  Do not bump bank geometry/magic merely to signal a field: old code can clear a
  wrong-shaped bank. Existing unknown-record byte preservation must remain.
- New explicit saves write the new versions; no boot-time flash rewrite. Factory
  initialization and all factory-template loads explicitly yield64. Preset undo,
  full sound copy, project slots, restore-last and diagnostic multi-engine loads
  need the same default/provenance policy; non-GRAIN109 remains reserved.
- `editor_backup.c` already transports raw objects and validates before commit.
  Keep archive/object IDs, record sizes and checksums stable; dispatch validators
  by version. Reject unsupported future versions safely. Backups with old formats
  retain original bytes; restore/load applies legacy interpretation only in RAM.

### Downgrade policy

Safe downgrade means **refusal/preservation**, not pretending an old engine can
render a new window. Current project readers reject an unknown magic. Current
`up_valid` rejects records beyond5, while `up_bank_check` preserves invalid
records when the bank shape matches; GRAIN-only `grain_bank_readonly` also blocks
writes to banks containing used unsupported records. Test these exact behaviors
by executing the old revision as the downgrade oracle, including restore-last
and editor backup imports. Do not promise this for every historical multi-engine
firmware: some old clients/firmware may allow explicit erase or unsupported bank
rewrites. Keep an exported backup and warn before downgrade; never auto-convert
new windows to Hann on disk. An optional **explicit lossy legacy export** could
set WIN to Hann and strip provenance, but is a separate future feature.

## UI and renderer integration

Place WIN at **GRAINS page1 / row3 / K2**, next to STRETCH:

```
SRC      POS      SIZE     DENS
PTCH     G-SPRD   RAND     TONE
STRETCH  WIN      empty    empty
```

Page2 remains unchanged. Keep SELECT's three-row wrap/entry behavior, EDIT tap
page switching, modifiers/GLO tempo priority, remembered source-page automation
resolution, two distinct spread controls and all empty sentinels. Update only
`GRAIN_GRID`, engine-aware descriptor formatting and focused tests. Display0/64/127
with GAUSS/HANN/TUKEY anchor names (numeric value retained in editor); intermediate
values use a concise numeric format, not a false discrete window label. Cell
fitting/readability must be verified at native240×240 before choosing formatting.

Promote only the validated static Gaussian9/Tukey tables, morph evaluator and
spawn capture into production. Retain original Hann table and original A render
kernel. WIN=64 must call `gr_win` directly at every envelope evaluation, without
new normalization, coefficient changes or altered scheduling. Clamp/sanitize an
invalid runtime WIN to64, never read outside tables. Capture one valid0..127
value into the existing grain byte (e.g.128+WIN); all active grains keep theirs.
Assert host and target grain/part sizes and padding before making the byte
unconditional. No additional per-grain allocation is proposed.

Window morphing with A still has the documented block-endpoint approximation
and short-grain silence limitation. **Do not enable B/C/HYBRID** as part of this
feature. Keep their test flags/selectors default-off and separate. Exact Hann
means exact PCM for equivalent state/notes/seeds under A, not an assertion that
all non-Hann windows have equal loudness or that Hann plus HYBRID sounds legacy.

## Browser/editor and protocol proposal

Keep existing ED_GET/SET/DESC, TRACK_PARAM/DUMP, counts and IDs. Updated firmware
returns canonical WIN metadata for GRAIN and reserved metadata for other engines.
Explicit parameter SET109 is an explicit WIN edit; descriptor discovery prevents
new clients from offering WIN on old firmware. Append an optional ED_INFO
capability record identifying WIN support, ID109, default64 and new storage
semantic generation. **Allocate the tag only after checking the full tag registry**;
0x47 is already occupied. Existing parsers must skip unknown tagged capabilities.
This is an additive protocol extension, not a change to old command layouts.

Bulk preset/library imports need provenance too: an old118-value array with109=0
is not an explicit Gaussian selection. Propose a capability-gated optional
WIN-aware trailer/new versioned bulk-import envelope, keeping legacy UP_PUT
payloads accepted with legacy WIN=64 semantics. Existing `up_parse` strictly
checks payload lengths; update it deliberately and test old firmware rejection
of extended payloads. An old client read/modify/write of a new non-Hann preset
must not silently erase WIN: reject ambiguous legacy replacement of a WIN-aware
non-Hann target and request an updated client. Return existing error framing;
choose/document a supported status before implementation. Do not silently guess
whether an arbitrary raw parameter array is old or new.

Browser library exports should use an explicit WIN-aware semantic marker/version
(proposed library version2) and retain all stable array IDs. Old/unmarked imports
force Hann; new marked imports preserve0. Explicit new imports to old firmware
must be rejected before sending, or require a separately approved lossy export.
Refresh descriptor109 on engine change alongside108; ensure generic118/110
controls show it. Extend the mock/capability tests, library conversions and bank
save/load code, rather than changing the old `knownLayout` list blindly. The
browser simulator must compile the same firmware feature path as hardware;
its worklet should not synthesize WIN behavior in JavaScript.

## Motion, MIDI and modulation policy

Initial slice: ordinary explicit WIN edits; MIDI learn is gated on the persistent-map provenance issue below. No invented fixed CC.
Keep all existing CC/NRPN and editor command meanings. Verify per-track fitting
and non-GRAIN reserved behavior using `midi_control.c` and canonical descriptors.

`midi_control.c:249–257` reserves learned engine codes120..127; common
ID109 encodes as110, so it fits without shifting any map code. However,
`ml_learn` accepts any ID<P_COUNT, including reserved109 on old firmware.
An old stored mapping could therefore become newly audible even if projects
migrate correctly. Maps live in `favorites.factory[14]` (`midi_learn.c:12`),
inside PER4 settings (`settings_persist.c:28–36`). Do not silently activate those
historical entries. Before persistent WIN learn ships, approve an explicit
settings semantic marker/version (e.g. a versioned PER successor, retaining all
old readers/fields) identifying WIN-aware maps. Legacy109 entries should be
quarantined/inactive in RAM until explicit re-learning, with old raw backups
preserved. Unversioned legacy editor-learn table uploads need the same treatment;
capability-gated new uploads must identify WIN intent. Exact marker placement and
downgrade behavior require a separate settings-format audit/approval, not an
assumed spare byte. Until then, leave109 learn unavailable rather than introducing
partial persistence that loses mappings at reboot. Parameter SET remains usable.

Next explicit gate: permit109 in `motion_param` only after versioned project
support, validate engine-aware values on insertion/playback, and test locks,
motion restoration/rebase, UI source-page targets and undo. New WIN records belong
to new-format projects. Never reinterpret an invalid old automation record as a
WIN event or drop unrelated events. Motion/lock changes affect future spawns;
already-running grains remain latched. Use current128-record budget, no new kind
or record layout. Motion policy is a deliberate eligibility addition, not an
accidental consequence of rendering a new cell.

Do **not** add a continuous MOD MATRIX destination in the first implementation.
Current E1..E8 do not include109; SHP already has a different meaning. Future WIN
modulation requires an append-only destination ID, capability/compatibility audit
and engine-aware temporary-parameter handling in `mod.c`. Reserve no destination
or parameter ID now. Spawn-only capture inherently samples a continuously
changing control at grain admission; there is no continuous deformation of an
active envelope.

## Publication and thread-safety plan

Current test arrays are not a production cross-thread control interface. Main
UI/editor writes and parameter-batch loads must use the existing control-to-audio
serialization contract; `load_begin/load_end` manage undo/motion and are **not by
themselves proof of mutual exclusion**. Do not rely on `volatile` as synchronization.

Propose one audio-owned WIN snapshot per part (four bytes, not per voice/grain),
captured once at the owning `mix_part` boundary after effective parameters are
established and before its voice renders. Every spawn in that part/block uses
that snapshot, including both SHIMMER spawns. Publish completed parameter changes
before dispatch; no UI writes to grain records. Hardware main-loop code is paused
while the audio ISR executes, and the ISR waits for C1 before returning; preserve
that ownership. CPU1 visibility uses the existing job synchronization barriers,
not a new unsynchronized shared test array. Audit TIMER/MIDI interrupt nesting so
no higher-priority producer mutates WIN mid-snapshot; queue such updates for the
next audio-owned control boundary if necessary. Keep batch loads transactional.

The snapshot capture must be located against actual motion/lock application and
restoration (`seq.c` events, `motion.c`, `mod_begin/end`), not assumed from UI
ordering. Test that a lock influences the intended block's spawns before it is
restored. Browser worklet messages/WASM rendering should apply controls at quantum
boundaries under one worklet owner; any shared-memory transport must use an atomic
mailbox rather than a JS main-thread pointer write. Expected latency is the next
part/block boundary, plus the existing spawn interval. No allocations, locks or
blocking in the callback. C1 fallback/retry must reuse the same snapshot.

## Required file changes (future, not performed)

| Files | Minimal purpose |
|---|---|
| `core.h`, `params.c` | Alias/name ID109 without renumbering; canonical descriptor; grid cell and default helpers. |
| `ui.c`, `project.c`, `upreset.c` | Every initialization/factory/reset/load path, versioned provenance, noinit handling, exact old import and downgrade policy. |
| `editor.c`, `editor_backup.c` | Capability advertisement, unchanged parameter commands, versioned bulk provenance, validators and non-destructive errors. |
| `web/editor.html` | Descriptor refresh, capability gates, library versioning/import/export, mock and bulk preset handling. |
| `eng_grain.c`, `grain_morph.h`, existing window table header | Production evaluator/capture only; leave hybrid/fidelity flags disabled. |
| `fx.c` and control publication sites as needed | Audio-owned per-part snapshot and verified C1 ownership/visibility; no general DSP rewrite. |
| `motion.c`, `midi_control.c`, `midi_learn.c`, `editor_learn.c`, `settings_persist.c` | Explicit eligibility and learned-map provenance gate and descriptor/path tests; no new MIDI mapping or record kind. |
| `web/emu` build configuration, target build configuration | Consistent window feature availability, independent fidelity experiment switches. |
| Focused tests | Extend project-format/upreset/editor/backup/STRETCH canonical-descriptor patterns; add WIN kernel/latch/default/migration/UI/browser/dual-core cases. |

## Risks and staged gates

| Risk | Required gate |
|---|---|
| Old zero becomes Gaussian | Every old version/count, raw109=0/64/127 and arbitrary legal reserved values must load64; checksum/source bytes unchanged until explicit save. |
| No provenance in legacy wire arrays | Marked new import preserves0; unmarked legacy import yields64; ambiguous overwrite of existing non-Hann data refused without modifying its record/bank. |
| No-init/cache bypasses disk migration | Reset/restore-last, raw backup and retained-slot upgrade/downgrade fixtures execute both revisions. |
| Unsupported preset erased on downgrade | Run exact base loaders/writers on new6/7 records; prove preservation/read-only bank behavior; reject unsupported future versions. |
| Factory initialization still uses TP0 | New project, all four tracks, INIT/CLEAN, template/program change, undo and sound copies assert64. |
| Thread/spawn inconsistency | Dual-core and one-core byte identity, fallback/retry, timed updates around spawn, SHIMMER pair consistency, no active-grain mutation; sanitizer/TSan where supported. |
| UI targets diverge | Resolver/edit/learn/lock/motion tests for WIN K2 and unchanged empty cells; native bounds and unrelated snapshot identity. |
| Audio/resource growth | Legacy A PCM exact and nonzero for all personalities/sources/four tracks; linked target sizes/stack and deadline measurements before release. |

1. **Compatibility fixtures first:** freeze old record/version bytes and downgrade
   oracles, approve proposed format/protocol semantics. No UI or new DSP yet.
2. **Headless persistence slice:** descriptor109/default64, versioned loaders/writers,
   noinit/backup and bulk provenance protections. Round trips0/1/63/64/65/126/127,
   all tracks, malformed/truncated/future data, engine switching and no writes on
   rejected input. Preserve3840/192-byte allocations and other parameter values.
3. **DSP promotion:** use existing tables and grain byte; snapshot/spawn latching.
   Legacy/default Hann byte-identical to this exact base; previous morph PCM exact
   with A; window anchors/endpoints/phaseUINT32_MAX, active-grain continuity,
   forward/reverse/user/sine/high-density/four-track strict ASan/UBSan tests.
4. **UI/browser/editor:** add WIN at page1 row3 K2, retain navigation; verify versioned MIDI
   learn/map provenance, library provenance and old/new capability combinations. Motion/locks
   enter only through their explicit gate; matrix modulation remains deferred.
5. **Target qualification:** target link and section/stack measurements, CPU2 ON/OFF
   and fallback, actual FM1 deadlines including effects. Keep experimental fidelity
   default-off even if morphing passes. No release-fit claim from host timings.

No tests/builds were run for this documentation-only audit; findings are source
inspection, not new implementation evidence. No code, sample, preset, format or
protocol changed. The earlier hybrid report's missing-toolchain result remains a
limitation, not a new target measurement. This document intentionally requests
separate approval for future format/protocol revisions. No merge/deploy/flash.
