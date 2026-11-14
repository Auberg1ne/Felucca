# SAMPLE EDIT: one page, two selectable rows

Base: `68c3229b66d86a067d5cacd3b20214cbf6bf2424`.
Branch: `feat/coffee-grains-sample-edit-grid`.

| Row | K1 | K2 | K3 | K4 |
|---|---|---|---|---|
| SAMPLE | BITS 112 | LOOP 113 | CUT 114 | empty sentinel 255 |
| VOICE | VCE 37 | DTUNE 44 | SPRD 95 | empty sentinel 255 |
| Breathing room | empty | empty | empty | empty |

Only the first two rows are drawn/selectable. K4 has no cell, text or indicator;
the entire third-row area is black. No scientist or other decoration is added.
The renderer reuses the exact GRAIN cell geometry, white text, units, rounded
coffee outlines, channel accents, selected-row rim and value indicators.
SAMPLE uses the shared descriptor label SPRD; GRAIN retains both G-SPRD and
V-SPRD and its dynamic personality labels unchanged.

## Navigation and parameter resolution

SAMPLE uses the existing logical EDIT 1 page. No PAGES entry is removed,
renumbered or reordered. All other SAMPLE EDIT rows/pages are menu-invisible.
Entering the page or selecting another track resets the shared row cursor to
row 1. Clockwise and counterclockwise detents toggle 1↔2; multi-detent moves
apply modulo 2. An EDIT tap stays on the sole page and re-enters at row 1.
Existing EDIT hold/double-tap engine-layer gestures remain intact.

The SELECT override applies only to an unobstructed EDIT grid. It never writes
the saved BPM/PAGE/MODW/VEL preference. GLO+SELECT, BPM LOCK, layers, momentary
controls, held-step locks and layer quiet periods retain their priorities.
Leaving SAMPLE EDIT restores the saved SELECT behavior. GRAIN still has two
pages, three selectable rows and its previous entry/wrap policy.

`edit_grid_page()` selects an internal view key (GRAIN 1/2, SAMPLE 3), not a
stored page or parameter ID. `edit_grid_param()` is the authoritative mapping;
`page_param_id()` and `page_desc()` provide that same mapping to physical
edits, momentary edits, motion capture and MIDI learn. Locks resolve the same
remembered source page and shared row on SEQ. K4 returns 255 before a descriptor
or parameter pointer can be obtained. Existing automation eligibility is
unchanged: for example VCE is editable/learnable but is not made automatable.

`sample_grid_normalize()` runs before input and drawing, and on track changes.
It redirects stale hidden SAMPLE page indices and hidden EDIT lock sources to
the canonical page before a physical knob can edit them. It preserves ENV/FX
lock sources. Old remembered family pages are also rejected by the ordinary
visibility-aware page navigation. The existing layer `page_over` mechanism is
preserved.

There is **one shared row cursor and one shared render cache**, not separate
SAMPLE navigation state. The existing cache page byte now distinguishes all
three grid views, so engine changes cannot reuse stale cells. The historical
`grain_edit_row`/`grain_grid` private variable names become
`edit_grid_row`/`edit_grid`; GRAIN test changes retain their assertions and
expected mappings, changing only those private names.

## Hidden values and compatibility

SET 110, TUNE 111, DRV 116, GLD 38, GLMOD 41, PRIO 42, ALLOC 43,
PAN 39 and MUTE 40 are absent from SAMPLE EDIT only. Reserved 115/117 never
become grid targets. No parameter values/defaults, canonical descriptors,
engine metadata, parameter count/order, source mapping, saved format or DSP
is changed. P_COUNT remains 118 and the project allocation remains 3840 bytes.

All hidden controls retain their supported editor and MIDI access; supported
motion targets remain supported. Source selection is also retained through
HOME SOUND's K1, preset browsing and external access (legacy HOME LEVELS still
has its original physical mappings). Master tuning, ADSR via ENV, user slots,
zone metadata, journal/allocator and legacy imports are untouched. Sound preset
loads still preserve destination PAN/MUTE. The Stage 1 STRETCH repair is intact.

The footer directly calls the existing HOME coffee-bean renderer. Notes/drum
hits are beans; ties/rests/empty steps are underscores. The real selected track,
SONG source, bank, pattern length and sequencer index drive the strip and its
independent playhead underline. HELP and MIDI-learn hints keep their priority.
No timing source, simulated step or change to HOME rendering is introduced.

## Validation against the exact base

| Suite | Final result |
|---|---:|
| New SAMPLE mapping/input/rows/empty cells/legacy pages/locks/motion/learn/hidden values | 1158 passed |
| New SAMPLE editor wire descriptors, GET/SET and per-track access | 120 passed |
| SAMPLE pixel bounds/contrast, full ranges, all palettes/styles/channels, blank K4/third row and HOME footer agreement | 1167 passed |
| Full UI, including 150000-frame navigation/motion/clock-wrap soak | 850 passed |
| GRAIN grid navigation and mappings | 2329 passed |
| GRAIN full-range/palette/style/dynamic-label rendering | 1935 passed |
| STRETCH canonical descriptors/runtime/preset/project/MIDI/motion/HOME | 1764 passed |
| Existing editor / presets / backup | 172 / 45 / 103 passed |
| Project format/corruption FUN11 + FUN10 | 41146 passed |
| Historical project formats / FUN10 UI and imports | 61 / 51 passed |
| Factory samples / eight-slot compatibility | 232 / 21 passed |
| Source order/aliases/encoders/locks/motion/presets/projects | 802 passed |

All named C suites above ran under ASan/UBSan. No sanitizer findings, with the
established DSP overflow/shift and host XIP-rebase exclusions, and no new
exclusions. The new visual suite reuses the existing GRAIN glyph/pixel/bounds
probe; GRAIN's normal main/scenarios remain enabled and pass independently.

The original source-order fixture failed at its assumption that SAMPLE EDIT
K1 edits SET. This is the approved behavior change: it now edits BITS. Its
SAMPLE source-selection portion now uses the unchanged HOME SOUND interface;
GRAIN continues using EDIT row 1. Every assertion is retained. The updated
fixture passes **802 checks on both the exact base and this tree**, with
identical logs. This was not classified as a pre-existing production failure.

Remaining baseline failures were freshly reproduced on both trees:

| Suite | Exact base | New tree |
|---|---:|---:|
| Motion | 97 passed / 27 failed | 97 / 27 |
| MIDI/control/clock | 134 passed / 15 failed | 134 / 15 |

Failure names match exactly; no new failures remain in these comparisons.
The full repository runner was not run to completion; the listed suites were
compiled and run directly, and their relevant new registrations were added to
`tests/run_tests.sh` (including sanitizer registrations).

**3947 non-SAMPLE-EDIT state/pixel snapshots are byte-identical** to the base,
including every GRAIN row, both GRAIN pages, HOME/MIXER/CLOCK/settings and
other engine pages with knob edits. The musical parameters, globals and saved
SELECT preference are included in the comparisons.

**Eleven real audio cases are byte-identical** with EDIT visible on both trees:
all five SAMPLE factory sources across POLY/MONO/UNISON examples, two GRAIN
cases (GUITAR and SYNTH), plus four seven-second loop/end cases. Short renders
are 176400 bytes each; long renders are 2469600 bytes each. All are meaningful
nonzero audio. The long cases verify CELLO sustains with LOOP on, ends with
LOOP off, and FLUTE/SYNTH end despite LOOP on because their metadata is not
looped. The same eleven cases pass ASan/UBSan. No DSP code was modified.

## Resources and build status

One new mapping table: **8 read-only bytes**. The existing 52-byte render cache
and one-byte row cursor are reused without growth. No new bitmap, framebuffer,
heap allocation, timer or audio-callback work. The six cells use the existing
57×50 canvas; the navigation hint uses the existing 240×12 canvas. Quiet
frames issue no writes; a single value change blits one 2850-pixel cell.

Matched x86-64 `cc -Os -ffp-contract=off` native browser-backend ELF sizes:

| Allocation | Base | New | Delta |
|---|---:|---:|---:|
| text + read-only data | 937903 | 938703 | +800 |
| initialized data | 31872 | 31872 | 0 |
| BSS | 1603360 | 1603360 | 0 |

With `COFFEE_HOME_VISUALS=0`: text/read-only 898287→899055 (**+768 bytes**),
initialized data 31840→31840, BSS 1603168→1603168. These are matched native host
proxies including the test harness, not target firmware sizes. Static RAM
increase is **0 bytes**; target stack/headroom and linked flash fit are not
measured. Rough planning cost is under 1 KiB in these host configurations;
pi32v2 code size can differ. Documentation PNGs are never linked into firmware.

Unchanged build attempts stopped before compilation:

- `emu: no emcc (Emscripten)`
- `build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)`

No toolchain was installed, hardware flashed, merge or deployment performed.
The 581564-byte target image limit remains unverified without a successful link.

## Review images and reproduction

Native 240×240: [row 1](sample-edit-grid/row-1.png),
[row 2](sample-edit-grid/row-2.png). Before/after, base left and new right:
[row 1](sample-edit-grid/compare-1.png), [row 2](sample-edit-grid/compare-2.png).
The base examples show SET and VOICE; the new examples show both selections
of the single page. All use the same parameter values and stored mixed step
fixture, with the real sequencer's resulting index.

```sh
cc -Os -w -ffp-contract=off -Ibuild/gen -Ifirmware/src \
  tests/sample_grid_preview.c -lm -o /tmp/sample-grid-preview
mkdir -p /tmp/sample-grid-images
/tmp/sample-grid-preview /tmp/sample-grid-images
```

For the old UI, compile the same driver with `-DSAMPLE_GRID_BASE` and
`-DGRID_UI_SOURCE='"/absolute/base/tests/ui_test.c"'`. PPM exports contain the
actual RGB565 panel colors expanded to RGB888. Convert losslessly to PNG;
comparison images concatenate at native scale. With existing generated build
prerequisites, `SAMPLE_GRID_BASE_TREE=/absolute/base tests/run_tests.sh` enables
the non-SAMPLE snapshot and eleven-case audio comparisons. Target build
prerequisites remain separate.

Changed production files: `firmware/src/{params,ui,ui_input,ui_draw,ui_graph,
ui_help,grain_edit_grid}.c`. Tests: `tests/{sample_grid_test,sample_grid_editor_test,
sample_grid_navigation_test,sample_grid_preview,grain_grid_test,sample_order_test,
ui_test}.c`, `tests/run_tests.sh`, and `web/emu/{sample_grid_audio_test,
sample_grid_visuals_test,grain_grid_visuals_test}.c`. Documentation: this report
and four PNG review images. No sample/DSP/storage/serialization source or
production image asset changed.
