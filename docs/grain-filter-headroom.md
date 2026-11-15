# GRAIN resonant-filter arithmetic safety (F9)

## Base and scope

Base: `ba89712fdfe56cc1f56402865b0739024fbf29b5`.
Audit reference: `0883922bcfc7106242914ba2b94e7de4aa1ef7a1`, F9 only.

This is a local output-arithmetic correction in `firmware/src/eng_grain.c`,
not a filter redesign. Shared `voice_amp`, `mulq15`, SVF helpers/coefficient
mapping, other engines, sample assets, parameter IDs/defaults, presets, saved
formats, UI and MIDI/editor routes are unchanged. No persistent state is added.

## Reproduced root cause before production changes

The old nonzero-CUTOFF output was:

```
out[i] += voice_amp(y, m, i) << 1;
voice_amp(y, m, i) = mulq15(mulq15(y, amp_at(m, i)), VOICE_FS);
mulq15(a,b) = (a*b) >> 15;             // signed int32 product
```

SVF input is limited to ±65535, but resonance can produce a larger output `y`.
The first signed product `y * amp_at(...)` can therefore overflow even at unity
amplitude. Widening only that first product would expose the second product
(`gained * 24000`) to overflow on sufficiently large corrected values. The
negative-output `<<1` also has undefined C behavior. A final `out += wet` can
overflow if the caller's int32 buffer is already near its representable limit.

Before editing production code, `tests/grain_headroom_repro.c` drove the actual
GRAIN render path with three synthetic, normalized, bounded grain contributions,
CUTOFF=25, RESON=127, TONE=127, amplitude=32767. Strict UBSan stopped at:

```
firmware/engines/src/dsp.c:8:64: runtime error: signed integer overflow:
66091 * 32767 cannot be represented in type 'int'
```

The input samples are bounded by signed 16-bit full scale; three gains use
`GR_NORM[3]`. The driver holds a synthetic window while applying a sinusoidal
signal near the filter center. This is an admissible bounded arithmetic stress
fixture, not a claim that a stock preset or recording provides that exact drive.
After the fix, the same driver completes: filter peak **153640**, direct voice
output peak **225050**, RMS **161983.702927**, **44099 nonzero samples**.

The exact-base comparison runner also requires the old reproduction to trap
and the new one to succeed. It cannot silently treat failure to reproduce as a
passing baseline check.

## Safe arithmetic and exact rounding

A GRAIN-local `gr_output` helper implements:

```
gained = ((int64)sample * amp_at(m,i)) >> 15;
wet = ((gained * VOICE_FS) >> 15) * 2;
sum = (int64)previous_out + wet;
result = saturate_to_int32(sum);
```

Both existing Q15 rounding stages are retained separately. They are not merged
into one product or rounded differently. Negative right shifts retain the
existing target/compiler arithmetic-floor convention, checked against an
independent division-based reference. Scaling by two is now defined wide
multiplication instead of a signed left shift.

**Saturation occurs only on the final accumulated buffer sum**, at
[-2147483648,2147483647]. There is no new clipping of SVF output, intermediate
gain, input amplitude or filter state. These limits affect only sums that were
previously unrepresentable; ordinary audio does not reach them. Keeping wide
intermediates until after the sum also avoids premature saturation before a
caller contribution can cancel it.

The same helper is used in CUTOFF=0's existing dry path. Its ±65535 input limit,
amplitude ramp, two rounding stages and state reset are unchanged. The only
new dry-path behavior is defined saturation of an otherwise overflowing final
buffer sum. Existing CUTOFF-zero tests remain unchanged and pass.

No per-sample division, coefficient work, new audio buffer or allocation is
introduced. The required costs are wide products/shifts, wide accumulation and
final range checks.

## Legal/stress ranges and internal-filter bounds

The common ramp helper remains unchanged. Tests cover every integer endpoint
0..65527 in rising-from-zero, falling-to-zero and steady ramps, plus bounded
negative-ramp stress cases. This includes ordinary unity envelopes and the
boosted/stress range retained by the earlier CUTOFF-zero repair. With endpoints
in ±65527 and i=0..31, the common ramp's difference-times-index fits int32.
No amplitude value is newly clamped.

The output helper is tested with both actual/conservative filter magnitudes
and full int32 sample extremes. With a full int32 sample and |amp|<=65527,
the first product is below 2^47 in magnitude; after its Q15 shift, the next
product is also far below int64 limits. The doubled scaled value and addition
to any int32 previous buffer value fit int64 before final saturation.

The existing integrators are clamped to ±150000. Given input ±65535, their
`v3 = input - ic2` is bounded by ±215535. For every integer CUTOFF -50..50
and every RESON 0..127, the interval bounds on the two shared SVF product sums
are at most **1596364365** and **1822617210**, both below INT32_MAX. Temporary
integrator updates also fit int32 before the original state clamps.

`bp=(new_ic1+old_ic1)>>1` stays within ±150000; its largest damping product is
`8192*150000=1228800000`, also safe. LP/HP output arithmetic fits int32 under
these state/input bounds. Thus widening/replacing the common SVF is unnecessary.
Observed corner-state LP/HP peaks in the bounded sweep are 191218/513850;
these observations are supplemented by conservative interval proofs, not used
as a guessed runtime clipping threshold.

## Wide-reference numerical verification

**10974406 assertions pass under ASan+UBSan with no exclusions**, including
signed-overflow and shift checking:

- Every ramp endpoint 0..65527, selected ramp sample positions, extreme sample
  values, and previous buffer values near both int32 limits.
- **3602246** cases where old narrow products and sum fit, compared directly
  with the unchanged `voice_amp` result and defined wide ×2 scaling. Of these,
  **2456591** also have nonnegative old output scaling and were fully defined
  by the old expression. **1145655** have negative output: the old `<<1` was
  undefined, but its intended integer result is preserved exactly.
- **3474778** cases with an old overflowing product or sum, matched to an
  independent `__int128` reference using division-based floor rounding.
  **1762447** stress sums require final int32 saturation. These include full
  int32 helper inputs and near-limit buffers, not just normal filter signals.
- Coefficient/state-range proofs across all accessible CUTOFF values and all
  128 resonance values, plus all signed state/input box corners.
- **56560 actual GRAIN render cases** across all four personalities, CUTOFF
  -50..+50, RESON 0/127, positive/negative accumulator extremes, zero and
  ±150000 integrator states, ramp extremes, and near-limit output buffers.
  Lengths include 0,1,17,32 and requests above CTL; canaries stay intact.
- **4096 live CUTOFF/RESON transitions** carrying real filter state through
  dry reset and HP/LP changes, compared sample-by-sample to the wide reference.

The reference does not copy the production signed-shift implementation: it
uses wide division/floor operations and explicit final saturation. Old helper
calls are made only after proving their products fit.

## Actual nonzero-output audio comparisons

**1168 strict exact-base comparisons** cover four personalities, all five
factory sources, a legal user zone and sine fallback; TONE 0/64/127; CUTOFF
-50/-25/0/25/50; RESON 0/127; four-track mixing; high-velocity fixtures;
UNISON/DTUNE and live filter sweeps. Every successful capture explicitly sets
master=4096 and verifies finite peak/RMS and meaningful nonzero sample count.

**1166 complete captures are byte-identical**, comparing **305659904 PCM bytes**.
This includes every ordinary static control, **228** fixed dry-bypass controls
and **606** valid TONE=127 captures.
TONE=127, existing TONE shaping, DTUNE, source position and scan behavior retain
their ordinary audio results. No golden files were regenerated.

Two programmed rapid-control sweeps trap on the exact base and complete safely
on the fix. Both use FLUTE (source 2), TONE=127, four sounding tracks and
CUTOFF traversing -50..50 with RESON alternating 0/127 every 32-sample block:

| Personality | Base overflowing product | Fixed final-mix peak / RMS |
| --- | --- | --- |
| CLOUD | 92645 × 25598 | 22199 / 3645.891595 |
| FREEZE | -85007 × 25598 | 22132 / 3342.230104 |

A captured UBSan stack confirms `mulq15 → voice_amp → grain_render →
track_render → mix_part → mix_block`. These are custom stress trajectories
using a factory asset, not unchanged stock presets. There is no valid complete
old PCM reference after undefined arithmetic; the fix is judged against the
wide numerical model and bounded, nonzero output, rather than an overflow-wrap
golden. The baseline traps are explicitly recorded, not accepted as ordinary
passing audio or hidden by sanitizer exclusions.

The audio comparisons keep signed-overflow, bounds, object-size and
pointer-overflow checking enabled. Only historical **shift** idioms elsewhere
in the mixer are excluded (`-fno-sanitize=shift`). The new output path and
bounded reproduction additionally pass the no-exclusion numerical/reproduction
runs. Other engines' shared amplitude helpers are deliberately unchanged.

## Retained regressions

The registered suite completes, including the no-exclusion reproduction/unit,
348 additional signed-overflow-checked headroom smoke renders and all retained
CUTOFF, position, TONE, DRIFT and DTUNE tests. Passing checks: GRAIN-only
selection/legacy protections 380, editor/storage 70, persistence 24, four-track
audio 330, CUTOFF-zero 2355, source position 854920, TONE 722655, CLOUD DRIFT
2315750, UNISON DTUNE 75537, theme 8229, grid visuals 1935, STRETCH descriptors
1764, GRAIN grid 2329 and FUN11/FUN10 project formats 41146. Full UI (850),
editor (172), project (61), preset (45) and backup (103) assertions pass.
Focused web policy passes 20 checks.

All **270 retained UI state/pixel snapshots are byte-identical** to the exact
base's validated artifact. No UI source or snapshot golden changes.

Separately classified historical failures: MIDI 134 passing / 15 failing;
motion 97 passing / 27 failing, with identical failing assertion names on base
and fix. Serial web is 282 passing / 5 failing, matching the exact base's
validated serial result and failure names. The parent report's additional
WATCH timing flake was not observed in this serial run.

These broader legacy native suites retain their historical sanitizer exclusions
for signed overflow, shift, bounds, object-size and pointer-overflow. They are
not substituted for the stricter F9 tests above. No new sanitizer diagnostics
or deterministic regression failures were observed.

## Native size, engine RAM and stack

Identical GCC 14.2 native `-Os -ffp-contract=off` compilation of
`web/emu/felucca_web.c`:

| Section | Base | Fix | Delta |
| --- | ---: | ---: | ---: |
| text | 879053 | 879148 | +95 bytes |
| data | 31060 | 31060 | 0 |
| BSS | 1603232 | 1603232 | 0 |

Native `grain_render` frame remains 328 bytes. The compiler outlines the new
output helper with an 8-byte individual frame; `gr_spawn_one` remains 112.
These are compiler frame measurements, not target worst-case stack analysis.
No persistent state or engine allocation grows: private GRAIN remains 7296
native bytes/part inside 12888 reserved (5592 spare); all four fixed regions
remain 51552 bytes. The earlier GCC 32-bit layout estimate remains 7200 per
part (5688 spare), since no structure changed.

## Reproduction and remaining limitations

```sh
sh tests/run_grain_only.sh
python3 tests/grain_headroom_compare.py --base-tree /path/to/ba89712 --out /tmp/grain-headroom-compare
cc -O1 -g -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=undefined -Ibuild/gen -Ifirmware/src tests/grain_headroom_test.c -lm -o /tmp/grain-headroom-test
/tmp/grain-headroom-test
```

Normal generated tables and the pinned engine dependency must already exist.
The registered headroom unit/reproduction tests do **not** inherit the legacy
signed-overflow exclusions. `GRAIN_HEADROOM_BASE_TREE` enables the exact-base
matrix from the runner. Its baseline reproduction must fail specifically at
the old gain multiply and its fixed reproduction must complete.

This does not redesign resonance, add output normalization or make shared
helpers safe for unrelated engines. Legal GRAIN state bounds and existing
input/state clamps remain the contract. Other audited DSP limitations and all
previous parameter mappings/lifecycles are unchanged. The safety result is for
bounded legal/stress inputs, not arbitrary corrupt filter-state memory.

Emscripten/JieLi tools are unavailable; nothing was installed or flashed.
Native size is not a target link. Hardware image fit within 581564 bytes and
actual target CPU/stack cost remain unverified until a target build succeeds.

## Changed files

- `firmware/src/eng_grain.c`: local wide gain/accumulation helper in wet/dry paths.
- `tests/grain_headroom_repro.c`: bounded before/after actual-render reproduction.
- `tests/grain_headroom_test.c`: wide gain/filter/render references and bounds.
- `tests/grain_headroom_audio_test.c`: actual sounding source/filter/mixer profiles.
- `tests/grain_headroom_compare.py`: strict exact-base comparisons and F9 traps.
- `tests/run_grain_only.sh`: strict tests, smoke matrix and optional base comparison.
- `docs/grain-filter-headroom.md`: this report.
