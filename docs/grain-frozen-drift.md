# Audible FROZEN DRIFT — Task243B

Base `1d0e3ae47488a058ffeda00835dbc0cf518637d8`, branch
`feat/coffee-grains-granular-quality`. Uses the complete243A ownership/lifecycle
contract. No new state, anchor, persistent format, editor/MIDI route, UI, asset or
SPREAD implementation. Local commit only; no push, release, merge or hardware flash.

## Algorithm and bounds

Only FROZEN/TYPE1 uses the existing per-voice `gr_drift_t`. Its Q32 phase still
advances4869 units/output frame when C>0, about20.002371876 seconds/turn at44100Hz.
At C0 phase pauses. For each eligible render fragment (n<=32):

```
c       = clamp(C,0,100)
phase  += c ? 4869*n : 0                    // uint32 wrap
q15     = floor(c*c*32768 /10000)
target  = trunc(sine_i(phase)*q15 /512)      // signed Q24
current = clamp(displacement,-2^21,+2^21)
next    = current + clamp(target-current,-96*n,+96*n)
```

`sine_i` is the existing1024-point interpolated sine, not a generated table or
user LFO. All positive C values retain the same period; quadratic depth provides
low-control resolution. No RNG is used. New grain creation converts the owned
Q24 displacement to source frames, then applies the existing forward/reverse
position bounds after existing POS/SHP/MOTION positioning:

```
offset = trunc(clamp(displacement,-2^21,+2^21) * zone_frames /2^24)
room   = max(zone_frames-3-span,0)
offset = clamp(offset,-floor(room/8),+floor(room/8))
start  = existing_position + offset
start  = existing_forward_or_reverse_span_clamp(start)
```

| Control | Maximum normalized displacement | Further window constraint |
|---|---:|---|
|0, never activated/settled |0, exact legacy placement |0 |
|25 |±0.78125% of zone |at most±room/8 |
|50 |±3.125% |at most±room/8 |
|100 |±12.5% |at most±room/8 |

Integer rounding may make actual extrema slightly smaller. For a grain fitted to
almost the entire zone, room may be0 and modulation correctly becomes inaudible.
Clipping the displacement to the available start domain can flatten motion near
fit limits; it does not reset phase. Existing MOTION random spread is preserved,
so the **added DRIFT component** is coherent, not the entire randomized position
sequence. The scanning head and its pitch/STRETCH advance are never changed.

The offset is captured only in a new grain's existing position/decoder state.
Already-active grains keep their trajectory, rate, direction, window and gain.
There is no per-sample resampling/pitch modulation or new grain allocation.

## Controls and lifecycle

The slew bound96 Q24 units/output frame smooths C changes even when phase resumes
near a peak. A maximum displacement returns monotonically to zero within21846
frames, about0.4954 seconds. C100→0 does not snap or reverse sign. Once the offset
reaches zero, the update fast path stops. No asymptotic tiny residual remains.

**C0 identity means never-activated/settled DRIFT.** During a C>0→0 transition,
the bounded tail intentionally continues to affect newly spawned grains until
zero; previously displaced grains also finish naturally. Demanding immediate
history-independent C0 PCM would contradict the approved smooth-tail/active-grain
contract. Filters may retain their normal signal history; no filter reset is added.

All243A reset conditions remain intact: fresh voices, zone/source/upload-generation
changes, inactive/invalid/fallback voices, personality exit and engine ownership
changes clear state. Same-zone sounding retrigger retains phase/displacement.
Track selection does not reset it. Transport alone does not reset it; actual note
and release lifecycle governs it. Different source/zone geometry starts neutral,
never reuses a raw offset from the previous zone. No ph[0]/ph[1]/ph[2], LFO, RNG or
unrelated engine memory is borrowed.

The phase/displacement remains continuous relative to the **moving legacy head**.
It is not a new captured anchor, nor a correction to legacy head recapture or
POS/SHP edits. Normal source-head wrapping does not reset DRIFT. Existing head
jumps from deliberate edits/recapture remain unchanged; new active grains are
windowed as before. Scalar controls use the existing serialized IRQ/C1 publication
boundary and aligned per-track parameter access.

## Arithmetic and realtime safety

- C is sanitized before multiplication: c*c*32768<=327680000.
- Sine×depth fits signed32 (magnitude<=2^30); target and displacement stay within
  ±2^21. target-current is within±2^22;96*n<=3072. No signed-overflow UB is needed.
- Displacement is clamped **before** signed64 multiplication, including INT32_MIN.
  Even an adversarial UINT32_MAX length yields a product below2^53, within int64.
  Constant division by2^24 truncates toward zero symmetrically. Offset/room bounds
  remain signed32-safe. Legal indexed zones are additionally limited by GR_NIDX.
- No variable division, floating point, runtime table generation, locks, timers,
  heap memory or added per-sample loop. One sine/depth/slew update per eligible
  voice fragment; one bounded wide source-position scale per FROZEN grain attempt.
- GR_NG12, GR_POLY3, GR_LOAD_MAX8, playback-rate ceiling, reverse caches, scheduler,
  admission and index-readiness rules are unchanged. Cold indexing may temporarily
  refuse a farther displaced candidate, as with existing spray; it cannot trigger
  an unbounded burst. Warm tests verify exact scheduling/RNG/load metrics.

## Qualification and reproduction

```sh
python3 tests/run_grain_drift_state.py --base-tree /tmp/coffee-drift-i3a \
  --out /tmp/grain-drift-audio/qualified
python3 tests/run_granular_quality.py --sanitize \
  --oracle-tree /tmp/coffee-grc-fd7 --out /tmp/grain-drift-audio/regression
python3 tests/run_grain_sparkle.py --base-tree /tmp/coffee-sparkle-745 \
  --out /tmp/grain-drift-audio/sparkle
python3 tests/run_grain_chaos.py --base-tree /tmp/coffee-chaos-93f \
  --out /tmp/grain-drift-audio/chaos
python3 tests/run_editor_stable.py --baseline-tree /tmp/coffee-grc-fd7 \
  --e-tree /tmp/coffee-e-371 --out /tmp/grain-drift-audio/editor
```

Reference trees are clean/pinned to243A, Step240C, pre-SPARKLE, Task241 and Step240E
respectively. Captures/measurements/WAVs stay outside Git. No golden or binary
fixture is regenerated. The old foundation's unused-displacement assertion is
replaced by exact bounded-placement, invariant-head/rate/gain/RNG and slew tests;
its ownership/reset/canary tests remain.

| Gate | Result |
|---|---|
| New strict DRIFT DSP |2488821 checks passed |
| Legal control/depth coverage |All101 controls ×1024 table points, exact targets and bounds |
| Long numerical playback |Five wraps; largest smooth mixed-fragment step512 Q24; live sweeps and finite zero tail |
| Grain placement |672 fitted grains, including336 actual reverse grains; five factory and short synthetic user zones |
|243A C0 PCM/legacy state |24/24 exact |
| Prior156 compatibility pairs |109 inactive pairs exact;47 active pairs distinct, nonzero and exactly repeatable |
| Active timing |RNG draws, admission, reverse admission, peak grain/load and filter-control metrics identical in warm comparisons |
| Long real mixer |Repeated47.55-second render, two phase wraps; expected phase, bounded displacement/output and exact repeat |
| Lifecycle |4311482 shipping /4392318 diagnostic strict checks passed |
| SPARKLE |1411308 kernel checks and full five-source PCM runner pass |
| CHAOS |3804742 checks and full five-source PCM runner pass |
| Short envelopes |1851392 cases /152924099 checks;306→0 silent cases |
| STUTTER gate |42069041 checks;16384 rate/gate settings |
| Filter transitions |3553312 strict checks; worst first edge0 |
| Editor/storage |Stable2909, WIN110, C2259, project bridge2189, persistence109038, old-reader362 pass |
| Browser/frozen storage |Compatibility89, WIN provenance11, frozen NOR5804 and user presets pass |

The109 inactive pairs include all five-source SPARKLE/CHAOS C25/50/100 and live
reverse comparisons, plus CLOUD and C0 cases. The original24 frozen fixtures still
pass. Positive-C FROZEN/mixed cases are intentionally reclassified as changed,
not silently re-goldened. Strict numerical/decoder/lifecycle/filter/storage probes
use ASan+UBSan with signed-overflow checks and recovery disabled. Full mixer PCM
retains its existing signed-overflow/shift exclusions for unrelated old arithmetic;
no exclusion is added for DRIFT. No new focused failure occurred. Historical broad
editor/motion/backup/robustness failures remain documented, not claimed green or
rerun as full broad suites in this task.

The compatibility run records313 unique PCM configurations with maximum peak20468
(<32767). All sounding captures assert nonzero peak/RMS/count. Representative
FROZEN high-density RMS atC25/50/100:

| Source |25 |50 |100 |
|---|---:|---:|---:|
| CELLO0 |1227.56 |1237.58 |1185.44 |
| FLUTE2 |3574.03 |3571.17 |3693.62 |
| GUITAR3 |1405.89 |1402.81 |1532.09 |
| SYNTH13 |2307.46 |2279.85 |2077.47 |
| VOX14 |1342.13 |1286.53 |1211.10 |

RMS/adjacent jumps need not be monotonic with movement depth because positions
change phase/content. measurements.json records hashes, RMS, peak, jumps and state
metrics. Unmodified-level SYNTH DRIFT WAVs are produced for listening; numerical
difference alone is not a claim of subjective musical quality or hardware approval.

## Resources and limitations

Same benchmark translation unit/header selection against243A, host
`cc -Os -ffp-contract=off -fstack-usage`: **text+297 bytes**, rodata/data/BSS0.
No engine/grain/track state growth: part7320 host/7224 expected32-bit, grain64/56,
track1840 host; engine reservation51552 total unchanged. Existing stack frames
are unchanged: spawn112, render272, block40, note-on48 host bytes. No added audio
buffer or per-sample operation. This is not a target stack/link measurement.

Five-run interleaved CPU-time medians,32768 blocks, SYNTH/FROZEN, no file I/O:

| Tracks/C |243A ns/frame |243B |
|---|---:|---:|
|1/C0 |135.189 |144.899 |
|1/C100 |137.305 |138.074 |
|4/C0 |468.741 |470.190 |
|4/C100 |463.975 |467.449 |

The one-track C0 case measures about+7.2%; the other cases about+0.3..0.75%.
These host timings include code-layout/cache/source-seek effects and noise; no
speedup or FM1 deadline guarantee is claimed. New work is bounded at block/spawn
rate, not sample rate, but target profiling is still required. Compile the existing
benchmark with TYPE argument1; base uses
`-DSPARKLE_UI_SOURCE="/tmp/coffee-drift-i3a/tests/ui_test.c"`. Compare size -A/.su;
raw measurements are in /tmp/grain-drift-audio/resources.

No installed default JieLi compiler was available. No download/install was done.
Target flash/RAM/POOL fit, interrupt stack and CPU timing remain unverified. Very
short/fitted sources can suppress movement; edge clamps can flatten it. Empty user
slots use the sine fallback and have no DRIFT. Cold index warmup can delay farther
starts. Existing MOTION/randomness, aliasing, overlap normalization and resonant
headroom are unchanged. Saved nonzero-C FROZEN sounds intentionally change now;
C0 defaults/storage remain unchanged. No arbitrary-patch clipping guarantee is made.

Changed files: firmware/src/eng_grain.c; tests/grain_frozen_drift_test.c,
tests/grain_drift_state_test.c,tests/granular_quality_test.c,
tests/run_grain_drift_state.py,tests/run_granular_quality.py,
tests/run_grain_sparkle.py,tests/run_grain_chaos.py; this report. No blocker remains
for the implemented DSP; hardware listening and profiling are the next safe steps.
