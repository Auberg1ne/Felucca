# Hybrid grain-envelope experiment

Base: `ae9d55541f26199e57813d8268c53b3e6265987e` (Experiment 4).
Branch: `experiment/coffee-grains-hybrid-envelope`.

## Result and recommendation

HYBRID is implemented, opt-in and default-off. Original A, previous B/C and
window/morph playback remain available unchanged. Recommend **threshold 128,
cap 8** for the next accuracy-first, on-device prototype, **not for shipping
without target profiling**. It exactly matches C over all tested 1–128-frame
lifetimes and uses lower-cost B8 above 128. Threshold 64/cap 4 is a useful
intermediate compromise. The initial 32/cap 8 candidate is not the most accurate.
No host timing result establishes which is optimal on FM1.

The optional one-frame adaptive optimization is independently PCM/state exact,
but measured speed changes are inconsistent. It remains separately disabled.
Do not silently replace legacy playback: improved short envelopes intentionally
change its PCM and can make previously silent fitted grains audible.

## Implementation and immutable policy

`firmware/src/eng_grain.c` adds a guarded include/dispatcher; the original
`gr_run` function and `grain_fidelity.h` are unchanged. New
`firmware/src/grain_hybrid.h` supplies the dispatcher and separate optimized
adaptive wrapper. Enable only in a test unity build with:

```
-DFELUCCA_GRAIN_WINDOW_EXPERIMENT=1
-DFELUCCA_GRAIN_MORPH_EXPERIMENT=1
-DFELUCCA_GRAIN_FIDELITY_EXPERIMENT=1
-DFELUCCA_GRAIN_HYBRID_EXPERIMENT=1
```

Per track, `gr_test_fidelity=3` chooses HYBRID;
`gr_test_hybrid_threshold` accepts 32/64/128,
`gr_test_hybrid_cap` accepts 4/8, and `gr_test_hybrid_opt` accepts 0/1.
Initial values are method **0 (A)**, threshold **32**, cap **8**, optimization **0**.
Methods 0/1/2 still delegate to the original A/B/C dispatcher. Invalid hybrid
threshold, cap or optimization safely falls back to A. A hybrid build without
the fidelity experiment is rejected at compile time.

These are **test-only, configured before playback** controls. Live method,
threshold or cap changes are unsupported; there is no public control/API or
persistent selector. For a fixed configuration the method remains stable for
an entire grain: classification uses immutable `winc=floor(UINT32_MAX/N)`,
not changing `left`. It selects C when `winc >= floor(UINT32_MAX/T)`.
For T=32/64/128 the floor values at T and T+1 differ strictly; monotonicity proves
the classification for all positive representable N. Tests exhaust N=1..1,000,000
and extreme uint32 durations, and verify the route after every rendered block.
No duration field or per-grain method field is needed. Existing window/morph
values remain independently latched at grain creation.

Long grains use the existing B subdivision policy with the chosen maximum cap.
Tail refinement uses remaining life as before, but does not switch the grain
from B to C as it ages. There are no new floating-point calculations, allocations,
source-decoder changes, divisions by variables, RNG calls or scheduling changes
in the callback. Minimum admitted duration remains unchanged (16 frames).

### Separate optimization

`gr_run_adaptive_fast` preserves B's fragment boundaries. A one-frame fragment
uses validated C(1) instead of calling the legacy endpoint-ramp kernel, whose
second endpoint/ramp slope is unused for that fragment. Larger fragments still
call unchanged `gr_run`. This removes redundant legacy calls and unused endpoint
lookups, without changing output, source decoding, phase, lifetime or cache state.
Optimization 0 remains the reference B path. Exhaustive tests compare both paths
against each other and an independently selected B/C route; the full private
part, including decoder/cache bytes, is compared with legacy one-sample calls.
It does not flatten/rewrite the preserved legacy kernel.

## Correctness and sanitizers

All runs below completed without reported sanitizer errors or test failures.

| Check | Result |
|---|---:|
| Strict numerical/kernel assertions | **32,824,528 passed** |
| Kernel rendering cases | **985,800 passed** |
| Short matrix | 1..128 frames × 32 offsets × 2 directions × 12 window representations × 10 policies = 983,040 |
| Long static-envelope cases | 600 |
| Moving signed-source/rate cases | 1,920 |
| Full long real-SYNTH decoder/cache cases | 240 |
| Window/morph latching | **365,256 assertions**, 24 configurations |
| Complete final-output audio executions | **7,092 passed** |
| Base/compiled-out/runtime-A identity cases | **832**, three builds each |
| Previous B4/B8/C base identity cases | **48**, two builds each |
| Hybrid optimized/reference audio pairs | **2,208**, two renders each |
| Listening renders included in audio total | 84 |

The strict kernel uses **ASan + UBSan, `-fno-sanitize-recover=all`, no sanitizer
exclusions**, including signed-overflow checks. It covers all three discrete
windows plus nine morph positions, long durations through 86,432 frames, signed
source endpoints, rates through 2×, reverse boundaries with >1,000 refill events,
output canaries, invalid control bytes and resource-size assertions. Entire
`gr_part_t` state equals the per-sample reference after each call; PCM equals C
where C is selected, and equals independently routed B otherwise. B boundary
samples agree with the per-sample reference. Optimization PCM is exact in every
kernel case (also B4/B8 directly, including very short cases).

The integration/latching fixtures retain Experiment 4's existing exclusions:
`-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow`.
They do **not** provide strict validation of unrelated historical mixer code.
No exclusion was added for the hybrid; its kernel is separately strict. Audio
fixtures explicitly use nonzero master, test sounding output, track-level signals
and canaries. All four personalities, all five factory IDs 0/2/3/13/14, synthetic
user audio, sine fallback, forward/reverse playback, high density, short fitted
sources and four simultaneous tracks are covered. All 128 morph controls have
base A identity coverage; anchor/intermediate cases cover the source matrix.
The latching matrix covers all 128 morph values, invalid selectors, independent
parts and long-B/short-C routes under every threshold/cap/optimization combination.

No UI, project, MIDI or preset code changed. Those broad historical suites were
not rerun; their pre-existing failures are neither claimed fixed nor counted as
new failures here. Strong disabled-path evidence includes byte-identical host
browser unity objects and fixture objects against the exact base. No unrelated
golden/snapshot was regenerated.

## Envelope fidelity

[Grouped numerical measurements](data/grain-hybrid-envelope/envelope-summary.csv)
retain duration, window and policy. Each policy below has 98,304 short cases.
Error is RMS of per-case normalized RMS errors against C, not relative loudness
error; input is held +32767 to isolate the envelope. Both playback directions
and all parent-block offsets are included.

| Method | Silent cases | Aggregate error RMS | Worst per-case error RMS |
|---|---:|---:|---:|
| A | 12,672 | 0.322244 | 0.865964 |
| B4 | 768 | 0.010213 | 0.044304 |
| B8 | 768 | 0.015102 | 0.045972 |
| C | 768 | 0 | 0 |
| H32 / 4 | 768 | 0.008641 | 0.044304 |
| H32 / 8 | 768 | 0.014087 | 0.045972 |
| H64 / 4 | 768 | 0.003117 | 0.011954 |
| H64 / 8 | 768 | 0.011554 | 0.045972 |
| H128 / 4 or 8 | 768 | 0 | 0 |

All 768 remaining silent cases per improved method are **one-frame grains**:
the preserved phase convention samples the zero starting endpoint. Two-frame
grains can sample the interior, but their waveform and jumps remain coarse;
there is no claim of meaningful general-purpose 1–2-frame audio. Durations below
admission minimum are kernel vectors, not a change to admission policy. A still
exhibits its known single-block silence when both evaluated endpoints are zero.
Crossover changes do not imply a new discontinuity inside an active grain; the
route is fixed. Cross-policy output differences, envelope interpolation error,
peaks and adjacent-sample jumps are retained in the numerical/audio CSVs.

## Listening material

[Listening ZIP: 168 WAVs](audio/grain-hybrid-envelope-listening.zip) contains all
four personalities, normal GUITAR source 3 and fitted 35/131-frame synthetic user
zones, A/B4/B8/C/H32/H64/H128 (hybrids cap8, optimization1). Each has raw and
offline RMS-matched variants: 44.1 kHz, stereo PCM16, 131,072 frames. Identical
seeds, note events and parameters are used within each comparison. Original
factory/user source assets were not modified. Generated test user audio is not
a replacement sample asset.

ZIP SHA-256: `57db8879b1a44fb1d45ec8b5c9c48b4bf48cb199079df9de3021880aeb25407c`.

[Listening gains/status](data/grain-hybrid-envelope/listening.csv) and
[render peak/RMS/energy/jump/direction data](data/grain-hybrid-envelope/audio.csv)
are included. Matching is offline only, with a common target at most -24 dBFS RMS
and 95% peak headroom. Four A/short35 captures are silent and explicitly marked
`silent_unmatchable`; their matched versions remain silent. Gains range up to
94.89× for quiet short fixtures, which also magnifies integer quantization. RMS
matching is not perceptual loudness matching. Measurements establish fidelity
and activity, **not subjective sound quality**.

## Operation counts and host timing

[Microbenchmark, 4,480 rows](data/grain-hybrid-envelope/host-bench.csv): Debian GCC
14.2.0, x86-64, `-O2 -ffp-contract=off`, no sanitizers, process CPU clock, five
repeats of approximately 262,144 frames/case. Includes common grain reset/initial
seek, decoder and checksum work. Four windows (Gaussian/Hann/Tukey anchors and
morph96), both directions, seven durations, ten reference policies plus six
optimized hybrids. Repeat variability reaches **5.89×** on this shared host;
small timing differences are not reliable. Each row also records source-derived
window evaluation and table lookup counts, not hardware performance counters.

Window evaluations per complete grain, initial offset zero (optimization OFF):

| Frames | A | B4 | B8 | C | H128/8 |
|---:|---:|---:|---:|---:|---:|
| 16 | 1 | 31 | 31 | 16 | 16 |
| 32 | 1 | 33 | 33 | 32 | 32 |
| 64 | 3 | 35 | 35 | 64 | 64 |
| 128 | 7 | 67 | 37 | 128 | 128 |
| 129 | 9 | 69 | 39 | 129 | 39 |
| 441 | 27 | 225 | 117 | 441 | 117 |
| 4410 | 275 | 2209 | 1109 | 4410 | 1109 |

At an anchor each evaluation uses one table interpolation; intermediate morphs
use two plus a blend. The optimization removes unused second endpoint evaluations
for nonterminal one-frame B fragments. It does not reduce decoder work. Across
paired host microbenchmarks optimized/reference median ratios range 0.990–1.007;
there is no consistent general speedup worth enabling by default.

Example forward morph96 median host CPU ns/output frame:

| Frames | A | B4 | B8 | C | H128/8 |
|---:|---:|---:|---:|---:|---:|
| 16 | 10.64 | 24.83 | 23.45 | 13.71 | 14.56 |
| 32 | 8.83 | 16.13 | 17.41 | 11.32 | 11.52 |
| 64 | 7.96 | 12.42 | 12.37 | 11.80 | 11.38 |
| 128 | 8.81 | 11.78 | 9.44 | 11.93 | 13.17 |
| 129 | 8.77 | 11.52 | 10.88 | 13.81 | 10.96 |
| 441 | 7.99 | 11.30 | 12.47 | 11.82 | 10.08 |
| 4410 | 8.90 | 12.37 | 10.28 | 13.45 | 10.10 |

### Four-track load

[256 pool stress rows](data/grain-hybrid-envelope/host-pool-load.csv) exercise four
parts, 48 total long grains at 0.25×, and decode-budget-constrained 2× profiles
(16 forward or 8 reverse grains). All anchors/intermediate, directions, policies
and optimization states are measured over 1,024 blocks. This is a **kernel-only
capacity proxy**, manually initialized, not proof all phases are simultaneously
reachable through the scheduler. Long steady-block window evaluations at 48
grains: A **96**, B4 **768**, B8/HYBRID **384**, C **1,536**. Intermediate morphs
double table interpolations. A conservative full-capacity hybrid ceiling is
1,536 envelope evaluations/32-frame parent block (3,072 table interpolations),
plus unchanged decode/mixer work. Admission/lifetime constraints can lower the
reachable instantaneous total. Window selection changes no decoder load.

[64 full mixer benchmark rows](data/grain-hybrid-envelope/host-mix.csv) use four
actually sounding tracks, FREEZE and SHIMMER, density127, low forward pitch or
fast randomized/reverse-pressure pitch, morph96, 2,048 warmup and 2,048 timed
blocks. **Each policy starts in a fresh process**: `ui_power_on` alone is not a
full reset of every mixer/DSP static. After correcting that benchmark fixture,
all policies have identical grain/head/RNG hashes and occupancy in each workload.
Max occupancy is 48 in slow profiles and 16 in fast profiles; mean occupancy is
47.8145/15.8105 (FREEZE), 47.3203/12.7207 (SHIMMER). Master/output are nonzero.
Effects/sends are disabled, so this is not the complete worst-case device load.

For H128/8 optimization0 the measured host CPU cost is 335.40/280.53 ns/frame
(FREEZE slow/fast), 373.56/286.16 (SHIMMER); corresponding A values are
238.83/396.69 and 314.05/255.55. The inverted fast FREEZE ranking and large
outliers reinforce the host-noise caveat. Per-row p99/max block times are supplied;
none is a device deadline guarantee. Target four-track/effects/reverse profiling
is required before any shipping decision.

## Resources and stack

Same new audio harness against the exact base/current source,
`cc -Os -ffp-contract=off -fstack-usage`:

| Build | text/rodata | data | BSS |
|---|---:|---:|---:|
| Base, previous experiments enabled | 537657 | 19676 | 310568 |
| Current, hybrid compiled out | 537657 | 19676 | 310568 |
| Current, hybrid enabled | 538276 | 19676 | 310568 |
| Enabled delta | **+619** | **0** | **0** |

Base/current compiled-out fixture objects are byte-identical. Native browser
unity objects with the same previous experiment flags and hybrid disabled are
also byte-identical: text886991, data31060, BSS1603232. These are host objects,
**not target ELF/image sizes**. No table or bitmap was added. Twelve logical
test-control bytes (three four-track arrays) are added only when hybrid is
compiled in; unchanged aggregate data/BSS reflects host layout/compiler effects,
not absence of those controls.

Grain **64 bytes**, private part **7296**, common track **1840**, reservation
**12888/track**, **51552 total**, unchanged/asserted. No persistent audio buffer,
framebuffer, dynamic allocation or record-layout change. Existing target ABI
record sizes should be unchanged but were not measured with a target compiler.

Individual host stack frames: `grain_render` **344→304**, unchanged `gr_run`
**120**, newly outlined `gr_run_fidelity` **72**, `gr_run_sample` **112**,
`gr_spawn_one` **112** unchanged. A smaller caller frame is **not** a whole-chain
stack reduction: the enabled unoptimized B chain can sum 304+72+120 versus
344+120 before common decoder helpers (+32 bytes in that portion). Optimized B
avoids the fidelity wrapper on that route. Full target call-chain/interrupt/CPU2
stack and ABI costs remain unverified.

Target preflight, unchanged `./build.sh`:

```
build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)
```

No toolchain installed/downloaded, target link, hardware timing, flash or deployment
occurred. Fit within **581,564 bytes** is not established. Host +619 bytes is not
a valid substitute for pi32v2 linked flash/stack/RAM measurements.

## Reproduction and changed files

With a clean base worktree at the exact base SHA:

```sh
python3 tests/grain_hybrid_compare.py \
  --base-tree /path/to/clean-ae9d555-worktree \
  --out /tmp/grain-hybrid-results-repro --bench
```

This builds/runs strict kernel CSV, 24 latching configurations, identity/optimized
pairs, 168 WAVs, resource objects and all three host benchmarks. Existing generated
factory headers must be present as for the base host tests. `CC` may select the
host C compiler. The final kernel controls/default checks were also run separately
with the same strict flags; the initial full audio-run log predates those extra
checks. Reported final strict total is the later 32,824,528-check run, not the
initial 29,823,760-check total.

Grouped CSVs in this report collapse kernel offset/direction rows by
length/window/method/cap/threshold (count, silence, RMS of normalized errors,
maximum error/jump). Raw CSVs remain reproducible outside Git. Listening ZIP uses
sorted WAV names, DEFLATE9 and fixed 2026-01-01 ZIP timestamps; no binary executable,
object, cache or raw multi-megabyte kernel CSV is committed.

Production files: guarded dispatcher in `eng_grain.c`, new `grain_hybrid.h`.
New tests: `grain_hybrid_{audio,kernel,latch,load}_test.c`,
`grain_hybrid_{bench,mix_bench}.c`, `grain_hybrid_compare.py`.
Documentation: this report, six measurement CSVs and the listening ZIP. Original
A/B/C code, windows, source assets, UI, controls and storage formats are unchanged.

Next: profile H128/8 and H64/4 on FM1 with all effects/CPU2 modes, inspect complete
stack, and listen to matched/raw captures before choosing a fidelity mode. Any
future live method control requires explicit per-grain latching or handoff design;
this experiment deliberately provides no such control. No merge, deployment or
flashing.
