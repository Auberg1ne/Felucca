# Disable standalone long-press EDIT

Base: `b58797d2477e32b70d53f42c0918fc6bda64056a`.
Branch: `fix/coffee-grains-disable-long-edit`.

## Behavior and exact scope

In the COFFEE GRAINS build (`FELUCCA_GRAIN_ONLY=1`), holding EDIT alone until
the configured HOLD threshold no longer opens the engine/sound quick layer.
It causes no page transition, preset load, parameter change, popup or
confirmation. Continued holding does not generate taps. Releasing the consumed
hold cannot execute the normal short-press action.

Short EDIT retains its existing behavior: GRAIN page selection, row-entry
policy, STEP/detail actions and USER/PROJECT rename actions. Other page buttons,
knobs, long shortcuts and GRAIN personality selection are unchanged.

This is deliberately **only the standalone long-hold entry**. Existing explicit
EDIT+key/encoder combinations and the short double-tap lock remain unchanged,
including their access to the retained sound layer. An already-open locked
layer retains its existing close/escape behavior. The menu code, factory/user
presets, INIT/CLEAN actions and sound-loading infrastructure are not removed.
Screen-wake and other existing context/modifier behavior are not redesigned.
The existing short-tap first-use hint text is also unchanged; long EDIT itself
emits no hint. Removing those other entry points or changing hints would be a
separate UI behavior change.

Diagnostic multi-engine builds (`FELUCCA_GRAIN_ONLY=0`) retain the old gesture.
The product's default remains GRAIN-only; no engine-policy setting is changed.

## Implementation

`firmware/src/ui_layer.c:layer_gesture` already owns layer-button timing and
release dispatch. The added guard applies only to an unopened, non-combo EDIT
press in the GRAIN-only build. At the existing configured deadline it marks
the existing timer `LY_DEAD` and clears its pending double-tap candidate.

This reuses the normal consumed/dead-gesture release path:

1. The hold never calls `layer_opened`, so it cannot display the sound layer,
   mark it seen, save its first-seen preference or load a sound.
2. The consumed marker prevents repeated hold recognition.
3. Release clears the armed gesture without returning an EDIT tap.
4. The deadline is checked on release too. If polling resumes only after a
   long armed press has been released, it still cannot become a short tap.

Timing uses the existing unsigned timestamp and HOLD preference. No new timer,
thread, persistent field, callback, bitmap or buffer is added. Tick-wrap
semantics and all other layer gesture code remain intact. Short taps still
pass through the unchanged `layer_tap` / `page_tap` functions.

There are no DSP, asset, parameter-ID, default, serialization, preset, MIDI,
editor-protocol or layout edits.

## Validation

| Check | Result |
| --- | --- |
| Focused UI/hold integration | **111230 assertions passed**, **1420 hold cases** |
| Full-check timing primitive | **1920 assertions passed**, no sanitizer exclusions |
| Retained EDIT/other-gesture snapshots | **288 byte-identical** to exact base |
| Existing retained UI snapshots | **270 byte-identical** to exact base |
| Real four-track sequencer, held vs control | **524288 PCM bytes identical**, nonzero output |
| Shipping legacy grid fixture | Same base/fix failure at line 63: assumes DRUM can be selected |
| `git diff --check`, shell syntax | Pass |

The hold matrix covers all visible GRAIN-only pages, all four tracks, every
HOLD setting, playing/stopped UI states, all three HOME views, all settings
tabs, INFO/ABOUT, confirmation and NAME contexts. Additional cases cover all
four personalities on both GRAIN grids with an active non-first row, occupied
USER/PROJECT records, and populated STEP editing. Assertions check unchanged
project/step data, user-preset banks, preset ownership, favorites, settings,
undo, page/row selection, confirmation/rename state, reload and panic flags.

It tests repeated holds, continued holding, release-only deadline recognition,
uint32 tick wrap, exact threshold-minus-one/threshold/threshold-plus-one release
boundaries, a pending double tap whose second press becomes long, and a normal
short tap immediately after a consumed hold.

Snapshot comparisons exercise short EDIT from every visible page/track,
other buttons' long actions (HOME, FX, GLO, SCL, SEQ, REC, SAVE), existing EDIT
double taps and explicit encoder combinations, plus SELECT/K1 GRAIN navigation.
The exact-base binary is required to **fail** the new no-menu hold test,
confirming it detects the previous behavior. No goldens were regenerated.
The existing 270 snapshots are independently rebuilt from base and fix.

Non-menu routes explicitly pass MIDI Program Change factory loading, user
preset loading, project save/load and sound-load undo. GRAIN-only/legacy
protections remain covered by the existing suites.

### Real playback control

The audio control and held-EDIT renders run in separate fresh processes so
DSP/FX/transport globals do not leak between controls. Both run the real
sequencer and mixer, with all four tracks sounding and master=4096. UI polling
continues while the hold crosses its deadline. Both reach sequencer step 11,
with peak 13962, RMS 1496.355513 and 119774 nonzero stereo samples; every PCM
byte matches. This verifies continuing playback rather than only setting a
playing flag. No audio processing was changed.

### Sanitizers and existing regressions

`long_edit_timing_test.c` runs ASan+UBSan with **all checks enabled**, including
signed overflow and shift checks. It exercises the exact gesture functions
without unrelated rendering/audio arithmetic. No diagnostics occur.

The larger rendered UI suite uses the repository's historical UI/DSP exclusions:
signed overflow, shift, bounds, object-size and pointer-overflow. ASan remains
enabled. These exclusions are disclosed and are not needed by the new unsigned
gesture logic. No sanitizer errors occur in the focused or broader runs.

Passing broader assertions: full diagnostic UI 850, editor 172, project 61,
presets 45, backup 103, GRAIN-only policy 380, editor/storage 70, persistence
24, four-part audio 330, CUTOFF-zero 2355, theme 8229, grid visuals 1935,
STRETCH descriptors 1764, GRAIN grid 2329 and FUN11/FUN10 format 41146.
The additional shipping-mode run of the older grid fixture stops at
`tests/grain_grid_test.c:63` (`!grain_grid_active()`): the fixture attempts to
select DRUM, which GRAIN-only policy rejects. The exact base fails identically;
the diagnostic grid run above passes all 2329 checks. The fixture was not changed.

Diagnostic multi-engine UI tests intentionally retain their original EDIT
layers; the new shipping behavior is tested separately above.

Known native baseline failures match the exact base, including failing
assertion names: MIDI 134 passing / 15 failing and motion 97 passing / 27
failing. No assertions were weakened or skipped. Web JavaScript is unchanged;
its focused policy check passed all 20 checks. The
full web suite was not rerun for this native button-routing change.

## Native size and RAM

Identical GCC 14.2 `-Os -ffp-contract=off` native compilation of
`web/emu/felucca_web.c`:

| Section | Base | Fix | Delta |
| --- | ---: | ---: | ---: |
| text | 879148 | 879229 | +81 bytes |
| data | 31060 | 31060 | 0 |
| BSS | 1603232 | 1603232 | 0 |

No RAM allocation or structure layout changes. Existing gesture fields and
flags are reused. Native individual stack frames remain 32 bytes for
`layer_gesture` and 208 for `ui_input`. These are not target worst-case stack
measurements. Audio/engine memory allocations are unchanged.

Emscripten and the target JieLi toolchain are unavailable; no tools were
installed. Native object size does not prove target flash fit. Hardware flash
fit remains unverified; no deployment or flashing was performed.

## Reproduction and changed files

```sh
python3 tests/long_edit_compare.py --base-tree /path/to/b58797d --out /tmp/long-edit-compare
cc -O1 -g -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=undefined -Ibuild/gen -Ifirmware/src tests/long_edit_timing_test.c -lm -o /tmp/long-edit-timing
/tmp/long-edit-timing
```

The usual generated tables and pinned engine dependency must already exist.
`tests/run_grain_only.sh` registers the integration and strict timing tests;
`LONG_EDIT_BASE_TREE` additionally enables exact-base comparisons. Existing
DSP suite entries and sanitizer policy are otherwise unchanged.

- `firmware/src/ui_layer.c`: consume the standalone EDIT hold deadline.
- `tests/long_edit_test.c`: UI state/data, retained gestures and playback fixtures.
- `tests/long_edit_timing_test.c`: strict deadline/release/wrap primitive tests.
- `tests/long_edit_compare.py`: exact-base snapshots and fresh-process audio control.
- `tests/run_grain_only.sh`: register tests and optional base comparison.
- `docs/coffee-grains-disable-long-edit.md`: this report.
