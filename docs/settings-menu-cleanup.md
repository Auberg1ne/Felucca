# GLOBAL SETTINGS presentation cleanup

Base: `064c455ae09c71a4450c1d4282cb505410664ea1`.
Only `firmware/src/ui_menu.c` changes production behavior, and only in the menu.

Visible rows in order:

- DISPLAY: ANIM, LEDS, SCREEN OFF.
- CONTROL: HOLD, KNOB ACCEL, FX LATCH, BPM LOCK, SCALE LEDS, STEP PREVIEW.
- AUDIO: SPEAKER EQ, USB LEVEL, CLICK, CLICK LEVEL, COUNT-IN, TUNE.
- MIDI: MIDI IN, T1 CHANNEL, T2 CHANNEL, T3 CHANNEL, T4 CHANNEL, LEARN CLEAR.
- SYSTEM: USB SERIAL, RESTORE LAST, HELP, conditional 2ND CORE, CALIBRATION, INFO, ABOUT.

Counts: 3/6/6/6/7 with CPU2, 3/6/6/6/6 without. Nine rows are hidden, not removed.
All MI identifiers, logical tab assignments, editor descriptors/IDs/order,
getters/setters, defaults, runtime consumers, settings/project/backup layouts and
existing import normalization remain intact. Hidden preferences remain available
through the editor. HOME=LEVELS, SELECT roles, chord entry, MIDI following and
wake policy retain their saved behavior. This does not repair or redefine ANIM.

The renderer maps visible ordinal positions to logical rows; PRESETS and tab
navigation use the same mapping. Remembered positions are visible ordinals.
Hidden/invalid selections normalize before both drawing and editing, to the first
visible row of their tab (DISPLAY for an invalid ID). They never reuse a stale
ordinal that might resolve to an action such as LEARN CLEAR. Normal tab changes
still restore a valid remembered position. The existing graphics, palettes,
font preferences and dynamic row sizing are reused; no redesign or new image.

## Verification

- Focused C test: exact order/counts, wrap, tab memory, stale/invalid selections,
  hidden values unchanged by physical menu edits, PER4 round trip/unknown byte,
  legacy HOME level mapping and retained actions.
- Actual row declarations/mapping functions compile and execute in a small harness
  with CPU2 off/on, confirming 27/28 visible rows without substituting a second
  visibility implementation.
- Browser-backend rendering checks every palette, STYLE and LARGE combination,
  visible labels and absence of all nine hidden labels, within 240x240 bounds.
- Full UI regression preserves SCALE LEDS, HELP, HOME, SELECT, chord entry,
  MIDI-follow/wake, calibration and action coverage. Old tests that physically
  edited now-hidden rows explicitly use their retained setter; all value,
  rendering, mapping and persistence assertions remain. Editor tests continue
  exercising the actual hidden-setting protocol IDs.
- Backup tests restore all nine nondefault hidden preferences and an unknown
  byte, then compare the complete canonical settings record byte for byte.
- PER1..PER4 and deferred persistence/retry suites remain applicable. The old
  settings fixture's MONO-is-last assertion was corrected: MONO remains index 9,
  while COFFEE was already appended at index 10 on the base revision.
- 270 navigation snapshots match the untouched base, including every non-menu
  pixel, musical parameters, global state and preferences.
- Audio comparison against the untouched base: all 705,600 bytes identical,
  SHA-256 `19edec409a606fa3d4080259771717ced0c46bdd9b8104823b8d089849d97338`.

ASan/UBSan use the established DSP exclusions documented in mixer-top-knobs.md;
no additional exclusions or production assertions were disabled for this change.
Set SETTINGS_BASE_TREE to an untouched base checkout to enable the optional
cross-checkout navigation/audio comparisons registered in tests/run_tests.sh.

## Limits and cost

No additional persistent/static RAM, allocation, framebuffer, bitmap or storage
writes were introduced. Mapping scans at most MI_COUNT rows on the UI thread.
There is a code-only flash change and bounded local stack use; an exact target
size/stack delta requires the unavailable pi32v2 toolchain. No target fit claim.

A full CPU2=0 native host build fails the existing core.h:336 assertion:
`track_t: a multiple of 16 bytes (a cheap trk[] index on the target)`.
The identical failure was reproduced on the untouched base. This host ABI issue
was not changed; conditional menu tests cover both configurations separately.
Target hardware behavior and physical calibration/recovery were not exercised.


Final passing counts under ASan/UBSan: visibility 226; rendering 12,321;
UI 850; editor 172; backup 91; persistence 76. The settings migration suite
also passed. Conditional mapping passed with CPU2=0 (27 rows) and CPU2=1
(28 rows). No new sanitizer failures remain. The unrelated full CPU2=0 host
compile blocker is described above. `git diff --check` and shell syntax passed.
The attempted target build stopped with `build.sh: no toolchain in
/home/agent/.jieli/toolchain (run tools/get_toolchain.sh)`; nothing was installed.
