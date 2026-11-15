# Short-grain envelope fidelity experiment

Base: `experiment/coffee-grains-window-morph`, **4c34b6e38612ca13d71aee5a5cc3cd7d9da3b07d**.
Branch: `experiment/coffee-grains-short-grain-fidelity`.

## Result and compatibility

The experiment compares:

- **A:** the original 32-frame endpoint ramp, unchanged verbatim.
- **B:** adaptive subdivision, tested with maximum sub-block sizes **4, 8 and 16**.
- **C:** one window evaluation per output sample.

A remains the default and the exact legacy Hann/morph path. B/C are explicitly opt-in alternate renderers and intentionally change envelope PCM, including Hann, when selected. No automatic correction is applied to old sounds.

Across the tested short-grain envelope cases, A had **12,672 silent cases / 98,304**; B4, B8, B16 and C each had **768**, all length-one cases. B4 reduced aggregate normalized envelope RMS error from **0.322244 to 0.010213**; C matched the independent per-sample reference exactly. These are constant-carrier numerical results, not musical quality ratings.

Recommendation: retain A for legacy compatibility; investigate a separately approved hybrid using **C for total durations <=32 frames** and **adaptive cap 8 for longer grains**, with cap 4 as the accuracy comparison. Measure 64/128-frame crossover alternatives on FM1 before choosing a shipping threshold. This fourth hybrid policy is a recommendation, **not implemented here**.

## Implementation and invariants

`firmware/src/eng_grain.c` changes only by adding the guarded header and selecting the wrapper at the existing grain-render call. The original `gr_run` function is byte-for-byte identical as source to the base (the comparison runner asserts this). No tables, source assets, parameters, IDs, presets, UI, modulation routing, formats, scheduling, RNG calls, admission limits or source-head calculations changed.

Enable `FELUCCA_GRAIN_FIDELITY_EXPERIMENT=1` to compile `grain_fidelity.h`. For three-window/morph testing also enable the existing window and morph experiment flags. Four test-only per-track method bytes initially contain **0/A**, and their cap bytes initially contain **4**. Methods: 0=A, 1=B, 2=C. Other method bytes fall back to A; invalid B caps also fall back to A. C ignores the cap. No public or persistent controls exist.

Set fidelity controls **before playback**. Fidelity-method changes during an active grain are intentionally unsupported; a live switch could change approximation gain. The existing **window/morph value remains latched per grain** under every method, including changes to window controls while a grain runs. No fidelity field was added to the grain record.

### Adaptive B

B reuses the original `gr_run` on bounded fragments; it never schedules or admits another grain. Duration bands are selected from the immutable phase increment (equivalent to the following tested total-duration bands):

| Total duration | Nominal fragment |
|---|---:|
| <=16 frames | 1 |
| 17..32 | 2 |
| 33..64 | 4 |
| 65..256 | 8 |
| >256 | 16 |

Clamp that fragment to the selected cap 4/8/16. Near the end, when remaining lifetime <= twice the fragment, refine to at most `max(1,remaining/2)`. Also clamp to the available portion of the current block. This explicitly samples short-grain interiors instead of connecting only two zero endpoints. At most 32 original-kernel calls occur in a 32-frame parent block; no unbounded loop or new carry state is needed.

The wrapper retains the original ramp arithmetic, including its existing per-segment division on partial segments. This is a correctness prototype, not a claim of optimal code. For tiny grains it repeatedly calls the legacy one-frame kernel, calculating an unused endpoint/ramp; that overhead is a reason to prefer the dedicated C loop there. No new variable division is introduced in C's sample loop; existing morph divisions remain constants 64/63.

Fragment boundaries restart from the true window value at the current phase, not zero. Strict tests compare each adaptive fragment's first output with an independent one-sample reference. That equality holds for all tested constant-carrier and signed/moving-source cases. Approximation slopes can still change at boundaries, and approximation error depends on parent-block partitioning. No universal click-free claim is made.

### Per-sample C

`gr_run_sample` evaluates the same latched window at each sample's current Q32 phase, applies the existing Q15 grain gain, multiplies the same linearly interpolated source value, and advances phase once. There is no float, table generation, heap allocation or new audio buffer.

Its forward/reverse decoder statements intentionally match `gr_run`; only envelope evaluation and phase/countdown placement differ. Moving countdown/phase updates to the end is safe here because decoder/cache helpers do not consume those fields. Strict tests compare **entire pool state**, including decoder predictor/index, interpolation samples/fraction, source position, phase, lifetime, owner, window byte and reverse buffers, after every parent call against both A and the reference. All agree. This duplication is a prototype maintenance cost: keep those equivalence tests if either decoder path is edited later.

Source advancement, ADPCM reads and cache-refill positions remain the same. B/C add window-table work, **not source-sample reads**. Voice amplitude ramps, TONE/filter processing and track mixing still execute once in their original locations after the grain accumulator is assembled.

## Reproducible tests

With existing generated headers/pinned engines available:

```sh
python3 tests/grain_fidelity_compare.py \
  --base-tree /path/to/clean/4c34b6e-worktree \
  --out /tmp/grain-fidelity-comparison
```

The final runner includes expanded kernel/latch vectors and the additional identity cases. During development these expansions were confirmed separately rather than rerunning unchanged audio matrices: the final totals below include the supplementary 80 exact identity pairs. No old assertions or goldens were weakened or refreshed.

| Validation | Result |
|---|---|
| Strict kernel/numerical vectors | **493,346 cases; 12,379,206 checks passed** without CSV; CSV mode adds two I/O checks |
| Latching/default-state tests | **25,281 checks × four policies = 101,124 passed** |
| Exact previous PCM identity | **832 input cases × two builds = 1,664 renders passed** |
| B4/B8/B16/C integration matrix | **1,344 sounding renders passed** |
| Independent methods across tracks | **12 mixed-policy renders passed** |
| Listening-source renders | **36 passed**, including four explicitly expected-silent A baselines |
| Total audio executions | **3,056 passed** |

### Strict reference and coverage

Strict kernel flags: `-O1 -g -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=all`, **no category exclusions**. Numerical metrics are computed in test code only, outside the callback.

The reference is the **unchanged original kernel called with n=1 repeatedly**, not the new C implementation compared against itself. C's output matches it exactly. A's output matches original parent-size calls exactly. After every parent call, all methods' complete pool states match both references.

Tests cover every duration **1..128**, every start offset **0..31**, both directions and 12 window representations: three discrete windows plus morph values 0/16/32/48/64/80/96/112/127. Varying duration and start covers every end offset too. Constant-carrier metrics use synthetic zero source advance to isolate the envelope; these are not invented production pitch settings. Separate tests cover signed source extremes and valid rates **256/32768/65536/131072 Q16**, moving source, output canaries and invalid control bytes. Representative longer durations are **129,441,4410,86432**.

Additionally, 120 real-source long cases render complete **86,432-frame** grains over SYNTH at the 2× cap, both directions, every selected window and method. They require nonzero output, cross more than 1,000 reverse-cache refill boundaries, and retain complete decoder/cache state identity. Output guards and bounds checks pass. All table/morph lookup code remains the previously validated code.

Integration/latch tests retain existing exclusions for signed-integer-overflow, shift, bounds, object-size and pointer-overflow. No new exclusion was added, and these integration runs are not presented as strict whole-mixer proofs. No sanitizer diagnostic occurred.

Identity coverage includes every morph 0..127 for all personalities on GUITAR, and Gaussian/Hann/Tukey anchors plus morph32 across every personality × five factories/user/sine × minimum/normal/maximum SIZE/HOLD states. All renders use four tracks and nonzero master. Alternate renders cover the same sources/durations at 0/32/64/127 with B4/B8/B16/C. Direction observations and state sizes agree with baseline. Mixed tests assign A/B/C/B independently to four tracks, with different caps and morphs; the A track remains silent in the tiny-zone case while improved tracks sound.

## Envelope error and silent counts

[Detailed per-length/window summary](data/grain-short-fidelity/envelope-summary.csv) aggregates offsets/directions. Raw vectors can be regenerated by the runner; their full CSV is not committed. Reference error is actual fixed-point PCM envelope output against the one-sample kernel on a constant carrier, normalized by 32767. Table quantization is part of the reference, not an error against an ideal continuous mathematical window.

For durations 1..128, each policy has **98,304 cases = 128 lengths × 32 offsets × 12 representations × two directions**:

| Method | Silent cases | RMS across per-case normalized RMS errors | Worst per-case RMS error |
|---|---:|---:|---:|
| A | 12672 | 0.322244 | 0.865964 |
| B cap4 | 768 | 0.010213 | 0.044304 |
| B cap8 | 768 | 0.015102 | 0.045972 |
| B cap16 | 768 | 0.015102 | 0.045972 |
| C | 768 | 0 | 0 |

The aggregate weights each test case equally, not each musical grain or each output sample across all durations. B8/B16 coincide in this short-duration cohort because duration bands already constrain fragments to at most eight frames; their longer-duration behavior differs.

Representative RMS errors, aggregated over tested windows/offsets for that duration:

| Frames | A | B4 | B8 | B16 | C |
|---:|---:|---:|---:|---:|---:|
| 16 | .54107848 | 0 | 0 | 0 | 0 |
| 32 | .41933279 | .00793258 | .00793258 | .00793258 | 0 |
| 64 | .22168663 | .00812982 | .00812982 | .00812982 | 0 |
| 128 | .09163061 | .00214408 | .00806784 | .00806784 | 0 |
| 441 | .01113693 | .00019101 | .00074283 | .00285599 | 0 |
| 4410 | .00012527 | .00001572 | .00002175 | .00004076 | 0 |
| 86432 | .00001978 | .00001458 | .00001725 | .00001886 | 0 |

Long-duration rows use the stated representative offset, not all 32 offsets; full short-offset coverage is the 1..128 cohort. Residual sub-LSB/LSB-scale differences at long lengths arise from endpoint gain/ramp rounding versus per-sample rounding. B is not partition invariant; C is exact against the per-sample reference across all tested partitions.

### One/two-sample constraints and discontinuities

All remaining silent envelope cases have length **one**. Under the retained phase convention, that one sample is at phase zero and every window is zero there. Making it nonzero would require a separate phase/sampling policy. Length two can sample an interior peak but then jump to silence; a nonzero one/two-sample pulse is not evidence of meaningful pitched sound or click-free playback. Existing admission still rejects grains shortened below 16 frames; 1..15 are numerical stress vectors, not newly enabled user grains.

The largest adjacent jump in the short constant-carrier cohort is **32765** for every policy, driven by such extreme short pulses. It is not a sub-block seam detector. Adaptive boundary outputs match the reference exactly, so no reset-to-zero seam was observed. Natural waveform changes and slope changes remain; listening and hardware testing are still required.

## Listening captures

72 WAVs: four personalities × three source conditions × A/B4/C × raw/matched. Each is all **131,072 stereo frames, 44.1 kHz PCM16 (~2.972 s)**. All source/notes/seeds/parameters are identical within a policy comparison. Window=morph32 throughout. Raw files preserve mixer integers, with explicit PCM16 bounds checks, no clipping or normalization. The four GUITAR/A raw captures are byte-identical to the previous morph recordings.

Source conditions:

- **GUITAR (3):** existing factory audio, RAND127; normal grains and reverse activity.
- **short35:** legal synthetic user-zone descriptor over 35 valid ADPCM frames, RAND0 so fitting yields 16..32-frame grains. A must be exactly silent; B/C must produce nonzero audio.
- **short131:** analogous 131-frame source for roughly 64..128-frame fitted grains. RAND0 isolates duration/envelope effects. These short demos are forward-only; reverse and high-load behavior is tested separately above. No actual user flash upload or original sample modification occurs.

Short fixtures are quiet test material. Offline matched copies target -24 dBFS RMS, capped to keep peak below 95% full scale. **Gain may amplify** these quiet derived captures (maximum about 94.885×); it never changes firmware gain or original recordings. No limiter/clipping is applied. Exact gains/status are in [listening metadata](data/grain-short-fidelity/listening.csv). Silent A/short35 files cannot be RMS-matched and remain zero even in the consistently named `matched` files. Do not treat those as equal-loudness listening comparisons.

[Audio measurements](data/grain-short-fidelity/audio.csv) contain 2,224 current render measurements; the previous-build identity buffers were compared but not duplicated in this table. [Checksums](audio/grain-short-fidelity/SHA256SUMS) cover all captures. RMS matching is not perceptual loudness matching, and no subjective sound-quality ranking is asserted.

| Personality/source | A raw / matched | B4 raw / matched | C raw / matched |
|---|---|---|---|
| cloud / 3 | [raw](audio/grain-short-fidelity/cloud-3-A-raw.wav) / [matched](audio/grain-short-fidelity/cloud-3-A-matched.wav) | [raw](audio/grain-short-fidelity/cloud-3-B4-raw.wav) / [matched](audio/grain-short-fidelity/cloud-3-B4-matched.wav) | [raw](audio/grain-short-fidelity/cloud-3-C-raw.wav) / [matched](audio/grain-short-fidelity/cloud-3-C-matched.wav) |
| cloud / short35 | [raw](audio/grain-short-fidelity/cloud-short35-A-raw.wav) / [matched](audio/grain-short-fidelity/cloud-short35-A-matched.wav) | [raw](audio/grain-short-fidelity/cloud-short35-B4-raw.wav) / [matched](audio/grain-short-fidelity/cloud-short35-B4-matched.wav) | [raw](audio/grain-short-fidelity/cloud-short35-C-raw.wav) / [matched](audio/grain-short-fidelity/cloud-short35-C-matched.wav) |
| cloud / short131 | [raw](audio/grain-short-fidelity/cloud-short131-A-raw.wav) / [matched](audio/grain-short-fidelity/cloud-short131-A-matched.wav) | [raw](audio/grain-short-fidelity/cloud-short131-B4-raw.wav) / [matched](audio/grain-short-fidelity/cloud-short131-B4-matched.wav) | [raw](audio/grain-short-fidelity/cloud-short131-C-raw.wav) / [matched](audio/grain-short-fidelity/cloud-short131-C-matched.wav) |
| freeze / 3 | [raw](audio/grain-short-fidelity/freeze-3-A-raw.wav) / [matched](audio/grain-short-fidelity/freeze-3-A-matched.wav) | [raw](audio/grain-short-fidelity/freeze-3-B4-raw.wav) / [matched](audio/grain-short-fidelity/freeze-3-B4-matched.wav) | [raw](audio/grain-short-fidelity/freeze-3-C-raw.wav) / [matched](audio/grain-short-fidelity/freeze-3-C-matched.wav) |
| freeze / short35 | [raw](audio/grain-short-fidelity/freeze-short35-A-raw.wav) / [matched](audio/grain-short-fidelity/freeze-short35-A-matched.wav) | [raw](audio/grain-short-fidelity/freeze-short35-B4-raw.wav) / [matched](audio/grain-short-fidelity/freeze-short35-B4-matched.wav) | [raw](audio/grain-short-fidelity/freeze-short35-C-raw.wav) / [matched](audio/grain-short-fidelity/freeze-short35-C-matched.wav) |
| freeze / short131 | [raw](audio/grain-short-fidelity/freeze-short131-A-raw.wav) / [matched](audio/grain-short-fidelity/freeze-short131-A-matched.wav) | [raw](audio/grain-short-fidelity/freeze-short131-B4-raw.wav) / [matched](audio/grain-short-fidelity/freeze-short131-B4-matched.wav) | [raw](audio/grain-short-fidelity/freeze-short131-C-raw.wav) / [matched](audio/grain-short-fidelity/freeze-short131-C-matched.wav) |
| stutter / 3 | [raw](audio/grain-short-fidelity/stutter-3-A-raw.wav) / [matched](audio/grain-short-fidelity/stutter-3-A-matched.wav) | [raw](audio/grain-short-fidelity/stutter-3-B4-raw.wav) / [matched](audio/grain-short-fidelity/stutter-3-B4-matched.wav) | [raw](audio/grain-short-fidelity/stutter-3-C-raw.wav) / [matched](audio/grain-short-fidelity/stutter-3-C-matched.wav) |
| stutter / short35 | [raw](audio/grain-short-fidelity/stutter-short35-A-raw.wav) / [matched](audio/grain-short-fidelity/stutter-short35-A-matched.wav) | [raw](audio/grain-short-fidelity/stutter-short35-B4-raw.wav) / [matched](audio/grain-short-fidelity/stutter-short35-B4-matched.wav) | [raw](audio/grain-short-fidelity/stutter-short35-C-raw.wav) / [matched](audio/grain-short-fidelity/stutter-short35-C-matched.wav) |
| stutter / short131 | [raw](audio/grain-short-fidelity/stutter-short131-A-raw.wav) / [matched](audio/grain-short-fidelity/stutter-short131-A-matched.wav) | [raw](audio/grain-short-fidelity/stutter-short131-B4-raw.wav) / [matched](audio/grain-short-fidelity/stutter-short131-B4-matched.wav) | [raw](audio/grain-short-fidelity/stutter-short131-C-raw.wav) / [matched](audio/grain-short-fidelity/stutter-short131-C-matched.wav) |
| shimmer / 3 | [raw](audio/grain-short-fidelity/shimmer-3-A-raw.wav) / [matched](audio/grain-short-fidelity/shimmer-3-A-matched.wav) | [raw](audio/grain-short-fidelity/shimmer-3-B4-raw.wav) / [matched](audio/grain-short-fidelity/shimmer-3-B4-matched.wav) | [raw](audio/grain-short-fidelity/shimmer-3-C-raw.wav) / [matched](audio/grain-short-fidelity/shimmer-3-C-matched.wav) |
| shimmer / short35 | [raw](audio/grain-short-fidelity/shimmer-short35-A-raw.wav) / [matched](audio/grain-short-fidelity/shimmer-short35-A-matched.wav) | [raw](audio/grain-short-fidelity/shimmer-short35-B4-raw.wav) / [matched](audio/grain-short-fidelity/shimmer-short35-B4-matched.wav) | [raw](audio/grain-short-fidelity/shimmer-short35-C-raw.wav) / [matched](audio/grain-short-fidelity/shimmer-short35-C-matched.wav) |
| shimmer / short131 | [raw](audio/grain-short-fidelity/shimmer-short131-A-raw.wav) / [matched](audio/grain-short-fidelity/shimmer-short131-A-matched.wav) | [raw](audio/grain-short-fidelity/shimmer-short131-B4-raw.wav) / [matched](audio/grain-short-fidelity/shimmer-short131-B4-matched.wav) | [raw](audio/grain-short-fidelity/shimmer-short131-C-raw.wav) / [matched](audio/grain-short-fidelity/shimmer-short131-C-matched.wav) |

Examples (raw RMS / peak / max adjacent jump): CLOUD/short35 A **0/0/0**, B4 **21.789/206/48**, C **21.879/206/48**; CLOUD/GUITAR A **2409.715/14459/1418**, B4 **2409.962/14459/1418**, C **2410.039/14459/1418**. SHIMMER/short131 A **42.592/246/162**, B4 **35.361/255/174**, C **35.312/255/178**. Fidelity does not imply larger RMS or smaller jumps for every source: the correct window interacts with the source waveform. The full CSV includes energy and squared adjacent differences as well as peaks/RMS.

## Operation counts and host timing

A uses two endpoint evaluations per parent call except the terminal endpoint forced to zero. B does that per fragment; C uses one evaluation per sample. Each evaluation needs one table lookup at an anchor, two plus a blend at intermediate morph values. Source decode counts are unchanged. B also repeats legacy function state loads/stores and its partial-ramp arithmetic.

Source-derived **window evaluation counts per grain at offset zero**:

| Frames | A | B4 | B8 | B16 | C |
|---:|---:|---:|---:|---:|---:|
| 16 | 1 | 31 | 31 | 31 | 16 |
| 32 | 1 | 33 | 33 | 33 | 32 |
| 128 | 7 | 67 | 37 | 37 | 128 |
| 441 | 27 | 225 | 117 | 63 | 441 |
| 4410 | 275 | 2209 | 1109 | 559 | 4410 |

These are algorithm counts, not hardware performance counters. The apparent cheapness of A at 16/32 is precisely its failure to sample the envelope interior. B's one-frame reuse computes an endpoint it never uses, so C can be both more accurate and cheaper than this adaptive prototype on tiny grains.

[Host benchmark raw repeats](data/grain-short-fidelity/host-bench.csv): `tests/grain_fidelity_bench.c`, `cc -O2 -ffp-contract=off` without sanitizers, process CPU clock, five repeats, about 1,048,576 frames/case. Includes common grain reset/initial seek, kernel and test checksum work, not the complete synth/mixer/UI. Earlier wall-time/shorter runs were too noisy and were replaced by this CPU-time protocol. Even final repeats varied by up to **2.15×** in the shared host; use broad trends, not fine percentage rankings.

Example median host CPU ns/output frame, forward SYNTH source, morph32:

| Frames | A | B4 | B8 | B16 | C |
|---:|---:|---:|---:|---:|---:|
| 16 | 11.5 | 23.1 | 24.3 | 23.9 | 14.9 |
| 32 | 8.6 | 15.7 | 14.9 | 15.2 | 13.5 |
| 128 | 9.3 | 12.3 | 10.8 | 10.8 | 12.3 |
| 441 | 9.3 | 11.4 | 9.6 | 9.1 | 11.1 |
| 4410 | 8.5 | 11.1 | 9.9 | 9.6 | 11.6 |

The small apparent B16/A reversal at 441 is within host variability, not a speedup claim. Host CPU clocks, cache and compiler differ from pi32v2; none of these numbers establishes a 44.1 kHz FM1 deadline. Benchmark reproduction: compile `tests/grain_fidelity_bench.c` with the three experiment defines, include paths used by the runner, `-O2 -ffp-contract=off -lm`, then run it with a CSV output path.

## Resources and hardware limitations

Identical `cc -Os -ffp-contract=off -fstack-usage` builds of the same new audio fixture:

| Build | text/rodata | data | BSS |
|---|---:|---:|---:|
| Previous morph source | 536363 | 19676 | 310568 |
| Current, fidelity compiled out | 536363 | 19676 | 310568 |
| Current, fidelity enabled | 537482 | 19676 | 310568 |
| Enabled delta | **+1119** | **0** | **0** |

The previous/current disabled fixture objects are byte-identical. Native browser unity objects with the same window/morph flags and fidelity disabled are also byte-identical: text **886977**, data **31060**, BSS **1603232**. Runtime A in an enabled object preserves PCM but not necessarily compiler layout/performance.

There are eight logical test-control bytes (four methods, four caps). They fit existing aggregate host section layout/padding in this fixture; unchanged data/BSS does not mean no new logical control state. No grain/track field, audio buffer or lookup table is added. Host grain remains **64 bytes**, private part **7296**, common track **1840**, reservation **12888/track**, **51552 total**. All are asserted/compared. Target ABI should retain the same existing record size because no record declaration changes, but it was not measured here.

Stack is **not neutral when enabled**: host `grain_render` frame **328→344 bytes**, and the original kernel becomes an outlined **120-byte `gr_run` frame** on A/B calls (previously inlined). The C loop is inlined in this host build. `gr_spawn_one` remains **112 bytes**. These are individual frames, not a target worst-case stack proof. Adaptive reuse is intentionally conservative about decoder correctness but has call/stack overhead that must be addressed/profiled before shipping. No claim that a 16-byte caller delta is the full call-chain increase.

The unchanged target build preflight failed:

```
build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)
```

No toolchain was installed/downloaded, no link or hardware benchmark occurred. Flash fit against **581,564 bytes**, target RAM/stack headroom, four-track worst-case callback cost and dual-core cache behavior remain unverified. The native single-grain benchmark does not replace all-track device profiling. At high density, per-sample morph work can add many window lookups despite unchanged ADPCM admission limits.

## Recommendation and remaining work

1. Keep A as the explicit compatibility/default path. Never silently replace legacy Hann or morph playback.
2. For an eventual opt-in fidelity mode, prototype **C through 32 frames** and **B cap8 above that**, comparing 64/128 crossover choices on device. C eliminates the demonstrated short-block silence and avoids adaptive one-frame-call overhead; B8 offers a lower lookup budget on longer grains. B4 remains the higher-accuracy alternative. Do not ship a crossover selected from noisy host timing alone.
3. Retain exact window/morph anchors and grain-level window latching. Decide a method-handoff policy before any live UI control: current fidelity controls are test-only and static during playback.
4. Measure target deadlines and complete call-chain stack with maximum active grains, reverse source rates, all tracks/effects and CPU2 fallback. Consider flattening adaptive execution without changing the preserved A body, and continue complete decoder-state comparisons if duplicated loops are refactored.
5. Keep the one/two-sample caveat and test actual musical sources/listening. No assertion of subjective superiority follows from lower envelope error.

Changed production scope: guarded call/header in `eng_grain.c`, new `grain_fidelity.h`. New tests: `grain_fidelity_{kernel,audio,latch}_test.c`, comparison runner and host benchmark. Documentation includes this report, measurements and derived listening captures. Existing UI, parameter/preset/backup formats, source assets, source-head fixes and other engines remain unchanged. Broad UI/MIDI/project suites were not rerun for this opt-in DSP experiment; default object identity and stated numerical/audio matrices are the actual evidence. No merge, deployment or flashing.
