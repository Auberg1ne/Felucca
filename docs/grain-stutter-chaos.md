# GLITCH/STUTTER CHAOS — Task242

Base: `93feb7652498633be829e38e9ed8ee4ee5b70084`, existing branch
`feat/coffee-grains-granular-quality`. Local-only implementation. No push,
merge, release, hardware flash or tool installation.

## Narrow DSP design

The actual STUTTER path already has an interval/countdown scheduler, rate control,
SPRD timing jitter and a phase-based smoothed gate. `gr_spawn_one` draws two random
words before admission: r1 controls pitch/direction; r2's low16 control source
spray. SHIMMER SPARKLE uses r2's high16 in TYPE3 only.

CHAOS uses those high16 **only in TYPE2** to add a bounded source-start offset.
It changes neither rhythmic timing nor pitch. The fragmentation is in source
content under the existing gate, rather than new random clock events. This avoids
scheduler bursts, extra RNG draws and abrupt gate changes. No new grains, voice
records, persistent fields, buffers, heap operations or locks are added.

For canonical C in0..100 and source length n:

```
amountQ16 = floor(C*C*32768 /10000)
width     = floor(n*amountQ16 /65536)
offset    = floor(width*(r2>>16) /65536) - floor(width/2)
start     = existing POS/SHP/SPRD start + offset
```

The existing forward/reverse span clamp then owns final bounds; indexing and
load admission checks are unchanged. Q16 scales use the existing split-product
`gr_scale`, with no new64-bit multiplication or variable division in firmware.

| C | Total additional window width | Approximate centered offset |
|---|---:|---:|
|0 |0 |exact legacy path |
|25 |3.125% of source |±1.5625% |
|50 |12.5% |±6.25% |
|100 |50% |±25% |

The quadratic depth keeps low settings smaller than a linear full-source random
jump. Existing grain lengths, pitch, envelopes, gain, normalization and gate shape
remain. Audio consists of contiguous sample fragments, not random sample values.
Subtle/fragmented/chaotic character remains a listening judgment; finite-capture
RMS and PCM differences are not a subjective sound-quality rating.

Only future grain starts see C edits. Already-active grains retain their decoder
positions and window trajectories. C is read through the existing per-track16-bit
parameter path; no new cross-core publication mechanism is needed. C0 skips the
new arithmetic entirely, preserving RNG advancement and scheduling exactly.
CLOUD/FREEZE remain inactive at C>0, while SHIMMER retains SPARKLE unchanged.

## Bounds and realtime reasoning

C is clamped to100 locally; negative values do not enter the branch. The largest
new numerator is100*100*32768=327680000, well within uint32. A usable indexed zone
has at most GR_NIDX*256=275200 frames. Width<=n/2 and |offset|<=n/4 (integer
rounding included); together with existing SPRD, the candidate signed position
stays far inside int32. Forward `[0,n-3-span]` and reverse `[span,n-2]` clamps
remain after this offset. Fitting/minimum-length rules are unchanged.

There is no new per-sample or scheduling operation. Added work is a TYPE/C branch,
a bounded quadratic scale and two existing-style fixed-point position scales
per eligible grain attempt. Division is by constant10000, not a variable. RNG,
GR_NG12, GR_POLY3, GR_LOAD_MAX8,2× playback-rate cap and reverse caches are unchanged.
Different source starts can alter seek-prefix work, still within the original
255 skipped-decode bound. The original one-attempt-per-voice/block limit remains.

The strict fully-indexed render tests show identical scheduler/RNG/gate/load
signatures at C0,C100 and live0→100→0 changes. During normal cold index building,
a randomized candidate can lie beyond the ready index and be refused, as with
existing spray; this can temporarily change admission/RNG history at nonzero C.
No compatibility claim is made for that intentionally different nonzero-C state.
The C0 exact-state guarantee also covers normal index behavior through unchanged
production code and exact-base PCM/state comparisons.

## Reproduction and results

Use clean local reference worktrees and their existing engine submodules. Outputs
are outside Git; no fixtures or golden audio are regenerated:

```sh
python3 tests/run_grain_chaos.py --base-tree /tmp/coffee-chaos-93f \
  --out /tmp/grain-chaos/final
python3 tests/run_grain_sparkle.py --base-tree /tmp/coffee-sparkle-745 \
  --out /tmp/grain-chaos/sparkle
python3 tests/run_granular_quality.py --sanitize \
  --oracle-tree /tmp/coffee-grc-fd7 --out /tmp/grain-chaos/regression
python3 tests/run_editor_stable.py --baseline-tree /tmp/coffee-grc-fd7 \
  --e-tree /tmp/coffee-e-371 --out /tmp/grain-chaos/editor
```

Reference commits:93feb76(Task241),7453396(pre-SPARKLE),fd7f6f0(frozen storage
oracle),37187a5(old editor/downgrade oracle). The CHAOS runner emits raw PCM,
measurements.json and unnormalized stereo SYNTH-CHAOS-C0/25/50/100.wav files.

| Gate | Result |
|---|---|
| Strict CHAOS kernel |3804742 checks passed;8098 changed source starts |
| Exact Task241 C0 |24/24 PCM and explicit state records identical |
| Unaffected C100 personalities |15/15 pure CLOUD/FREEZE/SHIMMER comparisons exact to Task241 |
| SPARKLE anchors/intermediates |15/15 five-source C25/50/100 PCM/state comparisons exact to Task241 |
| CHAOS audio |All five sources: C25/50/100 distinct, nonzero, meaningful difference and exact repeat |
| Live edits |Five-source reverse0→100→0 sequences repeat exactly; active-grain latching tested |
| Four-track rendering |C25/50/100, bounded grains/load and nonzero final mixer output |
| Existing SPARKLE kernel |1411308 checks passed; existing five-source audio gates pass |
| Short envelopes |1851392 cases /152924099 checks;306→0 silent cases |
| STUTTER/GLITCH gate |42069041 checks;16384 rate/gate settings |
| Filter transitions |3553312 strict checks; worst first edge0 |
| Persistence |109038 current checks plus5804 frozen NOR checks passed |
| Editor/backup/downgrade |Stable2909, WIN110, C2259, project bridge2189, actual old-reader362 passed |
| Browser |Compatibility89 and WIN provenance11 passed |
| User presets |Passed |

The strict kernel covers POS0/64/127, forward/reverse, C0/1/25/50/100 plus invalid
extremes, expected starts against a wide-integer reference, unchanged head/rate/
length/gain, no extra RNG draws, decoder trajectory latching, canaries and real
render scheduler signatures. Three synthetic user descriptors of32,257 and65535
frames exercise fitting/rounding and logical source bounds using existing source
data, without journal/allocator modifications. An initial short-fixture reference
incorrectly recomputed span from rounded fitted length; it was corrected to model
the existing conservative pre-fit span. Production fitting code was not changed.

Existing frozen24 C0 PCM/state cases still pass. The old C100 quarantine assertion
is updated intentionally:10 pure CLOUD/FREEZE cases remain exact;14 CHAOS/SPARKLE/
four-track cases now change. Dedicated exact-base tests replace the obsolete
STUTTER C100 identity expectation. No assertion/golden is silently removed.

Strict kernel, filter, envelope, gate and storage paths use ASan+UBSan with signed
overflow enabled and recovery disabled. Full mixer audio retains its existing
signed-overflow/shift exclusions for unrelated historical arithmetic. No exclusion
was added for CHAOS. No new focused failure or sanitizer regression occurred.
Historical broad editor/motion/backup/robustness failures recorded at the base
remain unresolved; those broad failing suites are not claimed green or rerun here.

Representative high-density final-output RMS,65536 stereo frames/source:

| Source | C0 | C25 | C50 | C100 |
|---|---:|---:|---:|---:|
| CELLO0 |995.91 |1014.49 |897.42 |831.77 |
| FLUTE2 |1791.22 |1796.76 |1938.05 |1763.20 |
| GUITAR3 |907.41 |1022.33 |1055.18 |1053.72 |
| SYNTH13 |1361.84 |1503.56 |1481.68 |1310.18 |
| VOX14 |856.11 |854.49 |734.53 |793.10 |

All141 distinct PCM configurations pass nonzero output and peak<32767 checks;
maximum peak across this set is19771. Difference RMS and adjacent-sample jumps
are recorded in measurements.json. Source relocation decorrelates phase/content,
so RMS or finite-record jump maxima need not vary monotonically with C. Gate and
envelope continuity are protected by unchanged kernels and their strict tests;
no hard C-controlled gain switch is introduced.

## Resources

Host `cc -Os -ffp-contract=off -fstack-usage`, identical benchmark translation unit
against Task241 and new DSP: **text+92 bytes, rodata0,data0,BSS0**.
Grain64, part7296 and track1840 bytes remain unchanged. Host compiler stack frames
remain112 bytes for gr_spawn_one and272 for grain_render (not target stack proof).
The existing benchmark accepts optional fourth argument TYPE (default3 unchanged):
`benchmark C tracks 2` measures STUTTER without changing old SPARKLE invocations.

Five-run CPU-time medians,32768 blocks, SYNTH/high density/RAND127, no file I/O:

| Tracks | Task241 C0 ns/frame | New C0 | New C100 |
|---|---:|---:|---:|
|1 |143.369 |143.733 |141.539 |
|4 |469.526 |467.948 |468.132 |

These are noisy host measurements, not a speedup claim or FM1 timing. C0 variation
is about+0.25% /-0.34%; seeking source material at different offsets can alter
measured work. The existing decoder bound is authoritative, not these averages.
For size/stack reproduction compile tests/grain_sparkle_bench.c with the same flags;
base adds `-DSPARKLE_UI_SOURCE="/tmp/coffee-chaos-93f/tests/ui_test.c"`. Compare
`size -A` and .su outputs. Measurements are in /tmp/grain-chaos/resources.

No installed default JieLi pi32v2 compiler was found. No download/install was
attempted. Actual FM1 flash/RAM/POOL fit, CPU timing and interrupt-stack use remain
unverified until a target link/profile. No new bitmap or framebuffer exists.

## Compatibility, changed files and remaining limits

ID118, range0..100/default0, one shared value/track and TYPE-dependent interpretation
are unchanged. Existing stored nonzero-C STUTTER sounds now intentionally change;
no stored values are rewritten. FUND13/preset8/9, legacy mapping, command82,
MIDI/automation quarantine, hardware grid and all assets remain unchanged. There
is no new CHAOS knob or MIDI route in this task. DRIFT and SPREAD remain inactive.

Very short/fitted zones or boundary-clamped positions can limit the offset's
resolution. Empty/unavailable sources use the existing sine fallback, with no
grains and hence no CHAOS. Cold index warmup can temporarily reject farther starts.
Existing normalization, aliasing and resonant headroom limitations are not changed.
Subjective musicality/click acceptability and target CPU need FM1 listening/profile;
no blanket no-clipping claim is made for arbitrary patches.

Changed files: firmware/src/eng_grain.c; tests/grain_chaos_test.c,
tests/run_grain_chaos.py,tests/granular_quality_test.c,
tests/run_granular_quality.py,tests/run_grain_sparkle.py,
tests/grain_sparkle_bench.c; this report. Production change is8 lines at grain spawn.
Next safe step is hardware listening/target profiling via the existing negotiated
scalar C access, with zero defaults and no new UI/protocol/preset changes.
