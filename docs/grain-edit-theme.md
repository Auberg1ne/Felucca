# GRAIN EDIT theme and shared coffee-bean footer

Base: `aec25f64e9c19aa2d3f1a89fd81034ae7f5848cf`.
Branch: `feat/coffee-grains-grain-edit-theme`.

Only `firmware/src/grain_edit_grid.c` and `firmware/src/ui_draw.c` change in
production. Grid structure, mapping, descriptors, formatting, input handlers,
SELECT preference/row policy, EDIT gestures, locks/motion/learn routing,
STRETCH, storage formats and DSP are unchanged.

## Visual decisions

The source references are `home_scientists.c`, `mixer_visuals.c`,
`clock_visuals.c` and `settings_visuals.c`. Their channel accents are RGB565
cyan `07ff`, yellow `ffe0`, magenta `f81f`, green `07e0`; SETTINGS adds purple
for its SYSTEM tab. GRAIN retains the shared dark surfaces `0841` / `18c4`,
white live text and muted `bdf7` units. It adds larger rounded corners,
a two-pixel selected-row rim, narrow channel-colored label separators and
warm coffee outlines/gauge tracks. The brown `4964` is the existing HOME
scientist palette entry 189 converted from panel byte order. A test verifies
that identity. It is an immediate color constant, not a dependency on sprite
image data.

White text is deliberate: the existing legacy GREY/MONO font ramp blends by
red channel, which would make cyan labels nearly black. Color stays in
procedural shapes, as on the other colorful screens. No global palette state
is overridden by the renderer. Negative radii use the existing primitive's
rounded-in-every-style convention, so an old saved LINE preference cannot
square off GRAIN's new cells; other pages still honor that preference.

No scientist motif is added: the grid, units, row hint and footer leave no
clear space without competing with parameter readability. Both spread labels,
all personality labels (including the existing two-line HARMONICS), fonts,
values and units remain unchanged.

## Sequencer semantics

The only footer routing change adds `grain_grid_active()` to the existing
HOME/MIXER/CLOCK coffee-bean branch in `draw_foot()`. It calls the **same**
`draw_home_step_icons()` and `home_step_bean()` functions, with no copied
symbol or state logic and no changes to HOME's output.

- A real pitched note (including MIDI note 0) or drum hit is a bean.
- Empty, tie and explicit rest steps are underscores, exactly as on HOME.
  Rest steps with stale note/hit payload do not become beans.
- Accents retain HOME's small highlight.
- The actual selected track, `seq_steps()` SONG source, visible bank and
  clamped pattern length provide the data.
- `song.playing` and the track's actual `seq_idx` provide the independent
  one-pixel playhead underline. No note/activity inference, new timer or
  internal-clock assumption is introduced.
- Invalid/out-of-range banks are guarded by the existing shared renderer.
- HELP, MIDI LEARN and action hints retain their existing footer priority.

The existing `home_step_accent()` behavior is preserved too: with colorful
HOME compiled in, it uses channel colors; without it, it uses `T_THEME`.
GRAIN cells always retain their four channel accents. This does not enable
scientist bitmap assets in hardware configurations that omit them.

## Validation

All named C suites below use ASan/UBSan with the existing host exclusions for
intentional DSP signed overflow/shifts and XIP pointer rebasing (`bounds`,
`object-size`, `pointer-overflow`). No new sanitizer exclusions or disabled
assertions were added.

| Check | Result |
|---|---:|
| HOME/GRAIN footer comparison, both pages, four tracks, stopped/playing and all 16 playheads; real transport/reset, SONG source, bounds | 8229 passed |
| Grid full value sweep, personality labels, borders, all built-in palettes/styles/channels, actual text-pixel contrast and canvas/ink bounds | 1935 passed |
| Existing grid navigation, every populated knob cell, inert empty cells, GLO/modifiers, locks, motion, learn, hidden values | 2329 passed |
| Full UI including 150,000-frame navigation/motion/clock-wrap soak | 850 passed |
| Existing HOME step symbols and unchanged surrounding pixels | 235 passed |
| STRETCH descriptors/runtime loads/project/preset/MIDI/motion/HOME K3 | 1764 passed |
| STRETCH editor wire metadata/GET/SET | 40 passed |

No sanitizer findings in these runs. The full host runner and its unrelated
known-failing motion/MIDI/web suites were not rerun; no claim of an entirely
green repository-wide baseline is made.

A native base/new comparison of **3936 non-grid state/pixel snapshots** is
byte-identical against the exact base, covering HOME, MIXER, CLOCK, GLOBAL
SETTINGS, non-GRAIN EDIT and other visible engine pages with knob edits.
Four actual GRAIN audio renders, STRETCH 0/1/64/127, are meaningful/distinct and
**byte-identical** to the base (176400 bytes each). EDIT is visible during
these audio renders. The source-head deltas also match the base. No audio
source was modified.

The full-range visual test now samples actual rendered glyph-box pixels,
not only text bounding boxes. This caught/prevents dark cyan text under the
legacy grayscale ramps. The former border check moves from a newly rounded
corner to the straight edge; it additionally checks the two-pixel selected
rim, warm unselected rim and round corners under both saved styles.

## Memory, code and build limits

- **Additional runtime RAM: 0 bytes.** The existing row/cache are unchanged.
- **Additional firmware bitmap/framebuffer/heap storage: 0 bytes.**
- The same 57×50 cell canvas and 240×12 hint canvas are reused. A normal value
  update still blits one 2850-pixel cell; eight idle frames write no pixels.
  Rounded-corner drawing remains bounded and only occurs on changed/full cells.
- Documentation PNGs are not referenced by firmware.

Matched x86-64 `cc -Os -ffp-contract=off` browser-backend ELF comparison:

| Section allocation | Base | Theme | Delta |
|---|---:|---:|---:|
| text + read-only data | 937471 | 937535 | +64 |
| initialized data | 31872 | 31872 | 0 |
| BSS | 1603328 | 1603328 | 0 |

A matched host build with `COFFEE_HOME_VISUALS=0` measured text/read-only
897231 → 897903 bytes (**+672 bytes**), unchanged initialized data 31840
and BSS 1603136. That configuration newly retains the procedural bean helper.
These are **native host proxies, not target flash sizes**; the conditional
helper cost must be accounted for by the target linker. Hardware fit against the 581564-byte
image limit, linked RAM headroom and real display timing remain unverified
without the pi32v2 toolchain and successful link. No target hardware was flashed.

Build attempts stop before compilation with `emu: no emcc (Emscripten)` and
`build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)`.
No toolchain was installed. Tests/previews use the real browser backend or firmware UI
compiled natively; there was no WASM or interactive browser run.

## Previews and reproduction

Native 240×240: [GRAINS](grain-edit-theme/page-1.png),
[CHAR/VOICE](grain-edit-theme/page-2.png).
Before/after (base left, theme right):
[GRAINS](grain-edit-theme/compare-1.png),
[CHAR/VOICE](grain-edit-theme/compare-2.png).

`tests/grain_theme_preview.c` renders the same mixed stored step fixture on
both trees: notes, an accent, a drum hit, tie and rest. Transport is advanced
by the existing sequencer and its resulting index is printed; the drawing
never invents a playhead. The images have identical musical/UI values.

```sh
cc -Os -w -ffp-contract=off -Ibuild/gen -Ifirmware/src \
  tests/grain_theme_preview.c -lm -o /tmp/grain-theme-preview
mkdir -p /tmp/grain-theme-preview-images
/tmp/grain-theme-preview /tmp/grain-theme-preview-images
```

For the base, compile that same file with
`-DGRID_UI_SOURCE='"/absolute/path/to/base/tests/ui_test.c"'`.
PPM images are 240×240 RGB888 exports of the actual RGB565 panel pixels.
Convert losslessly with Pillow to PNG; comparison images concatenate the base
and theme at native scale. `tests/run_tests.sh` registers the new theme suite;
its existing `GRAIN_GRID_BASE_TREE` comparison can point at the exact base.

Changed files: the two production rendering files above;
`tests/{grain_theme_test,grain_theme_preview}.c`, `tests/run_tests.sh`,
`web/emu/grain_grid_visuals_test.c`, this document and its four review PNGs.
