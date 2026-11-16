# WIN I1-R: headless versioned persistence

Base: `6055566f6d6f3a9735d822cf4d1f6283c1d23e57` (B1).
Branch: `feat/coffee-grains-win-i1-versioned-persistence`.

## Status and acceptance

The headless parameter/storage slice is implemented. All eight I1-R gates pass.
**No production WIN DSP, device/browser WIN control, morph renderer or hybrid
renderer is activated.** Changing WIN currently changes stored control state,
not audio. I2 DSP integration and I3 UI integration remain separate work.

B1 is the minimum supported downgrade baseline for the shipping GRAIN-only
build. This does not make earlier firmware safe. The original unpatched oracle
still proves a destructive overwrite; its fixture, source and expected failure
are preserved unchanged.

## Contract and formats

| Item | New behavior | Unchanged invariants |
|---|---|---|
| GRAIN ID109 | `WIN`, integer 0..127, default64 | Alias of `P_GFILTER_RSVD2`; no enum insertion |
| Other engines ID109 | Reserved, range/default0 | No engine parameter layout change |
| Project object | FUNC, `0x46554e43`, version12 | 3840 bytes, parameter start68, count byte66, all field offsets |
| Retained runtime schema | FUNC magic, existing size4840 | Same `project_t` fields and FNV checksum |
| Normal user record | Version6 | 192 bytes, UPB1 bank header and 16-record geometry |
| Grid user record | Version7 | Same pattern encoding and record size |
| Settings | PER1–PER4 readers unchanged | No learn provenance or settings-format revision |

The repository's production project magics ended at FUNB (`0x46554e42`). FUNC
was only a prospective frozen test fixture, not an existing production writer.
UP versions1–5 were the existing readers; 6/7 were unused. These collisions were
checked before activation. FUNC requires exactly118 parameters; historical
FUNB readers retain their prior map-by-count behavior. Future magic, truncated
or malformed objects fail closed under B1 ownership checks.

The FM6 packing remains112 bytes per track at3376, MIDI at3373, name at3824,
checksum at3836. Parameter count118, E0=110, four tracks, source IDs and all
other binary field offsets remain unchanged. No change to A/B commits, CRC,
flash geometry, user-sample pool, journal or allocator.

## Migration and initialization

`firmware/src/params.c::param_desc_of` is the canonical engine-aware resolver;
`track_desc` forwards109 to it. `core.h` supplies the stable alias, default and
schema validation helpers. Editor GET/SET/DESC and runtime loads already use
this resolution; no command numbers or reply layouts change.

`project.c::proj_import` is the authoritative import boundary. After existing
FM4/PHYS/PERC conversions, every pre-FUNC GRAIN track gets WIN64, irrespective
of its historical reserved byte; non-GRAIN gets0. The low-level historical
readers remain intact. Only FUNC serialized/runtime objects have explicit WIN
provenance. Their out-of-range values are rejected, not silently defaulted.
All production low-level `proj_unpack`/`proj_import_any` calls remain inside
this import boundary.

Old runtime/noinit FUNA objects are accepted only with their existing full-size
header, checksum, chain and motion validation, explicitly upgraded to the new
runtime schema, then given legacy WIN defaults. `project_restore_runtime`
uses the same import path for an old runtime schema. Retained serialized slots
and restore-last also pass through `proj_import`; they cannot bypass migration.
Migration may refresh RAM mirrors but does not program flash. Normal explicit
SAVE/STORE creates the new format. No saved data is automatically rewritten.

`ui.c::apply_preset_to`, `clean_engine_of`, `track_defaults` and the retired FM4
initialization path handle109 explicitly. Factory templates, fresh projects,
CLEAN/INIT, program-change factory loads and resets therefore get64 for GRAIN
and0 otherwise. Native preset load, project copy/restore and undo retain an
explicit0 correctly. No parameter values are rewritten merely because I1 boots.

`upreset.c::up_params` assigns legacy WIN after mapping versions1–5. New6/7
records require np118 and a valid engine-specific109. `up_grid` recognizes3,
5 and7. Category packing of an old record still promotes only to legacy4/5,
never to a WIN-aware version. Unsupported used records are retained inert;
direct overwrite/erase and whole-bank backup replacement are protected, also
in diagnostic multi-engine builds. Loading old records never changes flash.
Existing old migrations and pattern normalization are retained.

## Editor bulk ambiguity and intentionally blocked operations

Existing UP_PUT has no WIN provenance. Its unchanged118-value wire array is
parsed into legacy version4/5 and109 is forced to the engine's legacy default.
It cannot create a Gaussian value accidentally. A legacy replacement targeting
an existing valid6/7 GRAIN record with WIN other than64 is refused before RAM
or NOR mutation. Existing non-success result codes/framing are reused. Native
STORE, native LOAD/STORE copying, and versioned raw backups preserve explicit
non-Hann state.

The browser's legacy file/audition SET loops skip109 after their normal factory
initialization. Generic browser parameter rendering excludes WIN: I1 adds no
control. Metadata refresh includes the engine-aware109 descriptor. Known
non-Hann live/library export, upload and library audition are conservatively
refused by the existing error mechanism rather than emitting an unversioned
file that could silently lose WIN. An ambiguous old library with109=0 may also
be refused by this preflight; the firmware UP_PUT legacy import remains Hann.
A separately approved versioned library/protocol extension is needed for
faithful non-Hann JSON transfers. No capability tag or protocol extension was
invented here.

An individual scalar SET109 is an explicit headless parameter operation. The
existing protocol cannot identify whether a third-party client synthesized a
series of scalar SETs from an old file. Such clients must skip109 for legacy
bulk imports; the bundled browser does so. This is not a claim that arbitrary
external clients can infer WIN provenance under the old protocol.

## MIDI and automation quarantine

Historical learn109 bytes remain physically present in PER4 and its backups.
`midi_control.c::midi_learned` consumes but does not execute those mappings;
it does not fall through into another CC assignment. `midi_learn.c` refuses
new learn109 and ignores it as a UI learn target. `ml_cc_of` and editor learn
listing treat it as inactive. Explicit existing clear operations remain usable;
boot/import never delete the bytes. No settings revision has been introduced.

`motion_param(109)` remains false, no parameter lock or motion record is
created by editor SET109, and modulation destinations/fixed CCs are unchanged.
No GRAIN EDIT or HOME mapping exposes WIN.

## Frozen inputs and downgrade evidence

41 binary fixtures and their original SHA256 manifest are in
[`tests/fixtures/grain-win-i1r`](../tests/fixtures/grain-win-i1r/README.md).
They were frozen with the exact clean B1 converter **before editing converters**.
They cover FUN1–FUN11, old retained runtime objects and user versions1–5.
Historical109 values0,1,63,64,65,126,127 are present where that field exists.
The test additionally mutates the biased FUNB byte over its entire legal0..191
range, including negative decoded reserved values; every case imports Hann.

Valid FUN10 has at most114 parameters with its old FM6 tail. A supposed
118-parameter FUN10 containing109 overlaps existing fields and was already
invalid. It remains rejected; no malformed layout was legitimized to expand
fixture coverage. FUN1–9 did not contain common109 and import its new default.

`grain_win_persistence_test` emits actual FUNC and6/7 bank payloads. The new
`grain_win_downgrade_oracle.c` compiles against **exact B1 production sources**,
not the new reader. On seven actual WIN values across all four physical slots:

- B1 reports occupied/unsupported and refuses load, SAVE, SAVE AS, rename,
  erase and backup replacement/tombstone.
- Refetch and cache loss preserve the same read-only status.
- Restore-last/autosave and FM6 staging cannot overwrite unsupported autosave.
- B1 retains6/7 record bytes, refuses direct replacement and whole-bank restore.
- NOR erase/program counters stay unchanged, complete raw NOR and retained
  track/slot state remain identical on rejection.

**1,654 checks pass**. The original historical oracle against unpatched
`99325e7` still exits1 with12 setup checks and:

```
unknown slot readonly=0; load rc=1; SAVE rc=0; erase delta=1;
preserved=0; replacement magic=46554e42
```

This historical failure is evidence, not a passing assertion. Diagnostic
multi-engine B1 is not the supported downgrade baseline for unknown user
records; use the shipping GRAIN-only bridge. New I1 also protects unknown
records in diagnostic builds.

## Validation

Host GCC14, ASan+UBSan, `-ffp-contract=off`. New storage/editor/descriptor tests
use strict `-fsanitize=address,undefined -fno-sanitize-recover=all`, **no sanitizer
exclusions**. Fixture checksums pass and no unrelated goldens are regenerated.

| Focused gate | Result |
|---|---:|
| New persistence and physical NOR, shipping | 5,818 checks,0 failures |
| Same strict tests, diagnostic multi-engine | 5,818 checks,0 failures |
| Exact B1 downgrade oracle | 1,654 checks,0 failures |
| Editor GET/SET/DESC, four tracks, learn quarantine | 110 checks,0 failures |
| Browser provenance/headless/bulk checks | 11 checks,0 failures |
| Project all-field/bit-corruption/legacy format suite | 41,146 checks,0 failures |
| Project historical migrations | 61 passing assertions,0 failing |
| User record/parser suite | 45 passing assertions,0 failing |
| GRAIN-only selection/compatibility | 380 checks,0 failures |
| GRAIN-only editor/storage | 70 checks,0 failures |
| GRAIN-only physical persistence | 24 checks,0 failures |
| B1 ownership regression with future FUND | 2,189 checks,0 failures |
| Retained unknown-object protection | 62 checks,0 failures |
| STRETCH diagnostic descriptor/load regression | 1,764 checks,0 failures, same base total |
| Generic storage | 35 passing assertions,0 failing |

Coverage includes exact all128 WIN values on independently varied four tracks,
new format round trips, mixed engine/reserved behavior, all factory templates,
CLEAN, undo and native preset copies; valid/invalid record versions, malformed
np/values, CRC/length/future-magic protection, raw backup rejection atomicity,
PER4 NOR/backup raw-learn preservation, both note/grid records, old boot and
restore-last with zero flash writes. Existing project-format tests exercise all
118 parameters, GRAIN fields, motion, MIDI, all user source IDs and PERC recovery.

### Exact-base regression comparison

The broader inherited suites contain multi-engine assumptions invalid in the
shipping GRAIN-only configuration. Run base and new with identical host flags;
report actual failures rather than changing DSP or concealing them:

| Shipping suite | Exact B1 pass/fail | I1-R pass/fail |
|---|---:|---:|
| Motion |94/30|94/30|
| MIDI control |122/27|122/27|
| UI |503/94, then NULL failure at ui_test.c:3911|same counts and failure|
| Editor |88/9, then NULL params_by_count at core.h:110|same counts and failure|
| Persistence |45/31|45/31|
| Backup |77/26|78/25|
| Broad web |5 failures|same5 on repeat|

Backup's one improved assertion now requires atomic refusal of an incoming
future user-record bank, instead of demanding its acceptance. The new check
asserts unchanged complete NOR, erase count and RAM bank; it is a stronger
ownership test. Diagnostic backup remains **103/0** on both revisions. No
unknown-bank assertion was deleted to obtain a pass.

An initial concurrent broad-web run had one additional time-sensitive
`live: PING keeps WATCH on` failure; a repeat returned exactly the base's five
source-map/name failures. These are not classified as a deterministic WIN
regression. The STRETCH suite fails early in shipping mode on both revisions
because it requests other engines; its intended diagnostic build passes.

Broader historical host suites use their existing exclusions for signed
integer overflow, shifts, bounds, object-size and pointer-overflow. These do
not apply to the new strict persistence/editor tests. O0 host probes expose
unreachable pi32 `csync`/`ssync` assembly; use the optimized host harnesses,
not an O0 build, as in the existing test instructions.

Test-only expectation updates are limited to explicit FUNC output tags,
canonical WIN values when a fixture changes engines, reserving109's override
alongside108, legacy record versions4/5, and a handcrafted old89-parameter
motion fixture's explicit FUNA magic. Its former use of `PROJ_MAGIC` would
incorrectly label an old layout as FUNC. All unrelated assertions remain.

### Audio and visual identity

- Existing four-part final-output regression: **330 checks** on each revision;
  **57,344,000 bytes identical** by `cmp`, with explicit peak/RMS/nonzero tests.
- New four-simultaneous-track test: **28 sounding cases** on each revision,
  all four personalities and seven independently offset WIN values;
  **7,168,000 bytes identical** by `cmp`. Peak8497..18046,
  RMS2005.366529..4534.996244, nonzero56454..63972 of64000 samples per case.
  ASan+UBSan pass with only the inherited DSP signed-overflow/shift exclusions;
  bounds and other undefined checks remain enabled. The new persistence path
  itself has no exclusions. No production DSP source was edited.
- **225 complete retained RGB565 UI frames pixel-identical**. Captured navigation,
  globals, preferences and parameter state also match after masking only the
  four intended109 default changes0→64. No snapshots/goldens were regenerated.

## Reproduction and resources

With existing generated headers and pinned engine checkout already available:

```sh
WIN_B1_TREE=/absolute/clean/B1-worktree \
WIN_UNPATCHED_TREE=/absolute/clean/99325e7-worktree \
WIN_OUT=/tmp/coffee-grains-win-i1r tests/run_grain_win_i1r.sh
```

The runner verifies the exact B1 commit and frozen checksums, runs both strict
NOR modes, editor/preset tests, the actual-payload B1 oracle, optional unchanged
historical failing oracle, browser checks and final-output identity. The new
`grain_win_audio_test.c` accepts `WIN_AUDIO_UI_SOURCE` for exact-base comparison;
compile both revisions with identical ASan+UBSan flags and compare their PCM
files. Retained frame comparison uses the existing
`grain_only_navigation_test.c` with `GRID_UI_SOURCE`; each record contains
28 state bytes,115200 RGB565 bytes, four118-value arrays, then globals/preferences.
Mask only two bytes at109 in each track array for state comparison.

Same GCC `-Os -ffp-contract=off` retained-navigation harness:

| Host ELF section | B1 | I1-R | Delta |
|---|---:|---:|---:|
| text/rodata |841798|842822|+1024 bytes|
| data (includes pointer-relocated const descriptors) |31768|31800|+32 bytes|
| BSS |506376|506376|0|

The smaller final-output harness gives text+154,data+32,BSS0; harness reachability
changes native linked totals, so neither is an FM1 image measurement. The one
new descriptor is const. No new mutable firmware buffer or persistent state,
framebuffer, bitmap, heap allocation or audio-callback operation was added.

Host sizes remain: grain64, GRAIN part7296, track1840, runtime project4840,
serialized project3840, user record192, bank3080 bytes. GRAIN uses7296 of12888
engine bytes per part, leaving5592 as before. No structure fields were added.
No new large stack objects are introduced; migration uses existing scratch and
small scalar locals. Target ABI/stack and flash fit remain unmeasured.

No usable pi32v2 compiler was found on PATH; no toolchain was installed and no
target link, deployment or hardware flashing was attempted. Native host sizes
are not evidence of fitting the581564-byte FM1 image budget.

## Changed areas and next gates

Production changes: core/parameter descriptors; project/runtime schema and
migration; user-record versions and ambiguous replacement guards; initialization
WIN defaults; learn quarantine; browser legacy-bulk and export safeguards.
Tests add frozen inputs, physical-NOR and exact-B1 oracles, editor/browser gates,
a four-track sounding audio probe and a reproducible runner. Existing fixture
updates and this report complete the checkpoint. Samples, DSP, allocation and
all serialized field positions are untouched.

I1 is ready as the persistence baseline for a separately reviewed I2. Before
I2: retain exact-Hann default tests, implement a coherent per-track publication
and per-grain capture of WIN, and keep hybrid fidelity default-off until target
profiling. Before I3: approve a provenance-bearing editor/library extension,
settings learn migration and UI placement independently. Do not assume current
unversioned JSON can faithfully carry an explicit non-Hann WIN.
