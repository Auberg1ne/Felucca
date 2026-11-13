# COFFEE GRAINS MIXER preview

Base: 19082aaf46cbd83cff0eb3894674dd91c8649736.
A separate approved MIXER illustration was not available among the supplied files
or workspace assets. The implementation follows the user's written design using
the existing approved HOME scientists and palette; it does not claim an exact
match to an unseen MIXER reference or introduce unrelated character artwork.

## Scope and real controls

This is the existing HOME-cycle MIXER (SC_TRK / GR_TRK), not a new page or input
mode. HOME -> MIXER -> CLOCK -> HOME and ALGORITHM channel selection are unchanged.
A compact COFFEE GRAINS header reuses the existing HOME header routine. Its cache
is safely reconstructed on returning to main HOME. Main animated HOME, CLOCK,
engine pages, menus, settings values and physical input code are unchanged.

The original selected-channel LEVEL/PAN/REV/MUTE cards and param_format paths are
retained, as are their actual K1/K2/K3/K4 editing functions and hot/motion feedback.
LARGE retains its stored preference and gives larger labels on the MIXER's fixed
compact cards instead of overlapping the strips with tall cards. Its behavior on
all other pages is unchanged. No parameter, mapping or stored setting is invented.

Four 57x122 strips at the existing CARD_X positions and Y_GRAPH=76 have cyan,
yellow, magenta and green accents. They show CH1..CH4, each scientist, actual
track dB level, PAN, REV and MUTE, plus arm/record and MIDI-activity indicators.
PAN/REV ring pointers are procedural summaries; numeric readings retain the
legacy percent calculations. Per-strip level dB is rounded as in the original
MIXER; the selected card retains the existing detailed formatting and units.
Muted status is explicitly M ON (mute engaged), or M OFF. A bright double frame
always follows actual song.sel independently of level, mute or MIDI activity.
The selected preset/sound and MIXER page identification remain in the footer.

Output meters reuse the original per-track peak, meter_px and fall behavior;
there is no new metering or audio-callback work. REV send and channel LEVEL have
procedural gauges, and the existing selected-pattern coffee-bean strip/playhead
is enabled for MIXER as well as HOME. CLOCK and all other pages retain their old
step-bar path. Temporary help/MIDI-learn footer content remains unchanged.

## Assets, memory and render cost

No image, sprite, palette or font assets are added or modified. Each portrait
uses the existing idle-pose stream and shared RGB565 palette. A fixed even-row /
even-column selection while decoding maps the 90x56 tile into 45x28 pixels. This
is simple 2:1 sampling with counters and parity checks, not a generic runtime
resizer, rotation, floating-point transform or temporary bitmap. Faces/accessories
are small decoration; the real controls and numeric readings take priority.

The UI cache is **20 bytes** (four signatures and a valid byte, plus alignment).
The nine procedural ring-pointer directions use **18 constant bytes**. No pool
buffer, framebuffer, heap allocation, persistent state or audio state is added.
Portraits decode into the existing RAM canvas. A full strip transfer is
57*122*2 = **13908 bytes**. A meter-only change copies **3*28*2 = 168 bytes** and
does not decode/repaint the portrait or controls. The full-frame/forced paths
remain available for navigation, settings, wake and control edits.

New linked code size, actual flash headroom and hardware CPU/display time cannot
be measured without the target toolchain. Existing sample + startup + HOME
sprite payload is still 449224 bytes when all image features are enabled,
leaving only an arithmetic 132340 bytes for code/other data/alignment in the
581564-byte slot, not measured linked headroom. No hardware-fit claim is made.
The MIXER skin follows COFFEE_HOME_VISUALS (browser on, target opt-in with the
existing FELUCCA_HOME_ART flag). COFFEE_MIXER_VISUALS=0 is the legacy-renderer
comparison control used in tests, with main HOME left enabled.

Target build attempt: `build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)`.
WASM attempt: `emu: no emcc (Emscripten)`. No toolchain was installed and no
hardware was flashed. Native browser-backend rendering is provided for review;
actual WASM execution, target link and physical-screen timing remain unverified.

## Validation

The focused native browser-backend test verifies all four actual displayed
control values against TP formatting, editing each control on each selected
channel without changing any other parameter, actual ALGORITHM selection,
output/mute indicators, LARGE preference preservation, bean steps, navigation,
canvas bounds and numerical readability. Intentional rolling-digit clipping in
the original cards is distinguished from unintended static-value or strip
clipping. The latter are both zero. Meter-only redraw is exactly 84 pixels.

The navigation comparison explicitly traverses HOME -> MIXER -> CLOCK -> HOME
in normal and LARGE modes, then other page families. It records all parameters,
stored preferences/globals and every non-MIXER framebuffer, including main HOME
and CLOCK. No golden files are changed to obtain passing comparisons.

The audio regression renders the actual heavy four-part song while MIXER is
visible and channel/PAN/REV/MUTE edits occur, with old and new renderers. It
compares every float32 output byte. Existing UI, main animated HOME, browser
frontend, source-asset integrity and editor-order checks are also rerun.
Integration sanitizer runs use ASan/UBSan with the repository's existing DSP
exclusions (signed overflow, shift, bounds, object-size, pointer-overflow).

Changed areas: mixer_visuals.c, MIXER-only layout/dispatch and step-strip guards,
focused MIXER/navigation/audio tests, host runner, this report and a native
240x240 screenshot. No production audio, MIDI, input mapping, sample, storage,
project-format or global-settings persistence source is changed.

## Results in this environment

- MIXER: **7583 checks** passed under scoped ASan/UBSan. No out-of-bounds canvas
  transfers, strip text clipping or static numerical clipping. Twelve intentional
  clipped rolling-digit draws from the existing cards are retained, not treated
  as a new static-value truncation defect. Meter-only redraw is exactly 84 pixels.
- **270 snapshots** match between legacy/new MIXER rendering for all parameters,
  global values/preferences and every captured non-MIXER framebuffer. The trace
  explicitly includes HOME/MIXER/CLOCK cycling in normal and LARGE modes.
- Full UI **850** assertions; main animated HOME **89** checks passed under scoped
  ASan/UBSan. Browser frontend **20**, editor ordering **48**, backup **64** checks
  pass. Existing HOME sprite and startup asset integrity/reproduction tests pass.
- Heavy-song audio with MIXER visible and real channel/PAN/REV/MUTE edits is
  bit-identical across **705600 float32 bytes**, SHA-256
  `19edec409a606fa3d4080259771717ced0c46bdd9b8104823b8d089849d97338`.
  The unchanged main-HOME heavy-song render also matches the base.
- Native symbols confirm a 20-byte UI cache and 18-byte ring direction table.
  They are not measurements of target linked flash/RAM headroom.

mixer-visuals-preview.png is captured from the actual native browser framebuffer
with deliberately varied mixer test values. It is documentation only, not a
runtime image asset or a screenshot of actual WASM execution. The selected
channel's exact values and the other strips' rounded legacy readings remain real
parameter-derived data rather than decorative values from an illustration.
