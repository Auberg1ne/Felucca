# Two-page GRAIN EDIT grid

Base: `7d602e625ea05e9594bb0bfbb9cab22cb9c4d5f1` (Stage 1).
Only the GRAIN EDIT presentation and its page-relative routing change.

| Page | Row | K1 | K2 | K3 | K4 |
|---|---|---|---|---|---|
| GRAINS | 1 | SRC 110 | POS 111 | SIZE 112 | DENS 113 |
| GRAINS | 2 | PTCH 114 | G-SPRD 115 | RAND 116 | TONE 117 |
| GRAINS | 3 | STRETCH 108 | — | — | — |
| CHAR/VOICE | 1 | TYPE 103 | personality A 104 | personality B 105 | — |
| CHAR/VOICE | 2 | CUTOFF 106 | RESON 107 | — | — |
| CHAR/VOICE | 3 | VCE 37 | DTUNE 44 | V-SPRD 95 | — |

CLOUD uses DRIFT/WIDTH, FREEZE HOLD/MOTION, STUTTER RATE/GATE and
SHIMMER INTERVAL/HARMONICS. HARMONICS is displayed on two lines (HARMO / NICS)
to preserve the entire label at native resolution. UI aliases G-SPRD, V-SPRD
and RESON do not rename descriptors or editor/library keys. All live values
use the existing track descriptors and formatting, including source browsing.

## Interaction and routing

Bare SELECT moves one row per detent, wrapping 1→2→3→1 and 1→3→2→1.
Entering either page or selecting another track while on the grid starts at
row 1. A normal EDIT tap cycles the two pages and resets the row to 1. The
existing EDIT hold/double-tap engine layer remains available; successive page
taps must be spaced outside the existing double-tap window. Opening/closing
an overlay keeps the row. The persistent bright border surrounds all four
cells of the selected row. Empty cells show `--` and cannot edit, learn or lock.

Only bare SELECT on an unobstructed GRAIN EDIT grid overrides the saved
SELECT preference. No preference is written or reset. GLO+SELECT retains tempo
editing and the existing BPM LOCK exception; layers, momentary controls,
count-in, locks and input quiet periods keep their existing priority. Leaving
the grid restores BPM/PAGE/MODW/VEL behavior automatically.

`GRAIN_GRID` and `grain_grid_param()` are the authoritative page/row/column
mapping. `page_param_id()` resolves the active row; `page_desc()` feeds normal
knob edits, MIDI learn and motion capture, as well as momentary edits. Locks
use the same resolver. Leaving for SEQ retains the last grid row with the
existing lock-source page, so its K1–K4 lock the parameters last displayed.
Entering another sound page changes that source as before. Existing automation
eligibility stays unchanged: VCE/voice allocation is not made automatable.

Remaining raw page IDs identify page families/actions and non-GRAIN graph
layouts (envelope/FM/SLICE), not GRAIN grid knob targets. The renderer bypasses
scope drawing only for GRAIN EDIT. Audio scope capture and other engine graphs
remain intact. Existing status header and sequencer footer paths are reused.

## Compatibility and limits

GLD, GLMOD, PRIO, ALLOC, PAN and MUTE are absent from this GRAIN EDIT cycle;
their values, descriptors, editor access and consumers remain. No IDs, descriptor
order, parameter count, preset/project/backup layouts, source IDs, engine metadata,
HOME mappings or DSP changed. Stage 1 STRETCH resolution is untouched. Old
firmware may still clamp nonzero STRETCH on downgrade, as documented in Stage 1.
Existing DSP limitations involving TONE, CLOUD DRIFT, cutoff and long-source
playback are outside this UI change.

New persistent UI RAM: **53 bytes before linker alignment** (one-byte row and
52-byte render cache). No heap allocations, framebuffer or production bitmap
assets. Mapping/palette constants occupy 24/8 bytes. Each cell uses the existing
canvas (57×50); the footer hint uses 240×12. At most 12 cells are redrawn per
pass. A single ordinary value change blits exactly one 2,850-pixel cell;
eight unchanged UI frames produce no display writes. Full entry/row changes
also clear the 240×178 body. This is bounded work, without a new timer or
callback; hardware frame timing has not been measured.

Matched native x86-64 `cc -Os -ffp-contract=off` browser-backend ELF comparison:

| ELF allocation | Stage 1 | Grid | Delta |
|---|---:|---:|---:|
| text + read-only data | 935279 | 937471 | +2192 |
| initialized data | 31872 | 31872 | 0 |
| BSS | 1603264 | 1603328 | +64 |

The ~2.2 KiB native code/read-only delta is an estimate only, **not a pi32v2
flash measurement**. Hardware fit against 581,564 bytes and linked RAM headroom
remain unverified. Unchanged builds stopped before compilation:
`emu: no emcc (Emscripten)` and
`build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)`.
No toolchain was installed. Browser-backend rendering/audio was tested natively;
there was no WASM or interactive browser run.

## Validation

ASan/UBSan flags follow the established host suite: `-O1 -g -ffp-contract=off
-fsanitize=address,undefined -fno-sanitize=signed-integer-overflow,shift,bounds,
object-size,pointer-overflow -fno-sanitize-recover=undefined`. The exclusions are
the pre-existing intentional DSP wraps and host XIP rebase behavior; no new
exclusions or disabled assertions were introduced.

| Suite | Result |
|---|---:|
| GRAIN grid routing, controls, locks, motion, learn, hidden values | 2329 passed |
| Native grid labels, full value sweep, caching, ink/canvas bounds | 1747 passed |
| Full UI / FM4 UI | 850 / 840 passed |
| Editor / FUN10 / presets / robustness | 172 / 51 / 45 / 13 passed |
| Backup / project-format compatibility | 103 / 41146 passed |
| Factory samples / Stage 1 STRETCH / wire metadata | 232 / 1764 / 40 passed |
| Browser STRETCH metadata/library test | 27 passed |
| Non-grid navigation/state/pixel comparison | 3936 byte-identical snapshots |

The C suites above used ASan/UBSan except the native non-grid snapshot comparison.
No sanitizer findings. Full UI includes its 150,000-frame random navigation,
transport, motion and clock-wrap soak. The grid clears obsolete card-roll state
on full redraw so rolls from the previous page cannot linger unseen.

Four real GRAIN audio renders (STRETCH 0/1/64/127, 22,050 stereo float frames each)
are meaningful, distinct, and byte-identical to the exact Stage 1 backend with
unchanged parameters. Both backends draw their EDIT screen during playback.
Source-head Q16 deltas remain 50135040, 48176640, 4896000, 391680.
The same four values also render under ASan/UBSan.

Baseline comparison (no assertions removed or relaxed):

| Suite | Exact Stage 1 | New grid |
|---|---:|---:|
| Motion, unchanged fixture | 97 passed / 27 failed | 97 passed / 27 failed |
| MIDI, original fixture | 129 / 20 | 128 / 21 |
| MIDI, explicit ANALOG learn fixture on both trees | 134 / 15 | 134 / 15 |
| Broad web | 282 / 5 | 282 / 5 |

The additional original MIDI-fixture failure assumed EDIT 2 meant ANALOG
CUT/RES while the track was GRAIN. Its learn-test setup now explicitly selects
ANALOG; every original assertion remains. Both trees have identical remaining
15 MIDI failures. Motion and web failure names are identical to Stage 1; web
still has four stale S3 source-ID expectations and one old factory-alias check.
No new remaining failures were found in these comparisons. The whole host
runner was not run to completion; the named suites were compiled/run directly.

The full UI grayscale contract now explicitly verifies that GRAIN's approved
fixed-color skin is colorful, just as SETTINGS already was. All other themed
screens retain their grayscale assertions. The obsolete PERS help fixture now
checks SELECT ROW / K1–4 EDIT / EDIT PAGE on the accessible grid page.

## Reproduction and previews

After the existing generated prerequisites are available, the new suites are
registered in `tests/run_tests.sh`. Set `GRAIN_GRID_BASE_TREE` to an untouched
Stage 1 tree to enable the non-grid state/pixel comparison, and
`STRETCH_BASE_TREE` to enable the audio comparison already provided by Stage 1.
For focused rendering:

```sh
cc -O1 -w -ffp-contract=off -Ibuild/gen -Ifirmware/src \
  web/emu/grain_grid_visuals_test.c -lm -o /tmp/grain-grid-visuals
/tmp/grain-grid-visuals /tmp
python3 - <<'PY'
from PIL import Image
for n in (1, 2):
    Image.open(f'/tmp/grain-{n}.ppm').save(f'/tmp/grain-{n}.png')
PY
```

Native 240×240 RGB565 render previews: [GRAINS](grain-edit-grid/page-1.png),
[CHAR/VOICE](grain-edit-grid/page-2.png). These PNGs are documentation only and
are not linked into firmware.

Files: `firmware/src/{params,ui,ui_input,ui_graph,ui_help,ui_draw,grain_edit_grid}.c`;
`tests/{grain_grid_test,grain_grid_navigation_test,ui_test,midi_control_test}.c`;
`tests/run_tests.sh`; `web/emu/{grain_grid_visuals_test,grain_stretch_audio_test}.c`;
this document and its two PNGs.
