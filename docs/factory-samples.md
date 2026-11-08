# Coffee Grains factory sample pipeline (S8 preparation)

Production audio is unchanged. Five numeric factory identities already exist,
but only three independent materials are bundled. This document and the new
fixtures do not approve a replacement bank, migration or hardware deployment.

## Current identity and inputs

| SAMPLE SET / GRAIN SRC | Material | Storage |
| --- | --- | --- |
| 0 | GUITAR | Own zone/audio |
| 1 | GUITAR | Alias of 0 |
| 2 | VOICE | Own looped zone/audio |
| 3 | HIT | Own zone/audio |
| 4 | GUITAR | Alias of 0; historical SAMPLE PERC rules also exist |
| 5–12 | USR1–USR8 | Journal-backed dynamic user extents |

`tools/gen_samples.py:CC0_SETS` fixes the five factory rows. `alias_set()` shares
zones and bytes; `SMP_SET_ORIG={0,0,2,3,0}` controls alias handling in parameters
and preset browsing. Missing assets emit in-bounds zero-length zones rather
than removing rows. SAMPLE/GRAIN fall back to a sine for unavailable material.
The user offset must always remain five, even in partial/assetless builds.

The three current inputs under `assets/samples-cc0/{GUITAR,VOICE,HIT}` are
byte-identical copies of `assets/custom-samples/{GUITAR,VOICE,HIT}.wav`:

| File | Source duration | Source format | SHA-256 |
| --- | ---: | --- | --- |
| GUITAR.wav | 1.656939 s | 44,100 Hz, mono PCM16 | `9907ef1bfc03b4fe9b2acb941c1c06c5a68b70255f1aa83917cfded97089cb62` |
| VOICE.wav | 3.5 s | 44,100 Hz, mono PCM16 | `93ba5ce8c749491eb3fbb2a4cf4c03d96c34c5f3fbfd4ccfaf65095bc79c1335` |
| HIT.wav | 1.155079 s | 44,100 Hz, mono PCM16 | `8bcf3632acb59fa20b7adfcc5fc89590e43e6ab801cc1380bdafa0abe868a96e` |

The existing attribution files identify the older Versilian PIANO/FLUTE/SAX
recordings as CC0. They do not provide file-specific provenance/license for
these three custom-named inputs. Directory name alone is not licensing proof;
record their provenance before a release. No license or asset is changed here.
FLUTE/SAX remain available on disk but are not in the production source table.
Some historical WAVs are deliberately truncated; `read_any_wav()` uses the
available complete frames rather than trusting the declared data length.

## Generation and encoding

1. `tools/build.py:generate()` invokes `gen_samples.py`; the latter generates
   its drum WAV dependencies through `gen_waves.py` and caches by script/helper
   contents, Python version, SLICE option and input WAV contents.
2. `sampleio.read_any_wav()` reads PCM8/16/24/32 or float and mixes channels to
   mono. This path does not import WAV `smpl` loop metadata, despite the broad
   introductory generator wording. Filename notes and pitch detection choose
   roots/octave convention.
3. `cc0_entries()` trims onset (retaining up to16 input frames), resamples to
   **22,050 Hz**, caps one-shots at1s, sustains at0.95s, kits at0.6s. PIANO's
   historical export has its separate0.75s cap. One-shots get an80ms fade;
   sustains use a0.4s loop start and60ms crossfade, inclusive end at n−1.
4. Peak-normalized PCM targets30000. Roots retain1/16-semitone precision;
   sorted zones split the keyboard at root midpoints.
5. `Builder.add()` emits raw4-bit IMA ADPCM, low nibble first, initial
   predictor/index0/0. There are no WAV-style ADPCM block headers. Each zone's
   predictor/index immediately before loop start is retained. Each encoded
   region is padded to an even byte boundary.
6. `header()` emits `SMP_DATA`,28-byte zone records, fixed five-row set/name/
   preset tables and alias map. The SLICE BREAK is appended after factory data;
   its128-point seek grid and AUTO states are generated. Historical SLICE
   source4 PIANO shares the GUITAR0 middle-C data; it is not SAMPLE source4.

`firmware/src/eng_sample.c` includes the header through the real firmware TU.
Constants enter XIP `.text/.rodata` via `firmware/app.ld`; factory audio does
not consume the240KiB user pool. SAMPLE and GRAIN decode the same bytes in
place. GRAIN builds RAM seek indexes, not full decoded PCM copies. Simulator
and firmware compile the same generated header and engines; no JavaScript
mock is substituted by the new tests. Real editor descriptors are dynamic;
mock labels in `web/editor.html`/`web/test_web.mjs` remain current-bank labels.

## Size measurements and constraints

Rebuilding the current header in a temporary directory is byte-identical:
SHA-256 `775d9eb020836f8524a182475a5ca1641b49b65ac1d8ad13705470a34f027e90`.

| Temporary bank | Factory ADPCM | With unchanged BREAK | Zones |
| --- | ---: | ---: | ---: |
| Production: three materials/two aliases | 32,526 B | 54,576 B | 3 |
| Example: one FLUTE and one SAX sustain zone replacing aliases | 53,474 B | 75,524 B | 5 |
| Example: all three existing FLUTE and SAX sustain zones | 95,370 B | 117,420 B | 9 |

Examples use existing input files solely for measurement, not selected sounds.
One-zone example takes each candidate library's middle file. Added audio is
**20,948 B** or **62,844 B**, respectively. Zone metadata grows56B or168B;
set count stays five, but names/preset metadata/alignment add further bytes.
SLICE's shared GUITAR source adds no audio copy. BREAK remains22,050B.

For N output frames, encoded aligned bytes are `2 * ceil(N/4)` per zone.
Two mono22,050Hz one-second one-shots cost22,052B; two0.5s zones cost11,028B;
two0.75s zones cost16,540B. Neither sample duration nor extra flash can be
approved solely from these estimates. S7 has no target ELF: the581,564B app
limit includes all code/other constants, and its actual free margin is unknown.

Keep GRAIN's32-zone and1,024-seek-entry limits: each zone consumes
`ceil(n/256)` entries. The aggregate source budget is therefore at most262,144
frames (slightly less useful space because each zone rounds separately), about
11.89s at22,050Hz. Excess indexed zones become silent. The current one/three-zone
candidates are well below it. Replacing aliases alone adds no per-part grain
pool. Zone offsets/counts/root/loop fields must also remain within their actual
C field ranges; do not raise sample rates or durations by assumption.

Future asset acceptance must reject too-short/silent/nonfinite inputs, bad
roots/key ranges, invalid loops and oversized seek tables before emission.
The existing fixed crossfade/fade indexing assumes sufficiently long inputs;
short WAVs can fail generation. Factory headers do not pass through the
upload END validator. Test fixture validation is not a new production validator.

## Compatibility: stable numbers do not mean stable sound

- `params.c:param_fit()` retains SAMPLE/GRAIN alias numbers in stored projects,
  presets and motion. Simply changing bytes at1/4 retimbres those records.
- `project.c:proj_import()` applies PERC-to-DRUM migration to pre-FUNB SAMPLE4,
  but FUNB SAMPLE4 remains the GUITAR alias. GRAIN4 is not PERC-migrated. Both
  FUNB and older GRAIN records can therefore require old-bank interpretation.
- `upreset.c:up_migrate()` still treats SAMPLE4 as historical PERC.
- `ui.c:apply_preset_to()` treats SAMPLE preset4 as DRUM; `settings_persist.c`
  migrates its factory favourite to DRUM. `SMP_NPRESETS` is still4, so a new
  fifth waveform would not become a fifth normally browsable factory preset
  merely by generating bytes. Preset1 is currently skipped as an alias.
- Motion locks, retained project caches, autosave, user presets, favourites and
  backup restore all need consideration. Backups do not embed factory audio.
  Never globally remap the number4: SLICE source4 and chain slot4 have other
  meanings. Engine-aware parameter-ID migration must follow legacy ID mapping.
- A future bank retains0 GUITAR,2 VOICE,3 HIT and all user IDs5–12; slots1/4
  become distinct only under an explicitly approved bank-identity policy.
  Regenerated byte offsets are internal and must not become saved identities.

Options:

| Policy | Consequence |
| --- | --- |
| Keep current bank | Exact current timbres; only three unique sounds |
| Explicit alternate bank with retimbre consent | Stable numbers, intentionally changed sound; not seamless compatibility |
| Versioned bank identity in all persistent selection contexts | Can preserve old aliases as0 and retain historical PERC migration while exposing new1/4; needs separately approved schema/migration work |

Recommended policy is version-aware bank identity, not silent replacement or
inference from labels. FUNB alone cannot distinguish a current alias-bank save
from a future bank using the same format. No migration or format change is
implemented in S8. Approval must cover user-preset and favourite semantics too,
not only project magic. Until that decision and a genuine target size check,
keep production audio and the old bank as default.

## Test-only preparation

`tests/factory_pipeline_test.py` builds private synthetic WAV banks through the
actual `cc0_entries`/Builder path, compiles `factory_pipeline_test.c` against
those temporary headers and the real simulator/firmware TU, and checks:

- five independent encoded/decoded sources; absent IDs remain reserved;
- complete SAMPLE vs GRAIN decoding against a Python reference, loop-state
  restart, finite/nonzero rendered output (including missing-source fallback);
- selector boundaries4/5/12/13 and all13 FUNB selections;
- eight relocated v2 user samples and unchanged NOR;
- existing preset4/legacy4 limitations, rather than pretending they are fixed.

Run from the checkout with existing build/gen/drum dependencies; all outputs
must be outside the checkout and each profile must be fresh:

```sh
python3 tests/factory_pipeline_test.py --out /tmp/cg-factory-test-unique
python3 tests/factory_pipeline_test.py --sanitize --out /tmp/cg-factory-san-unique
```

Three profiles (five present, missing1/4, all absent) each run43 native
checks with SLICE enabled, including playback of all eight user slots through
SAMPLE/GRAIN/SLICE. Both normal BREAK and `--without-break` configurations pass.
`--slice-off` tests the actual `FELUCCA_SLICE=0` configuration (35 checks/profile).

```sh
python3 tests/factory_pipeline_test.py --without-break --out /tmp/cg-no-break-unique
python3 tests/factory_pipeline_test.py --slice-off --out /tmp/cg-slice-off-unique
python3 tests/factory_pipeline_test.py --without-break --strict-decode --bad-descriptors --out /tmp/cg-strict-unique
```

Strict decode mode runs21 checks/profile with ASan/UBSan and no exclusions:
address boundaries, malformed factory tables/zones (including a later bad zone
and an offset impersonating a user slot), full decoder CRCs, loop state and
all eight recovered user sources. It intentionally omits DSP rendering, whose
older signed arithmetic requires separate exclusions. Full render sanitizer
mode excludes only signed-integer overflow/shift, retaining bounds,
object-size and pointer-overflow checks. Synthetic fixtures are test material,
not new production assets or perceptual proof of five good sounds.

## S8A: assetless addressing repair

The original no-BREAK/all-absent reproducer had valid v2 journal mappings and
nonzero raw NOR audio in every slot. But `SMP_DATA` was a one-byte const zero
array, and GCC14.2 compiled the old `SMP_DATA[offset]` probe to unconditional
zero. Decoders therefore received zero nibbles: this was neither missing
metadata nor a sine-fallback selection. User offsets intentionally identified
bytes outside the factory C array, which made that expression undefined.

`smp_data_byte()` now reads factory offsets only inside the factory array and
translates other flat32-bit offsets back into the bounded actual NOR pool.
Isolated host fixtures use their own bounded slot objects. No assumption that
user audio is adjacent to or above the factory object is needed for reads.
The existing transient offset representation, FSMP/SMA/journal formats,
publication order and active-reader retention are unchanged. SAMPLE, GRAIN,
SLICE and waveform previews all use the same accessor; none changes IMA math.
Invalid factory descriptors are rejected before source-table indexing and
fall back safely; their offsets cannot authorize reads from a user extent.

The one-byte sentinel stays one byte: no dummy audio or padding workaround was
introduced. The old no-BREAK diagnostic now passes, including every user slot.
An actual SLICE-disabled build is also covered. Populated-build heavy-song
output remains bit-identical for88,200 stereo frames; all nine SLICE demo WAVs
match the before-repair build. Real FM-1 compilation/timing and physical audio
remain unverified; S7A's toolchain/SDK prerequisites are still outstanding.
