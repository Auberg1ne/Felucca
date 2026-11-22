# STUTTER gate-edge smoothing — Step 239

Base: `f0be0148acd39e27fe772e028cdc72630bdd7066` on
`feat/coffee-grains-granular-quality`.

## Cause and focused correction

In `grain_render`, STUTTER previously left the accumulator untouched while
`phase < on` and immediately set it to zero otherwise. The opening at phase 0
and closing at phase `on` could each jump by the complete source amplitude.

The period and opening remain exactly:

```
period = 128 + (127 - RATE) * 12
on = period * GATE / 127
phase = (voice.ph[1] + sample_offset) % period
```

The new `gr_stutter_gate` applies a symmetric, phase-derived envelope entirely
inside that original opening. No history, extra DSP state or new control exists.
For a partial gate:

```
fade = max(2, min(64, ceil(on / 2), period - on))
distance = min(phase + 1, on - phase)  // inside the opening only
step = ceil(65536 / fade)
gain = distance < fade ? distance * step : 65536
```

At most 64 samples (about 1.45 ms at 44.1 kHz) are used per edge. Short openings
use shorter triangular ramps. Short closed intervals also cap the ramps so a
nearly continuous gate does not become a deep, cycle-long swell. One-/two-sample
openings retain ideal half gain. Signed-symmetric nearest rounding keeps even
constant +/-1 sample pulses from vanishing entirely. GATE 0 is exact silence;
GATE 127 is an exact, uninterrupted unity bypass.

Gain never exceeds unity. The ramp product uses int64, bounded below 2^47 for
all int32 source values; adding the half-LSB rounding bias remains safe. Constant
division by 65536 implements nearest rounding, with ties away from zero. It adds
no polarity-dependent DC offset. The variable reciprocal is computed once per
block, not per sample; the phase modulo was already present in the old loop.

The voice phase increment is unchanged, including its existing uint32 wrap
semantics. Grain decoding/scheduling still runs while the gate is closed.
Filter/TONE ordering is unchanged: smoothing precedes them, just as the hard
gate did. Their signal-dependent states can naturally differ for STUTTER audio;
this is distinct from decoder, admission and scheduling state.

## Timing and tests

`tests/grain_stutter_gate_test.c` passes **42,069,041 checks**:

- All 128 RATE x 128 GATE settings (16,384 combinations), across complete cycles.
- Exact opening support/count, unchanged period, symmetric envelope centroid,
  unity bound, zero/full gate behavior and no all-zero opening for a nonzero
  constant source (including +/-1 at its center).
- Amplitudes 0, +/-1, +/-2, +/-32767, +/-393216, INT32_MAX and INT32_MIN.
  Wide-reference rounding, signed symmetry and magnitude bounds.
- Every block split 1..32 at rate extremes/all gate settings; starts around gate,
  block and uint32 boundaries. No stale samples or output-canary corruption.
- Actual grain_render phase advancement at every RATE/GATE setting with three
  starting counters, including uint32 wrap: RNG, grain lifetime/fraction and
  spawn countdown remain unchanged by gating.

The mask duty fraction remains `floor(period * GATE / 127) / period`. Opening
start/end and its centroid do not move. Fade overlap cannot accumulate: the
minimum-distance envelope is stateless. Edge attenuation intentionally reduces
energy; there is no compensating gain boost. This preserves timing, not the
old rectangular gate's exact RMS. For an arbitrary source, amplitude modulation
can still change its average; signed symmetry does not promise zero DC for every
possible input waveform.

For constant input 16,384, maximum adjacent-sample jumps over repeated cycles:

| RATE | GATE | Period / open samples | Fade samples | Before | After |
|---:|---:|---:|---:|---:|---:|
|0|1|1652 / 13|7|16384|2341|
|0|64|1652 / 832|64|16384|256|
|0|126|1652 / 1638|14|16384|1171|
|127|1|128 / 1|2|16384|8192|
|127|64|128 / 64|32|16384|512|
|127|126|128 / 126|2|16384|8192|

Zero/full gate settings have no recurring transition before or after. Across
all partial gates, the constant-source jump is at most half the former full
step. The shortest intervals cannot contain a 1–3 ms fade without changing the
rhythm; their bounded shorter ramps are deliberate.

## PCM, state and compatibility evidence

Fresh seeded processes were compiled against an archived exact-base firmware
and the corrected source, with the same generated inputs and pinned submodule.
All 24 granular captures have identical RNG draw counts, scheduling/admission
observations, decoder-load maxima and explicit decoder/head state hashes.

Fifteen captures (CLOUD, FREEZE and SHIMMER without a mixed STUTTER track) remain
byte-identical. Nine audio captures intentionally change: all six STUTTER
captures plus the other three mixed-personality four-track captures. The old
JSON golden is unchanged. `granular-quality-stutter.json` explicitly pins only
PCM SHA-256, peak, RMS and nonzero counts for those nine cases; the runner checks
its exact affected-case set and never overrides state or storage expectations.

Single-track STUTTER maximum PCM jumps, in final int32 mixer units:

| Capture | Before | After |
|---|---:|---:|
|low-density|4573|2549|
|high-density|5202|1867|
|minimum-size|12103|4760|
|reverse|5371|1686|
|mod-filter|4538|2927|

STUTTER high-density RMS changes from 945.771365 to 856.108375 as expected from
softening edges. Meaningful nonzero output is required in every capture.
Mixed-output maxima need not decrease monotonically: SHIMMER/four-track changes
from 11769 to 11783 (+14), while all individual non-STUTTER captures remain exact.
This is a mixed waveform statistic, not evidence of a newly hard gate edge.
Every current capture peaks below 32767 (maximum 19771), asserted by the runner.
No new numerical clipping/overflow is introduced; the gain path is bounded and
has no boost. Existing downstream filters/mixer limiting remain authoritative.

Strict ASan/UBSan passes both the new gate test and Step 238's 152,924,099-check
short-envelope gate without exclusions. The 306 -> 0 short-grain correction
remains intact. Strict filter-transition testing passes 3,553,312 checks with
worst first edge 0. Full-mixer fixture sanitizers retain their documented legacy
signed-overflow/shift exclusions; those exclusions are not used for the new gate.

Twenty relevant existing shipping suites: 15 pass. The same inherited problems
remain: Editor has 11 failed assertions then SIGSEGV; Motion has 30; robustness
has 3; STRETCH stops at its non-GRAIN descriptor assertion; MIDI's shipping test
fails compilation because ANALOG_PRESETS is absent. No unrelated assertion or
golden was changed, and these are not new regressions. The existing 5,804-check
persistence/NOR and user-preset gates pass. IDs, defaults, formats and stored
preset values are unchanged; saved STUTTER sounds intentionally have softer edges.

## Host resources and performance

Same Step 238 harness, compiler and `-Os -fstack-usage`:

- `.text` +163 bytes; GNU size text aggregate 507698 -> 507861.
- `.rodata`, `.data`, `.bss` and reserved engine pool: no change.
- Host grain/part structures remain 64 / 7296 bytes. No persistent fields/buffers.
- Gate helper inlined; grain_render stack remains 248 bytes, unchanged.

O2 gate-loop benchmark, median of five repetitions, includes buffer fill and
observation overhead; these are host process-CPU times, not FM1 timings:

| RATE/GATE | Before ns/sample | After ns/sample |
|---|---:|---:|
|0/0|1.367|0.381|
|0/64|1.355|1.591|
|0/126|1.352|1.685|
|127/1|1.413|1.477|
|127/64|1.449|1.817|
|127/126|1.464|1.770|
|127/127|1.361|0.331|

Partial-gate loop cost increases roughly 17–25% in these representative
half/high-duty cases; muted/full gates become cheaper. This is not a whole
callback percentage. Worst case is bounded by 12 extra reciprocal calculations
and 384 scaled samples per CTL32 callback (four parts x three voices). There are
no allocations, locks, tables, extra decoder reads or unbounded loops.

The existing WIN four-track mixer benchmark was copied outside Git and its
`GA=GB=127` setup changed to `GA=GB=64` so STUTTER exercised a partial gate.
The same probe was compiled against archived base/current firmware: 120 captures,
up to 48 active grains. All 120 scheduling hashes and all 90 non-STUTTER peak/RMS
pairs agree. Median STUTTER time was 154.974 -> 154.178 ns/frame; other personality
medians varied by less than 0.5%. Treat this as host timing noise, not a speedup
or a worst-case FM1 guarantee. The per-sample gate benchmark exposes the added
edge work independently of decoding/mixing costs.

## Reproduce and limitations

```
python3 tests/run_granular_quality.py --sanitize --out /tmp/stutter-validation
cc -O2 -w -Wno-implicit-function-declaration -ffp-contract=off \
  -Ibuild/gen -Ifirmware/src tests/grain_stutter_gate_test.c -lm \
  -o /tmp/stutter-bench
/tmp/stutter-bench --bench > /tmp/stutter-bench.csv
```

The original short-grain legacy oracle and all storage checks remain mandatory.
The new gate test is also mandatory in this runner. No device/UI/sample/build
changes were made. No push, merge, release, toolchain install or flashing.

FM1 callback headroom, target stack and linked firmware fit remain unverified
without a usable JieLi target toolchain and hardware profiling. Hardware audition
is still needed, especially for very narrow or nearly full openings. Abrupt live
RATE/GATE changes, intentional GATE=0 mute, and the inherited uint32 phase wrap
can still cause discontinuities: this stateless correction smooths recurring
edges without inventing a new parameter slew policy or changing rhythmic phase.
