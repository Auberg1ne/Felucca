# MIXER upper circular controls

Based on `3060988fa503cc22fbb21df3dd842b5b3bd8a4d0`. Only the four 57×44
upper MIXER cells (y28–71) change. LEVEL, PAN and REV use the existing
procedural MIXER ring primitive and their actual parameter descriptor ranges.
Values use `param_format`, including dB and percent units. MUTE uses a closed
binary ring: hollow OFF, filled ON, with an explicit text value. All four
indicators use the selected channel's cyan/yellow/magenta/green accent.
White labels retain readability independent of the font's grayscale ramp.

No input mappings, parameter values, audio processing, images, lower channel
strips, scientists, selected-channel frame, steps or footer were changed.
Rendering is cached per control; channel selection, value changes and hot-control
feedback invalidate the relevant cells. Leaving MIXER invalidates the cache.
The feature follows the existing browser-default / hardware-opt-in MIXER skin;
`COFFEE_MIXER_TOP_KNOBS=0` retains the old tiles for reference comparisons.

## Cost and validation

Additional persistent cache: 20 bytes (four 32-bit signatures plus valid flag
and alignment). No new bitmap bytes, allocations or audio callback work.
A changed control blits 2,508 pixels (5,016 RGB565 bytes); all four are 10,032
pixels. Existing canvas memory is reused. Small code/label tables increase
read-only storage; an actual target-link delta is unavailable without pi32v2.
The 581,564-byte image limit remains unverified; no hardware readiness claim.

`web/emu/mixer_top_knobs_test.c` sweeps every valid parameter value on all four
channels, checking exact formatted text, selected-channel colors, binary mute
and rendering bounds. `mixer_visuals_test.c` retains physical encoder, selection,
meter, mute, LARGE, HOME/CLOCK and step tests. `MIXER_TOP_COMPARISON` compares
270 navigation snapshots, including every pixel outside the changed cells.
Old/new top renderers also render identical real audio gestures for bytewise
comparison. Both comparisons are registered in `tests/run_tests.sh`.

Sanitizer invocation uses ASan/UBSan with the established DSP exclusions:
`-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow`.
No sanitizer checks are disabled specifically for this renderer.

Validation results: 9,265 focused knob checks, 7,583 MIXER checks, 89 HOME
checks and 850 UI assertions passed under ASan/UBSan. Bounds and static-value
text clipping counts were zero. The browser frontend branding smoke test passed
20 checks. The 270-snapshot old/new comparison matched parameters, navigation
and every framebuffer pixel outside the upper controls.

Build attempts stopped before compilation: `build.sh: no toolchain in
/home/agent/.jieli/toolchain (run tools/get_toolchain.sh)` and
`emu: no emcc (Emscripten)`. No toolchain was installed.

Old/new MIXER audio matched all 705,600 bytes, SHA-256
`19edec409a606fa3d4080259771717ced0c46bdd9b8104823b8d089849d97338`.
`git diff --check` passed. The native 240×240 preview was inspected for label,
value and control readability; no production bitmap was added.
