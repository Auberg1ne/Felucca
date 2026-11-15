# Experimental Hann / Tukey grain windows

Base: `feat/coffee-grains-seq-theme` at **d9e352f9380da85dedd0b37434a513fed5b6b636**.
Branch: `experiment/coffee-grains-grain-windows`.
Architecture reference: `docs/graine-engine-comparison.md` on audit commit `f4f23dbe65cedff297750ff4e13034f5d3726423` (not merged into this branch).

This is an **opt-in DSP experiment**, not a shipping control or release. Hann remains the exact default. No UI, IDs, presets, parameter mappings, modulation, serialization, factory recordings, decoder, allocator, scheduling or four-track voice policy changed.

## Implementation

`firmware/src/eng_grain.c` retains `GR_HANN` and `gr_win` verbatim. Without `FELUCCA_GRAIN_WINDOW_EXPERIMENT=1`, the render lookup macro expands to the original `gr_win(phase)` expression and the grain structure is unchanged. Neither the Tukey table nor the test selector exists in that build.

With that flag enabled, tests may set the translation-unit-local `gr_test_window`: 0=Hann, 1=Tukey. `gr_spawn_one` latches `gr_test_window == 1` into `gr_grain_t.window`; other selector values become Hann. The byte uses existing structure padding in the measured host layout. Existing grains never read the selector. A later selector change affects only newly admitted grains. No user interface/editor/MIDI command or persistent field exposes this mechanism. Tests set the selector outside the audio render; concurrent control writes are not an implemented feature.

`firmware/src/grain_tukey.h` contains a **257-entry, int16 Q15 Tukey window, alpha=0.5**, explicitly zero at both endpoints. It has a flat middle half and cosine edge quarters. `tools/gen_grain_tukey.py` independently generates the standard mathematical formula; no GRAINE code or data was copied. Its first half is rounded to nearest integer and mirrored exactly. The generator uses floating point offline; the callback uses only fixed-point lookup, bounded integer interpolation and the existing block ramp.

For phase `p`, `i=p>>24` is 0..255 and `f=(p>>9)&32767`. The lookup is `table[i] + floor((table[i+1]-table[i])*f/32768)`. Signed floor is expressed without a negative signed shift. `|delta|<=32767`, `f<=32767`, so the product and negative-rounding adjustment fit int32. It is a convex interpolation bounded by adjacent entries and 0..32767. Both endpoints are safe; UINT32_MAX interpolates to zero. The Hann arithmetic was not rewritten to share this implementation.

The existing `gr_run` still evaluates its window only at block endpoints and linearly ramps between them; sample decoding, gain/overlap normalization, direction and position updates are untouched. There is no new per-sample coefficient computation, floating point, division, buffer, allocation or callback synchronization. The division by constant 32768 is only in the Tukey endpoint lookup. There is no live table rebuild or window morphing.

## Executed tests

Outputs were generated outside the worktree under `/tmp/grain-window-results`. Reproduction requires the same existing generated tables and pinned engine dependencies as the other host fixtures; no dependencies were installed.

| Test | Result |
|---|---|
| `grain_window_numeric_test.c` | **8,000,773 checks passed**, full ASan/UBSan with no category exclusions |
| `grain_window_kernel_test.c` | **30,160 checks passed**, actual production `gr_run` with full ASan/UBSan, no category exclusions |
| `grain_window_latch_test.c` | **97,473 checks passed**; all personalities, minimum/maximum SIZE, FREEZE HOLD endpoints, both directions/windows, full grain lifetimes, output canaries |
| Exact-base audio matrix | **84 states × 4 renders = 336 successful sanitized renders** |
| Existing position regression, experimental build with Hann default | **854,920 checks passed** |
| Existing GRAIN-only policy regression, experimental build with Hann default | **380 checks passed**, including legacy rejection/storage protections |

The kernel test executes the actual grain render directly without unrelated UI/mixer initialization, using signed sample extremes, Q16 steps 256/32768/65536/131072, forward/reverse paths, output canaries and lengths 16,17,31,32,33,441,86432 frames. The lookup test compares two million pseudorandom phases against a wide reference, mirror symmetry (within four Q15 units after phase quantization), mathematical table values, convex bounds and endpoints.

The latch test clones a running grain/pool, changes the global selector and renders both copies to completion: PCM remains identical. It verifies the next grain uses the newly selected window, so the test does not merely inspect a descriptor. It also checks selector 255 safely selects Hann.

### Audio matrix and identity

`tests/grain_window_compare.py` compiles the **same** audio fixture against:

1. The clean exact base.
2. This branch, normal build (no experiment flag).
3. This branch, experimental build with Hann selected.
4. This branch, experimental build with Tukey selected.

All **84** base/default/experimental-Hann PCM comparisons are byte-identical. All **72** sample-backed Tukey states differ from Hann and pass meaningful nonzero-output assertions. All **12** sine-fallback states are byte-identical between windows: fallback creates no grains, so the new grain window correctly has no effect there.

Coverage: four personalities × seven sources (CELLO 0, FLUTE 2, GUITAR 3, SYNTH 13, VOX 14, a synthetic user-zone descriptor, empty-slot sine fallback) × three SIZE/HOLD scenarios. Scenarios are SIZE=0/HOLD=0, SIZE=80/HOLD=127, SIZE=127/HOLD=127. Each uses DENS=127, RAND=127, four simultaneous tracks with real nonzero master (4096), and verifies all four channel levels. Every sample-backed run observes both forward and reverse active grains; forward/reverse active-grain observations and grain/part sizes match between the two windows in all 84 states. The user-zone fixture points at valid existing ADPCM test material; it does not upload, rewrite user flash, or exercise the sample journal. Other settings, RNG initialization and note inputs are identical between paired runs.

Audio fixtures and latching/retained integration regressions use the repository's existing host exclusions: `-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow`. This does **not** make them fully strict DSP tests. Strict interpolation and direct production grain-kernel tests above run with **no such exclusions**, including signed overflow checks. No sanitizer diagnostics were reported. No exclusion was added to hide a new failure.

Reproduce:

```sh
python3 tools/gen_grain_tukey.py --check
python3 tests/grain_window_compare.py \
  --base-tree /path/to/clean/d9e352f-worktree \
  --out /tmp/grain-window-comparison
```

The runner compiles numerical, kernel, latching and all audio comparisons; it writes measurement JSON and paired WAVs. Compiler flags use `-O1 -g -ffp-contract=off`; strict targets use `-fsanitize=address,undefined -fno-sanitize-recover=all`. The generator check fails rather than silently refreshing a stale table.

## Listening captures and measurements

These are actual four-track GUITAR/source-3 renders, scenario 1, with identical state/seeds and no gain normalization, resampling, trimming, clipping or limiter added by the exporter. Each WAV contains all **131,072 frames**, 44.1 kHz, stereo PCM16, approximately 2.972 seconds, **524,332 bytes** including header. The exporter asserts every integer fits PCM16 before conversion. They are documentation listening artifacts, not replacement firmware sample assets.

| Personality | Hann | Tukey |
|---|---|---|
| CLOUD | [Listen](audio/grain-windows/cloud-hann.wav) | [Listen](audio/grain-windows/cloud-tukey.wav) |
| FREEZE | [Listen](audio/grain-windows/freeze-hann.wav) | [Listen](audio/grain-windows/freeze-tukey.wav) |
| STUTTER | [Listen](audio/grain-windows/stutter-hann.wav) | [Listen](audio/grain-windows/stutter-tukey.wav) |
| SHIMMER | [Listen](audio/grain-windows/shimmer-hann.wav) | [Listen](audio/grain-windows/shimmer-tukey.wav) |

[Complete measurements](audio/grain-windows/measurements.csv) cover both windows for all 84 states. `energy` is the sum of squared output integers; `diff-energy` sums squared adjacent differences independently per stereo channel; `max-jump` is the largest absolute same-channel adjacent difference. Forward/reverse values count active-grain observations, not unique spawns.

Representative peak / RMS / maximum adjacent jump:

| Personality | Hann | Tukey |
|---|---|---|
| CLOUD | 15484 / 2788.967 / 1598 | 16684 / 3727.465 / 2397 |
| FREEZE | 16639 / 2946.172 / 1805 | 17184 / 3930.291 / 1870 |
| STUTTER | 16838 / 2766.152 / 1405 | 16912 / 3717.660 / 1584 |
| SHIMMER | 18571 / 2988.285 / 3365 | 18405 / 3964.533 / 3308 |

Tukey raises RMS in these cases because it spends more of the grain near unity while retaining the existing overlap gain. Ideal continuous Hann mean-square is 0.375; Tukey(alpha=0.5) is 0.6875, before finite-table/block approximation or sample correlation. **No compensating gain change was made.** Larger differences/jumps do not alone prove a new audible click, nor does higher RMS prove a better sound. Listening and later loudness-matched evaluation are required; these captures intentionally retain native level differences.

### Known short-grain limitation, preserved

The direct kernel test confirms **320** tested single-block cases (length <= CTL=32) produce zero for both windows. A grain that begins and ends in one render call has window endpoints zero, so the unchanged block-ramp approximation fills zero throughout. This is existing Hann behavior, not caused by Tukey. It is explicitly asserted and reported rather than relaxed into a false nonzero pass. Normal minimum SIZE grains are much longer; boundary fitting can produce these tiny grains. Correcting intra-block envelope evaluation is outside this experiment and would intentionally change baseline PCM. No claim is made that every legal tiny grain is audible.

## Resource measurements

Host compiler `cc -Os -ffp-contract=off`, native object measurements:

| Build | text | data | BSS |
|---|---:|---:|---:|
| Exact-base native browser unity object | 885503 | 31060 | 1603232 |
| This branch, experiment disabled | 885503 | 31060 | 1603232 |
| Audio comparison fixture, experiment disabled | 533779 | 19660 | 310568 |
| Same fixture, experiment enabled | 534445 | 19660 | 310568 |

The default native browser unity object is also **byte-for-byte identical** to the retained exact-base object. Default code/data/BSS delta is zero, not just default audio identity.

The experimental fixture adds **666 text/constant bytes**, including **514 bytes** for the Tukey table and the test selection/render branch. This is a host fixture measurement, including conditional test-harness code; it is not a target flash delta. Static `gr_test_window` is one byte (test builds only); aggregate measured data/BSS did not grow because of section alignment. `gr_grain_t` stays **64 host bytes**, `gr_part_t` stays **7,296/12,888 bytes per track**. Four engine reservations remain **51,552 bytes**. No new persistent buffer is allocated. The latched byte fits existing padding on this host; ordinary 32-bit layout also has a spare byte, but a target compiler must confirm it.

Host `-fstack-usage`: `grain_render` 328→336 bytes in the experimental fixture; `gr_spawn_one` 112 unchanged; fixture main 448 unchanged. `gr_run` is inlined, so its cost is reflected in its caller. These are individual compiler frames, not a proved whole-call-chain or FM1 stack high-water result. Default build's identical object implies no default stack/code change.

The broader UI/MIDI/motion suites were not rerun: no UI/protocol code changed, and the default native object is identical. Historical failures are not claimed as passing. Emscripten is unavailable; no WASM build was performed. No target build/link or hardware profiling was performed. The **581,564-byte firmware image limit remains unverified**, as do target CPU deadline and stack headroom. No claim of hardware fit, release readiness or better sound. No merge, deployment or flashing.

## Changed files

- `firmware/src/eng_grain.c`: conditional test selector, per-grain latch and endpoint lookup dispatch.
- `firmware/src/grain_tukey.h`, `tools/gen_grain_tukey.py`: independent reproducible table and bounded fixed-point interpolation.
- `tests/grain_window_numeric_test.c`, `grain_window_kernel_test.c`, `grain_window_latch_test.c`, `grain_window_audio_test.c`, `grain_window_compare.py`: strict numerical/kernel checks, latching, exact-base audio matrix and capture runner.
- This report and `docs/audio/grain-windows/`: paired actual renders and measurements.
