# GRAIN CUTOFF zero: dry bypass and sounding audio regressions

Base: `44ec4606c0469cc79326a4887b78e321ec704739` (`feat/coffee-grains-grain-only`).
Branch: `fix/coffee-grains-cutoff-zero`.
Audit: [F1 and F10 at 0883922](https://github.com/Auberg1ne/Felucca/blob/0883922bcfc7106242914ba2b94e7de4aa1ef7a1/docs/grain-dsp-audit.md).
Only these two findings are addressed.

**Behavior correction:** a saved project with GRAIN CUTOFF=0 now becomes audible
when its source and amplitude envelope are sounding. No saved data is rewritten;
zero was already described as a filter bypass, but previously discarded audio.
Projects using that accidental silence as a mute should use the existing MUTE
or LEVEL controls instead.

## Implementation

The only production change is five added lines inside `grain_render`'s existing
`cutoff == 0` branch in `firmware/src/eng_grain.c`:

1. Clamp the already rendered accumulator to the same ±65,535 input range used
   by the nonzero filter path.
2. Apply the existing Q15 amplitude ramp and `VOICE_FS` scaling, including the
   existing ×2 output convention, and **add** the result to the caller's buffer.
3. Retain the existing reset of filter-state slots `s[2]`, `s[3]`, `s[4]` and return.

The dry calculation follows the same two-stage rounding as `voice_amp`, with
wide multiplication intermediates local to this new branch. The existing AMP
modulation can boost a full envelope above unity (ramp up to 65,527). A direct
`voice_amp` call on a ±65,535 accumulator would overflow its signed product in
that case; a strict test caught `65535 * 65527`. The bypass therefore uses wide
intermediates for **both** gain products. Normal-range values retain the exact
existing gain result, and boosted ramps retain their intended scale without a
new overflow or extra amplitude clipping.

No nonzero-CUTOFF filter code, shared gain helper, filter coefficient, DSP
algorithm outside this bypass, persistent state, parameter ID/range/default,
preset, MIDI mapping, editor protocol, project format or sample asset changed.
In particular, the separately audited resonant-filter headroom issue F9 remains
outside scope; this branch does not modify that filtered path. TONE, DRIFT,
DTUNE, POS, STRETCH, source-head and FREEZE issues remain unchanged.

## Test-fixture repairs

`tests/grain_only_audio_test.c` and `tests/home_levels_test.c` explicitly set
`song.master_q12=4096` after their fixture resets. Every sounding capture must
pass peak, RMS, finite-energy and nonzero-sample-fraction checks before it can
serve as audio-equivalence evidence. The GRAIN fixture now covers all four
personalities, all four channel solos and both stereo-spread settings: 32
sounding cases, with the existing silent-level and HOME-only meter checks kept.

The MIXER/CLOCK helper and SETTINGS helper use an explicit browser MASTER
position (700, the existing default) and validate the actual recorded float
output. They already had a functioning browser master path; their new assertions
prevent future silent equivalence comparisons. Existing gestures remain intact.
The shared statistics helper is **test-only**.

The new cutoff-specific mixer harness starts each process from clean state,
uses the unchanged GUITAR source, real voice allocation/envelopes and mixer,
master gain 4096, no effects sends, and finite/nonzero final output checks.
It supports a single track, four simultaneous tracks, and live transitions
`-25 → 0 → +25 → 0 → -25`. Each four-track stage also checks all four channel-level
signals. A zero-master build is required to fail, and the original base's
CUTOFF=0 captures are also required to fail their sounding assertion. No silent
comparison is treated as success.

The analytic unit test independently calculates a known sine-fallback dry input
and its gain ramp, starting with a nonzero caller buffer. It checks exact
addition, rising/falling/zero/constant/above-unity ramps, filter reset, lengths
0/1/17/32/40 (renderer clamps to 32), and untouched output canaries. A bounded
synthetic grain-pool case separately exercises the real-grain accumulator limit
and the same ramps in every personality. This does not introduce an audio asset.

## Audio results against the exact base

The **same repaired test harness** was compiled against the clean exact base
and this implementation. Baseline production code and fixtures were not edited.
Each capture validates nonzero final output before comparison.

| Comparison | Result |
| --- | --- |
| All 100 nonzero integer CUTOFF values, −50…−1 and +1…+50, for CLOUD/FREEZE/STUTTER/SHIMMER | 400 sounding captures, **419,430,400 bytes identical** |
| Four simultaneous tracks, CUTOFF −25/+25, all four personalities | 8 sounding captures, **8,388,608 bytes identical** |
| Total nonzero-CUTOFF reference audio | **408 captures, 427,819,008 bytes identical** |
| Existing retained UI/navigation trace, compiled against base and fix | **270 snapshots / 31,381,560 bytes identical** |
| MIXER old/new renderer during live edits | **705,600 bytes identical**, both sounding |
| CLOCK old/new renderer during live edits | **705,600 bytes identical**, both sounding |
| SETTINGS old/new renderer during live edits | **705,600 bytes identical**, both sounding |
| HOME instrumentation on/off and existing two-core host fixture | **14,336,000 bytes identical** across all three configurations, now with sounding captures |

The 400 single-track comparisons and eight four-track comparisons were executed
with the final compiled harnesses; the committed comparison runner includes both.
Recorded nonzero-CUTOFF output remains unchanged. Live transitions containing
zero deliberately differ from the base because the dry output is now present.
No audio or image goldens were updated.

CUTOFF=0 measurements from 131,072-frame stereo captures (integer mixer units):

| Personality | One-track peak | One-track RMS | Nonzero samples / 262,144 | Four-track peak | Four-track RMS |
| --- | ---: | ---: | ---: | ---: | ---: |
| CLOUD | 12,709 | 3,755.326615 | 258,286 | 17,882 | 6,195.436888 |
| FREEZE | 10,893 | 2,880.546725 | 258,034 | 17,233 | 5,160.854632 |
| STUTTER | 10,957 | 2,444.524397 | 253,104 | 18,012 | 4,389.153235 |
| SHIMMER | 12,881 | 4,169.665248 | 258,328 | 18,538 | 6,451.262990 |

Every stage of the four-personality, four-track live-transition runs passed its
sounding check. The original base produced zero output at CUTOFF=0 and failed
as expected. An intentionally zero-master fixed build also failed as expected.
The helper additionally rejects empty, all-zero and non-finite captures.

## Regression and sanitizer results

| Suite | Result |
| --- | --- |
| New cutoff unit test | **2,355 passed**, with strict ASan/UBSan |
| New cutoff integration | Four personalities × single/four-track dry captures; four four-track switching runs passed |
| Strict cutoff integration | Four four-track dry runs + four four-track switching runs passed under ASan/UBSan |
| Repaired GRAIN four-part audio fixture | **330 passed**, 32 sounding solo/spread/personality captures |
| GRAIN-only engine selection / legacy project policy | **380 passed** |
| GRAIN-only editor / backup policy | **70 passed** |
| Physical NOR / protected autosave / legacy staging | **24 passed** |
| GRAIN theme and shared sequencer | **8,229 passed** |
| Grid labels / values / native bounds | **1,935 passed** |
| Retained multi-engine UI, diagnostic build | **850 passed** |
| Retained editor, diagnostic build | **172 passed** |
| Project runtime, diagnostic build | **61 passed** |
| FUN11/FUN10 format and corruption, diagnostic build | **41,146 passed** |
| Presets, diagnostic build | **45 passed** |
| Backups, diagnostic build | **103 passed** |
| Repaired HOME levels fixture, diagnostic build | **90 passed**; feature-off variant **50 passed**, two-core variant passed |
| Native browser MIXER/CLOCK/SETTINGS audio helpers | All six old/new renderer executions passed ASan/UBSan and nonzero-output assertions |
| Editor capability/selection check | **20 passed** |
| Factory original checksums and reproducible ADPCM | All five verified; full-frame encoding unchanged |

The broader native suites above used ASan/UBSan with the **existing** intentional
DSP/XIP exclusions (`signed-integer-overflow,shift,bounds,object-size,pointer-overflow`).
No exclusions were added. The final cutoff unit and integration tests additionally
ran with **only shift checks disabled**: signed overflow, bounds, object-size,
pointer-overflow and address checks remained enabled, including the boosted-ramp
accumulator stress case. No sanitizer errors were reported in final validation.
The preliminary plain-gain failure was replaced by the safe dry-path calculation;
it is not an ignored final failure.

Cross-engine legacy suites explicitly use `FELUCCA_GRAIN_ONLY=0` to preserve
backend coverage. Shipping-policy tests use the default GRAIN-only behavior.
No test was skipped, weakened, or given a changed expected golden to make it pass.

Known baseline failures were reproduced with the exact `44ec460` source:

| Suite | Exact base | Fix | Failure-list comparison |
| --- | --- | --- | --- |
| Motion, diagnostic mode, ASan/UBSan | 97 passing / **27 failing** | 97 / **27** | Identical assertion names |
| MIDI, diagnostic mode, ASan/UBSan | 134 passing / **15 failing** | 134 / **15** | Identical assertion names |
| Broad Node web suite | 282 passing / **5 failing** | 282 / **5** | Identical assertion names |

These baseline failures remain unresolved; the task does not claim an entirely
green repository. No new failures were found. Earlier milestone MIDI counts
are not substituted for the actual exact-base comparison.

## Native size and resource impact

Matched Debian GCC 14.2.0 native objects, `cc -Os -ffp-contract=off -c
web/emu/felucca_web.c`, with identical generated assets and engine submodule:

| Section | Exact base | Fix | Delta |
| --- | ---: | ---: | ---: |
| text (including read-only assets) | 878,292 | 878,410 | **+118 bytes** |
| data | 31,060 | 31,060 | **0** |
| bss | 1,603,232 | 1,603,232 | **0** |

There is **no new persistent/static firmware state**, framebuffer, bitmap,
allocation, lock or thread. The dry loop uses automatic wide arithmetic
intermediates; target register/stack placement and multiplication cost are not
measured by native section totals. Tests' statistics, reference buffers and
harnesses do not compile into the target firmware.

These native objects include the browser NOR/display simulation; their sizes
are **not target firmware flash or RAM usage**. Normal build attempts stopped:

```text
build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)
emu: no emcc (Emscripten)
```

No tools were installed, no hardware was flashed, and no target or WASM build
was produced. Fit within the **581,564-byte image limit remains unverified**.
The newly active zero-cut path and its wide gain operations still require target
real-time/link validation. All other audited GRAIN limitations remain outside
this fix; saved CUTOFF=0 audibility is the intentional compatibility change.

## Reproduction and changed files

Prepared generated tables and the pinned engine submodule are prerequisites.
From this worktree:

```sh
# Shipping policy and dedicated zero-cut tests (existing sanitizer exclusions).
sh tests/run_grain_only.sh

# All nonzero cutoff values against a clean checkout of exact base 44ec460.
python3 tests/grain_cutoff_compare.py \
  --base-tree /absolute/path/to/clean-44ec460 \
  --out /tmp/coffee-grains-cutoff-comparison

# Fresh strict check of the new dry path, keeping signed-overflow checks active.
cc -O1 -g -w -ffp-contract=off -Ibuild/gen -Ifirmware/src \
  -fsanitize=address,undefined -fno-sanitize=shift \
  -fno-sanitize-recover=undefined tests/grain_cutoff_zero_test.c -lm \
  -o /tmp/grain-cutoff-zero-test
/tmp/grain-cutoff-zero-test
```

The comparison runner rejects an incorrect/modified base, requires positive
capture checks before every comparison, verifies original-base zero-cut rejection,
verifies deliberately zero-master rejection, and compares the unchanged UI trace.
Local validation logs/captures are outside the worktree at
`/workspace/work/cutoff-zero-validation`; they are not committed.

Changed files:

* `firmware/src/eng_grain.c`: only the CUTOFF=0 dry-output branch.
* `tests/audio_stats.h`: test-only finite/nonzero/peak/RMS validation.
* `tests/grain_cutoff_zero_test.c`: exact dry-gain/accumulation/ramp/reset/bounds tests.
* `tests/grain_cutoff_audio_test.c`: sounding mixer and transition harness.
* `tests/grain_cutoff_compare.py`: exact-base audio/UI comparison and negative controls.
* `tests/grain_only_audio_test.c`: nonzero master/assertions and all four personalities.
* `tests/home_levels_test.c`: nonzero master and sounding-capture assertions.
* `tests/run_grain_only.sh`: register the new unit and four-track integration checks.
* `web/emu/mixer_audio_test.c`: MIXER/CLOCK captured-output validation.
* `web/emu/settings_visuals_audio_test.c`: SETTINGS captured-output validation.
* `docs/grain-cutoff-zero.md`: this report.
