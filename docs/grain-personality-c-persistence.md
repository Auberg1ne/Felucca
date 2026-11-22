# Personality-C versioned persistence — Step240F

Base: `37187a52589f8f736172b4f77957b7eff4a47520`, branch
`feat/coffee-grains-granular-quality`. Local implementation only; no push,
merge, release or flashing. C is headless storage/scalar control, not a DSP feature.

## Contract and version decisions

`P_GRC=118`, internal `P_COUNT=119`; IDs0..117 and E0..E7=110..117 are unchanged.
GRAIN's canonical C descriptor is0..100/default0. Other engines reserve118 at0.
One value follows TYPE (0 future SPREAD,2 CHAOS,1 DRIFT,3 SPARKLE); switching TYPE
retains that one value. No personality-dependent DSP or grid cell is introduced.

The accepted registries at the exact base contained projects1..12 and presets1..7.
The frozen Step240A oracle independently confirms availability of FUND13 and8/9.

| Object | New marked representation | Legacy import |
|---|---|---|
| Packed project | FUND (`0x46554e44`), format13, count119, stable IDs | Formats1..12 retain their readers; C=0 |
| Native runtime | FUND,4848 bytes, named-field schema | Frozen FUNA/FUNC4840-byte structure migrates explicitly; C=0 |
| Normal user preset | Version8, count119, stable IDs | Versions1..7 retain historical tail-eight mapping; C=0 |
| Grid user preset | Version9, count119, stable IDs | Historical grids3/5/7 retained; C=0 |

Only new marked formats interpret stored index118 as C. In particular version4
count119 still maps input111→E0/110 and input118→E7/117, **not C**. `params_by_count`
is historical only; stable-format decoding is separate. WIN-aware6/7 presets and
FUNC projects preserve WIN, older formats retain their Hann migration.

Project allocation stays3840 bytes, preset record192, bank3080, and all bank and
NOR addresses unchanged. Four extra packed project bytes consume spare space
(49→45 bytes before the fixed MIDI area). MIDI3373, FM63376, name3824 and checksum3836
remain fixed. Native runtime schema grows4840→4848; it is not the packed slot.
Retained slots remain wire objects, not widened native memory. Old retained and
restore-last objects pass through explicit import. Boot/load never rewrite old NOR
merely to migrate it. Undo and native snapshots include C.

The engine descriptor index is explicitly bounded to110..117. Historical motion
migration uses frozen118 arithmetic, never new `P_COUNT`. Automation base arrays
and restore loops remain118-wide; C cannot be recorded, locked or restored by
motion. MIDI codes120..127 still encode only110..117;118 has no learn code. No
modulation destination or fixed CC is added. DSP and graphics sources are unchanged.

## Atomicity, physical ownership and downgrade

Project import validates in a private main-loop staging object, publishing only on
success. Null, unaligned, truncated, checksum-invalid and unsupported inputs are
refused. Every production ingress buffer is aligned. This costs4848 bytes in
`.pool`, without a heap/audio allocation or additional callback work.

Step240C's physical A/B ownership checks remain authoritative for every bank
mutation. New provenance restrictions inspect **both** physical copies, independent
of selected copy or browsing cache. Legacy project/bank backup replacement or
clear cannot discard C in either copy. Legacy preset-slot commands also inspect
physical copies: they refuse a C-bearing target, or an adjacent write whose
browsing cache no longer exactly preserves other C-bearing records. This can
restrict legacy operations until a fresh/current native state is available; it
never treats a lost cache as permission to replace C. Fully validated FUND or preset8/9 records
provide explicit representation, including C0. A blank preset row has no such
provenance: raw bank clear/deletion is refused until supported native operations
retire C, including the inactive copy. No force-clear/recovery command is added.
Raw bank exports read authoritative physical bytes, preserve C after cache loss,
and fail closed on read/ownership failure instead of exporting a false empty bank.

An actual new FUND/C100 project and version8/C100 bank were fed to the unchanged
Step240E production reader. Its project and preset-bank guards refuse overwrite
in both physical directions and retain exact raw bytes, including cache loss.
This is evidence for the guarded checkpoint, **not arbitrary older firmware**.
The frozen historical unsafe oracle is retained unchanged: truncated/unknown/CRC
recovery on older unpatched firmware can destroy future data. External backups
and a guarded minimum downgrade revision remain necessary.

## Editor contract

See [EDITOR_PROTOCOL.md](../web/EDITOR_PROTOCOL.md), Step240F section, for exact
frames. Legacy ED_INFO count remains118; legacy scalar and bulk layouts do not
change. Command82's transport version1/layout1, nonce/USB binding and5000ms expiry
remain. Two tag58 envelopes advertise full capability-v2 count119/flags23 and the
unchanged compatibility-v1 count118/flags7. The old Step240E browser successfully
negotiates118 against this firmware; the updated browser also works with Step240E
and older firmware. Unknown capability versions fail closed.

ID118 requires explicit119 negotiation. An118 lease cannot address it; an118
snapshot of nonzero C is refused. Full snapshots include119 real values. New
capability-gated op5 reads globals so the existing browser can refresh live state
without an unsafe truncated legacy DUMP. No extended bulk write is advertised.

Legacy unrelated scalar edits preserve C. Destructive legacy replacement/load,
UP_PUT/ERASE and occupied-slot STORE refuse nonzero C. Unrepresentable legacy
reads use existing no-reply semantics. Native STORE into an empty slot captures the
complete versioned state. The browser stores C separately from its118-control UI;
there is no C control. Unmarked library capture/export/upload/audition refuses C
before replacement. Historical119 and existing WIN provenance rules remain; no
new library version, ambiguous fallback, MIDI encoding or non-Hann bulk permission.

## Reproducible qualification

Clean reference trees must point to the pinned revisions, with their local engine
submodules and current generated headers available. Outputs go outside Git:

```sh
python3 tests/run_editor_stable.py \
  --baseline-tree /tmp/coffee-grc-fd7 \
  --e-tree /tmp/coffee-e-371 \
  --oracle-tree /tmp/coffee-preset-evidence-606 \
  --regressions --out /tmp/grc-240f/qualification
```

Pinned trees: fd7f6f0 (Step240C),37187a5 (Step240E),6062014 (unchanged unsafe
historical evidence). This runner compiles actual old-source downgrade/client
oracles; it does not substitute new readers or rewrite old expected failures.

| Gate | Result |
|---|---:|
| Step240A frozen migration/oracle proof |132084 checks passed |
| Historical unsafe downgrade counterexamples, unchanged |8229 checks passed, demonstrating old unsafety |
| Current WIN/persistence/NOR compatibility |5818 checks passed |
| New C persistence/mappings/atomic imports, including export checks |109038 checks passed |
| Physical C backup/cache/restore-last/power-loss checks |320 passed |
| Step240C both-copy ownership |26299 checks passed |
| Project downgrade bridge |2189 checks passed |
| Command82 validation/negotiation |2909 checks passed |
| Real C scalar/legacy refusal/defaults/undo |2259 checks passed |
| Existing WIN editor |110 checks passed |
| Browser actual native gateways, four compatibility combinations |89 checks passed |
| Existing browser WIN provenance |11 checks passed |
| Exact Step240E downgrade with actual new C100 objects |362 checks passed |
| Frozen legacy persistence reader |5804 checks passed |
| Short-envelope kernel |1851392 cases /152924099 checks;306→0 silent cases |
| STUTTER gate |42069041 checks;16384 rate/gate combinations |
| Filter transition |3553312 checks passed; worst first edge0 |

Native storage/protocol/ownership and strict kernels use ASan+UBSan with recovery
disabled and no signed-overflow exclusions. The retained full mixer PCM harness
continues its existing signed-overflow/shift exclusions for unrelated legacy
mixer/ADPCM arithmetic; bounds checks remain enabled. No new exclusion was added.

All24 existing nonzero PCM captures at actual C0 match their frozen Step239
expectations, including RNG, scheduling, admission, decoder and load hashes. A
separate process repeat matches. All24 with actual C100 also match C0 PCM and
state exactly. No binary fixture or golden changed. UI mapping assertions retain
the empty fourth personality cell. User-preset tests pass.

Broad suites were run against the exact Step240E source and current source with
the same host flags/generated inputs. Failure lines are identical:

| Suite | Base | Current |
|---|---|---|
| Editor |91 ok,22 failed, then SIGSEGV |same |
| Motion |94 ok,30 failed |same |
| Backup |78 ok,25 failed |same |
| Robustness |10 ok,3 failed |same |
| User presets |pass |pass |

The standalone historical project test does not compile in either shipping build
(`PM_SYMP`/`PM_MODAL` unavailable); real project coverage above uses current/frozen
NOR fixtures instead. Motion host compilation needs the existing
`-Wno-implicit-function-declaration` accommodation for graph_wheel/graph_fm6 in
unrelated graphics; those sources are unchanged. These inherited failures are not
reported as green suites. No new focused regression or sanitizer failure occurred.

## Resource measurements and limitations

Same-source `cc -Os -ffp-contract=off -fstack-usage` host harness comparisons against
37187a5 (not target firmware sizes):

| Harness | text delta | rodata delta | data delta | BSS delta |
|---|---:|---:|---:|---:|
| Editor |+1472 |0 |0 |+4864 |
| Physical persistence |+625 |0 |0 |+4864 |

Host fixtures strip section attributes, so their BSS includes target `.pool`
storage. Principal RAM cost is the4848-byte atomic import stage, plus native
schema/watch bookkeeping. `track_t` remains1840 bytes (including a32-bit layout
probe), parameters236→238 bytes consumed from padding. Grain record64 and grain
part7296 remain unchanged. CPU2=0 already fails the track-size alignment assertion
on the exact base with this host ABI; this task does not claim to qualify that
configuration. Default CPU2=1 assertions pass. Representative host stack frames
are unchanged: editor handler640 bytes, runtime restore240, backup export528;
these are compiler frames, not a target call-chain/interrupt-stack measurement.
No bitmap/framebuffer/audio buffer or dynamic allocation is introduced.

No JieLi pi32v2 compiler is installed at the configured/default location or the
workspace/opt paths checked. No installation/download was attempted. Actual target
text/flash/RAM/POOL fit and target stack remain unverified. The larger control-only
stage must be included in the next real target link; host deltas do not prove fit.

## Changed files and remaining work

Production metadata/storage/protocol: core.h,params.c,project.c,upreset.c,up_fm6.c,
ui.c (defaults only),motion.c,midi_control.c,midi_learn.c,editor.c,editor_stable.c,
editor_learn.c,editor_backup.c under firmware/src. Browser: web/editor.html,
web/test_stable_protocol.mjs,web/EDITOR_PROTOCOL.md.

Tests: grc_persistence_test.c,grc_editor_test.c,grc_backup_safety_test.c,
grc_downgrade_oracle.c,grc_win_compat_test.c,editor_stable_test.c,editor_test.c,
granular_quality_test.c,granular_quality_storage_test.c,motion_test.c,
preset_bank_ownership_test.c,project_downgrade_bridge_test.c,upreset_test.c,
run_editor_stable.py,run_granular_quality.py,run_grc_migration.py under tests;
this report. Frozen migration evidence and asset sources are unchanged.

Next safe work is a target link/memory check and a separately reviewed C-aware
library design if required. DSP meanings, UI exposure and automation remain future
work. C must stay quarantined until those implementations are independently tested.
