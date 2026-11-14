# GRAIN STRETCH descriptor compatibility

Base: `69f750c54da0dd45320494c3973e6bbaf9a00ba5`.
The existing DSP uses P_GFILTER_RSVD1 (108). Its display descriptor allowed 0..127,
but the canonical storage/editor descriptor was the reserved TP slot (0..0),
so runtime loads clamped it to zero and editor/MIDI/motion validation rejected
nonzero values. The canonical engine-aware resolver now returns STRETCH, F_INT,
0..127, default 0 for GRAIN only. track_desc reuses that exact descriptor.
Every other engine retains TP[108], fixed at zero. The unrelated descriptor
pointers/fields, ID 109, P_COUNT=118 and P_E0=110 remain unchanged.

Project restore, preset load, editor DESC/GET/SET, per-track editor SET,
MIDI learn and motion already call the canonical resolver. No additional
runtime consumers needed alteration. HOME K3 already resolves ID 108 through
track_desc; its mapping and legacy HOME=LEVELS behavior were not changed.
There are no DSP, sample, UI layout, navigation or startup changes.

## Browser metadata and library compatibility

The editor previously reread only E0..E7 after engine/track changes. It now also
refreshes the common descriptor identified by STRETCH / G-F RSV1, so it cannot
retain GRAIN's editable range on a different engine, or a reserved range on GRAIN.
The existing descriptor-driven control builder is reused. No markup, CSS,
layout, input mapping or navigation was changed. Older layouts without this
common slot retain the original eight descriptor requests.

Library keys retain the historical G-F RSV1 key for ID 108 even when its UI/wire
label is STRETCH. The two labels are equivalent only at that stable ID when
comparing/remapping keys. This avoids creating a library migration solely from
an engine-dependent label. Tests exercise the actual libAdopt path and verify
no record writes, preservation of the nonzero value and an opaque future field.
Library format/version and all other keys are unchanged.

## Storage compatibility

No magic/version, record size, parameter count, field offset, raw encoding,
settings format or backup schema changed. FUN11 projects remain 3840 bytes;
valid 0..127 values fit the existing parameter byte, preset record and motion
int8 value. Missing fields in older records still default to zero. Existing
imports and unknown-record/unknown-field handling remain in place. Resolving a
descriptor is read-only; no save-on-boot or automatic saved-record rewrite was
added. Valid nonzero GRAIN values already present in slot 108 are now honored
at runtime instead of being discarded. Other engines keep reserved-slot behavior.

**Downgrade limitation:** older firmware may read the same binary format but
clamp GRAIN STRETCH to zero. Resaving on it can lose the nonzero setting; retain
an original backup when downgrading. This repair does not migrate or fix the
unrelated TONE, DRIFT, CUTOFF, POS or long-source DSP findings from the audit.

## Validation

ASan/UBSan used the established DSP exclusions documented in mixer-top-knobs.md;
no additional sanitizer exclusions or assertions were disabled.

| Suite | Feature result |
|---|---:|
| Focused canonical/runtime/preset/project/MIDI/motion/HOME test | 1764 passed |
| Actual editor wire DESC/GET/SET and per-track SET | 40 passed |
| Browser refresh, stable library keys, no automatic record rewrite | 27 passed |
| FUN11/FUN10 format, corruption and migration | 41146 passed |
| User-preset records and old parameter layouts | 45 passed |
| Backup, including runtime and preset-bank STRETCH restores | 103 passed |
| Existing editor regression | 172 passed |
| Full UI regression | 850 passed |

Values 0, 1, 64 and 127 each survive project capture/pack/import/runtime restore,
project save/load, user-preset save/load, backup restores, editor operations,
learned MIDI CCs and motion. Out-of-range motion values are rejected. Non-GRAIN
reserved slots remain non-editable and project loads still clamp them to zero.

The audio fixture renders the actual GUITAR factory source at MIDI 60 with
fixed, identical settings, no effect sends and only STRETCH varied. It does not
alter audio assets, gain-normalize, resample or change the algorithm. Each
500 ms render has meaningful nonzero output. Four distinct renders demonstrate
the existing effect, while all four match the untouched base byte for byte when
the same raw value is injected directly (bypassing the base's known descriptor
bug). All 4 x 176400 bytes match. Measured head advance over a short fixed window:

| STRETCH | Peak | RMS | Head advance Q16 |
|---|---:|---:|---:|
| 0 | 0.108063 | 0.048956 | 50135040 |
| 1 | 0.147842 | 0.033906 | 48176640 |
| 64 | 0.102432 | 0.053226 | 4896000 |
| 127 | 0.109558 | 0.062268 | 391680 |

The head slows monotonically; RMS is not expected to be monotonic for a
source-position effect. No pitch/length/scaling algorithm was changed.
STRETCH_BASE_TREE enables exact-base audio comparisons in tests/run_tests.sh.

## Existing broader-suite failures

These suites were run on both this branch and the exact untouched base under
the same host/sanitizer conditions (Node for the web suite). No new failing
assertion names were introduced; unrelated fixtures were not repaired here.

| Suite | Base pass/fail | Feature pass/fail |
|---|---:|---:|
| Standalone motion | 96 / 28 | 97 / 27 |
| Standalone MIDI/control/clock | 129 / 20 | 129 / 20 |
| Broad web | 282 / 5 | 282 / 5 |

Examples include old ANALOG/FM6/LOFI/DRUM power-on assumptions (current startup
is four GRAIN tracks), old menu navigation and P_COUNT=111/P_E0=103 assertions.
Web failures are the same four S3 source-ID checks and old PIANO/TRANH/PERC alias
expectation. The motion assertion that now passes is the step-1 held-note/lock
case, consistent with ID 108 becoming a valid motion parameter.
These pre-existing failures remain follow-up test-fixture work, not storage
compatibility blockers. Native checks do not establish target-link/hardware
readiness. No flashing, toolchain installation, merge or deployment performed.

## Changed files

- firmware/src/params.c — canonical GRAIN-only descriptor and shared UI resolution.
- web/editor.html — descriptor refresh and stable legacy library-key handling only.
- tests/grain_stretch_test.c — descriptor/runtime/load/MIDI/motion/HOME coverage.
- tests/grain_stretch_editor_test.c — actual protocol framing and metadata coverage.
- tests/grain_stretch_audio_check.py — distinct meaningful audio/head progression.
- web/emu/grain_stretch_audio_test.c — actual DSP rendering, optional exact-base include.
- web/test_stretch_descriptor.mjs — editor refresh and no-write library compatibility.
- tests/backup_test.c — runtime/preset-bank STRETCH backup round trips.
- tests/run_tests.sh — regression registration and optional STRETCH_BASE_TREE A/B.
- docs/grain-stretch-descriptor.md — compatibility, validation and baseline limits.

`git diff --check` and shell syntax validation passed. No DSP or format source
files were edited, and no golden files were updated.
