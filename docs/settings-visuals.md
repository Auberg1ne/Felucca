# Colorful GLOBAL SETTINGS

Base: `94eb70689834606683191f1cb7f3fd3d1cd2e2d0`.
The five tab views use a procedural 240×240 RGB565 skin. `COFFEE_SETTINGS_VISUALS`
defaults to 1; 0 keeps the old renderer for comparison. The skin is entered only
for ui.menu == 1. INFO, ABOUT, calibration, recovery and every non-menu page keep
their existing implementations. No setting identifiers, defaults, values,
getters/setters, input handlers, formats, backups or editor access were changed.
The cleaned menu's visible-row mapping and stale-selection handling are reused.

## Composition and differences from the reference

The approved five-panel SETTINGS illustration was not attached or found in the
workspace. This implementation follows the written composition/color requirements;
a pixel-level comparison against that illustration cannot be claimed.

- Fixed dark background; compact COFFEE GRAINS and HOME CLOSE header.
- Existing five tab icons, selected border and cyan/yellow/magenta/green/purple
  accents (RGB565 07ff/ffe0/f81f/07e0/a81f).
- Rounded selected-row outline/fill and accent marker; full left labels and
  right-aligned actual values. Binary controls have explicit ON/OFF text and a
  procedural switch. CPU2 keeps its actual ON (BETA) text rather than implying an
  immediately active switch. Live tune/hold units and learned-CC counts remain.
- Existing M font, with S fallback if label/value widths require it. Text and
  keycaps use explicit high-contrast colors so legacy light/custom palettes
  cannot make HOME CLOSE or footer controls disappear on the dark background.
- Three to seven rows fit without row scrolling: 20-pixel rows for up to six,
  18-pixel rows for seven, separated by 2 pixels. PRESETS still wraps as before;
  tab memory and ABOUT's existing document scrolling remain intact.
- Scientists and decorative illustration backgrounds are omitted. Seven-row
  SYSTEM has no spare decoration area, and enabling HOME sprite data solely for
  settings would increase hardware flash cost in builds where it is disabled.
- No visible tab sliding or other new animation. Existing cache/signature makes
  idle frames draw nothing. ANIM's behavior on other screens is unchanged.
- COLOR, STYLE and LARGE remain stored and effective on their existing themed
  screens. This fixed-color skin intentionally does not inherit those visual
  preferences. HELP, SCALE LEDS and all other runtime preferences are unchanged.

## Native previews

Each image is an unscaled 240×240 capture of the real native browser backend:

| DISPLAY | CONTROL | AUDIO | MIDI | SYSTEM |
|---|---|---|---|---|
| [PNG](settings-previews/settings-display.png) | [PNG](settings-previews/settings-control.png) | [PNG](settings-previews/settings-audio.png) | [PNG](settings-previews/settings-midi.png) | [PNG](settings-previews/settings-system.png) |

To regenerate, compile web/emu/settings_visuals_test.c with -O1,
-ffp-contract=off, -Ibuild/gen -Ifirmware/src and -lm. Run it with a directory
argument to write five settings-TAB.ppm files. Convert PPM to PNG losslessly
(e.g. Pillow Image.open(...).save(...)); do not resize. These documentation PNGs
are not firmware assets or linked into the image.

## Cost

No new mutable persistent/static UI state, allocations or framebuffer. The
existing ui.menu_sig cache and canvas are reused. Each row uses a fixed 16-byte
local formatting buffer plus scalar locals; exact target stack delta is unknown.
Firmware bitmap bytes added: zero. Constants include five 16-bit accent colors
and a nine-entry key-hint table (about 72 bytes on the 32-bit target ABI, excluding
pooled strings). A changed view transfers 24+108+108 screen rows, the same full
240-row maximum as the existing menu. Idle frames transfer zero pixels.

Native GCC -Os size proxy, same native_check source with skin disabled/enabled:
text/RO aggregate 931830 -> 934502 (+2672 bytes), data 31688 -> 31848 (+160),
BSS unchanged at 1603264. Total loaded code/data proxy: +2832 bytes (~2.8 KiB).
Host pointer relocation tables and x86 code are not a target flash measurement.
The 581564-byte target limit, actual target stack and LCD timing need a real link
and hardware validation. No hardware fit/readiness claim is made.

## Validation

ASan/UBSan uses the established DSP exclusions documented in mixer-top-knobs.md,
with no additional exclusions. Tests exercise every visible value, all five tab
colors, every selected-row accent, ON/OFF switch pixels/text, HOLD/TUNE units,
long CPU2/source/channel values, maximum learned-CC count, immediate updates,
no row ink loss/overlap, idle redraw suppression and native bounds.

The existing all-palette/style/LARGE test also verifies exact visible labels and
absence of hidden row labels. Full UI tests retain navigation, calibration,
INFO/ABOUT, help, scale LEDs and performance controls. Three obsolete visual
expectations were updated: static tab redraw behavior and GREY/MONO's intentional
fixed-color menu exception; all other grayscale screens remain checked.

Old/new renderers produce byte-identical audio while visiting every tab and
editing SPEAKER EQ and TUNE during playback. Cross-checkout navigation additionally
returns from every menu tab to HOME and EDIT to catch leaked canvas/palette state.
Storage, backup, migration and stable editor identifier suites remain unchanged.

Full CPU2=0 host compilation still fails core.h:336's existing track_t size
assertion on both the exact base and this branch. No assertion was disabled.
Actual conditional row declarations/mapping pass with CPU2 off/on (27/28 rows).
The full CPU2-enabled native suites run normally.

Build attempts: `emu: no emcc (Emscripten)` and `build.sh: no toolchain in
/home/agent/.jieli/toolchain (run tools/get_toolchain.sh)`. No tools were installed.
Native backend previews/tests passed; no newly linked WASM or target ELF exists.

Final sanitizer results: 590 focused visual checks; 12,321 all-palette/style/LARGE
rendering checks; 226 visibility/normalization checks; 850 UI assertions;
172 editor, 91 backup and 76 persistence assertions; settings migrations passed.
Conditional mapping passed CPU2 off/on, and frontend smoke passed 20 checks.
There are no new sanitizer failures. The CPU2=0 full host build blocker above
remains, identically reproduced on base and feature branches.

Legacy non-menu traces matched across 290 snapshots. The same cross-checkout
comparison passed all 290 snapshots with COFFEE_HOME_VISUALS=1, protecting the
current scientist HOME, MIXER and CLOCK graphics after returning from all five
settings tabs.
Audio during all tabs and live edits matched all 705,600 bytes against the exact
base, SHA-256 `ecac6169c2f793363fe656e4e312c48cf2373f7ee5b1d498c4065e263fd9ed4a`.
`git diff --check` and shell syntax checks passed. No golden files were replaced.
