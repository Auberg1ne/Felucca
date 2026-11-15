# Experimental continuous grain-window morph

Base: `experiment/coffee-grains-three-windows`, **ccf1e227c0f585ed03a5263d870c9dc4a708fc26**.
Branch: `experiment/coffee-grains-window-morph`.
Reference audit: **a012723f699721f9834804050596e82af31720e4** (not merged).

## Behavior and compatibility

This is a DSP-only, opt-in experiment. It adds no shipping UI, parameter ID, modulation target, preset default or project field. All three existing window tables, their lookup functions, grain decoding, RNG, source scanning, scheduling, admission, normalization, voices and block-endpoint ramp remain unchanged.

The existing `FELUCCA_GRAIN_WINDOW_EXPERIMENT=1` still enables only the discrete test selector: **0 Hann / 1 Tukey / 2 Gaussian**. A separate **`FELUCCA_GRAIN_MORPH_EXPERIMENT=1`** enables the new code and requires the window experiment. Without it, the previous experimental code is retained exactly. A build attempting morph without the window experiment is rejected explicitly.

Even with both flags enabled, four test-only per-track mode bytes initially contain **0 (OFF)**, and their morph controls initially contain **64 (Hann)**. Tests write these controls between render calls; this is not a concurrent UI/control API.

| Runtime test mode | Control | Newly spawned grain |
|---|---|---|
| OFF (0) | Existing discrete selector 0/1/2 | Exact previous Hann/Tukey/Gaussian behavior; morph value ignored |
| ON (1) | 0 | Original Gaussian lookup directly |
| ON | 1..63 | Gaussian/Hann linear amplitude blend, fraction `m/64` |
| ON | 64 | Original Hann lookup directly, no extra rounding |
| ON | 65..126 | Hann/Tukey blend, fraction `(m-64)/63` |
| ON | 127 | Original Tukey lookup directly |
| ON | Invalid morph byte 128..255 | Hann |
| Invalid mode byte 2..255 | Any value | Hann |
| OFF | Invalid discrete selector byte 3..255 | Hann, as before |

Mode ON overrides the discrete selector only for that track's future grains. This preserves the exact legacy default and supports four simultaneous, independent settings. No equal-power interpolation or loudness compensation is performed in firmware.

## Per-grain latching without record expansion

`firmware/src/eng_grain.c:gr_spawn_one` keeps the existing `uint8_t window`. Discrete values 0..2 retain their meanings. With morph compiled in, **128+m** encodes a latched morph value 0..127 in that same byte. Values 3..127 defensively decode as Hann. All high-bit codes are valid morph codes, including 255 (Tukey anchor); no table is indexed directly by these codes. Input invalidity is handled before encoding.

Thus no additional record padding is required or assumed. Grain-record, track and engine-pool sizes do not grow on any ABI from this encoding change. The optional morph byte is the *existing* experimental window byte, not a new persisted field. Its high-bit meanings are compiled out when morph is disabled. Private live grain state is never serialized.

`gr_experiment_win` decodes the grain's captured value. It never reads the current test controls. Changing mode, value or discrete selector leaves old grains untouched; they finish with the captured envelope. New grains latch the latest controls. Test controls are per-track; tables remain shared const flash data, with no table rebuilding or extra audio buffers.

A future real UI would need an explicit event/block snapshot and safe cross-thread publication policy. The test globals are deliberately not exposed to MIDI, an editor or concurrent UI code in this experiment.

## Exact fixed-point arithmetic

`firmware/src/grain_morph.h:gr_morph_mix` takes nonnegative neighboring lookup results A,B in 0..32767:

- `m<64`: `floor((A*(64-m) + B*m + 32)/64)`.
- `m>=64`: n=m-64; `floor((A*(63-n) + B*n + 31)/63)`.

These are nearest-integer results; exact half ties in the /64 segment round upward. The /63 segment has no integer half tie. There is no intermediate Q15 coefficient approximation. Values 0,64,127 dispatch before blending in `gr_morph_win`, preserving all original lookup rounding exactly.

The weighted sum has coefficients totaling 64 or 63. Maximum numerator, including rounding, is **32767*64+32 = 2,097,120**, inside signed int32. Convexity keeps the output between A and B and within 0..32767 without saturation. Existing per-table interpolation remains unchanged. The new divisions are by constants 64 and 63 at block endpoints, not variable divisors in the per-sample loop. No callback float, exponential, runtime table generation, additional ADPCM read, RNG draw, allocation, lock or voice-state change was added.

The second segment has 63 intervals: values 80/96/112 correspond to approximately 25.40% / 50.79% / 76.19% of Hann→Tukey, not exact quarters. This follows the requested 64..127 anchor positions.

## Reproducible tests

Use the existing generated headers and pinned engine dependencies; no new dependency was installed:

```sh
python3 tests/grain_morph_compare.py \
  --base-tree /path/to/clean/ccf1e22-worktree \
  --out /tmp/grain-morph-comparison
```

The runner verifies the base SHA and clean source, builds the same audio fixture against both trees and executes numerical, kernel, latching, identity, sweep and listening tests. Final kernel/latch coverage was also run directly after expanding long-lifetime/default-state assertions; the current runner includes those final files.

| Test | Result |
|---|---|
| `grain_morph_numeric_test.c` | **85,344,961 checks passed**, strict ASan/UBSan |
| `grain_morph_kernel_test.c` | **3,315,208 checks passed**, strict ASan/UBSan |
| `grain_morph_latch_test.c` | **25,277 checks passed**, integration sanitizer policy |
| Off/anchor PCM comparison matrix | **1,008 renders passed** across 252 discrete/anchor cases |
| All-value sounding sweep | **512 renders passed** (all 128 values × four personalities) |
| Independent and changing per-track controls | **56 renders passed** |
| Total audio integrations | **1,576 successful sanitized renders** |

No assertions were weakened and no prior goldens were regenerated. Existing three-window tests were not edited.

### Numerical and kernel coverage

The strict targets run with `-O1 -g -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=all`, with **no category exclusions**, including signed overflow checks.

The numerical test exhausts all **8,388,608 effective Q32 index/fraction combinations for each exact anchor**, including both ends of every discarded-low-nine-bit class. It covers UINT32_MAX and the complete effective phase domain without claiming to enumerate every raw 32-bit integer individually. It checks every morph against a wide reference across every table interval and difficult fractions, adds one million random phase/value cases, and exhausts every morph against 32,768 possible A values and four boundary B patterns. Bounds and rounding match the independent int64 reference.

The kernel executes actual production `gr_run` for **every morph value**, both directions, signed sample extremes, Q16 rates 256/32768/65536/131072, and lengths 16,17,31,32,33,441,86432. Output and pool canaries, record/reservation assertions, invalid low-bit code fallback and real reverse ADPCM cache boundaries are checked. Additionally, every morph renders a complete **86,432-frame lifetime** at the 2× rate cap in **both directions** over the full SYNTH source, with nonzero energy and canaries. This exercises long phase progression and repeated reverse-cache refills, not only a long initial countdown.

The latch fixture tests all 128 values on all four tracks, changes values and valid/invalid modes while comparing cloned running pools to completion, verifies OFF-to-ON transitions on discrete grains, checks the following grain's new tag, verifies initial mode OFF/value64, and exhausts invalid mode, morph and discrete-selector byte ranges. Existing grains keep their byte and audio; per-track controls remain independent.

Audio/latch integration retains the repository's prior exclusions: signed-integer-overflow, shift, bounds, object-size and pointer-overflow. No new exclusion was introduced. No sanitizer diagnostic occurred; these integration runs are not claimed as strict whole-mixer numerical proofs.

### Exact comparisons and sounding coverage

84 states = four personalities × seven sources (CELLO 0, FLUTE 2, GUITAR 3, SYNTH 13, VOX 14, synthetic user zone, sine fallback) × three SIZE/personality-A scenarios (0/0, 80/127, 127/127). These include FREEZE HOLD extremes. Each state compares all three selectors against:

1. Previous three-window experiment.
2. Current code with morph **compiled out**.
3. Current morph-capable code with runtime mode **OFF**.
4. Current code with runtime mode ON at corresponding anchor (64/127/0).

All four PCM buffers are **byte-identical** for each of the **252 cases**. The original Hann identity is inherited from the previous exact-legacy comparison and directly preserved here at morph64. All **12 raw listening-anchor WAVs** also match the previous discrete recordings byte-for-byte, including their headers.

All 128 controls render with GUITAR in all four personalities and four simultaneous tracks. Active-grain forward/reverse observations and grain/part/track sizes match across values. The independent-track and live tests add all seven sources per personality. Independent settings are 16/53/90/127; live controls advance independently between blocks. Each sample-backed run asserts both directions and meaningful nonzero output. Sine fallback creates no grains and correctly remains unaffected. Synthetic user testing uses a legal descriptor over valid ADPCM test material, without uploading or modifying user flash/journal data.

All audio runs use explicit nonzero master 4096, DENS/RAND 127, TONE 127 and CUTOFF 0, with identical seeds, notes and source parameters for comparisons. Audio output canaries and actual channel-level signals are checked.

## Preserved short-grain limitation

The kernel explicitly confirms **20,480 single-block cases are silent**. A grain starting and ending within CTL=32 can have zero window gain at both evaluated endpoints, so the unchanged block ramp is zero throughout. Near-one-block shapes can also be poorly represented. Morphing does not fix this existing limitation, and a zero buffer is not counted as a successful sounding integration. The complete long-grain tests and audio matrices separately require nonzero energy/output.

No new interpolation frequency or sub-block rendering was introduced. Resolving this limitation needs a separately approved experiment with explicit Hann compatibility policy.

## Listening WAVs and measurements

Every file contains **131,072 stereo frames at 44.1 kHz**, approximately **2.972 seconds**, PCM16, **524,332 bytes**. Raw files preserve mixer integers exactly and are checked to fit PCM16; no clipping, normalization, trimming or resampling is applied. Source recordings remain untouched.

Matched files are derived offline only. For each personality, target RMS is `min(32767*10^(-24/20),0.8*minimum(raw RMS across the nine listening values))`. All gains attenuate. Within-personality matched RMS spread is below **0.000028 dB** after PCM16 rounding. RMS matching removes gross level bias but is not perceptual/LUFS loudness matching. No subjective audition or better-sound claim is made.

[All measurements](audio/grain-window-morph/measurements.csv) contains **820 current morph measurements**: 252 anchor states, 512 sweep states and 56 independent/live states. Previous/off buffers were compared during the 1,008-run identity matrix but are not duplicated in that CSV. [SHA-256 manifest](audio/grain-window-morph/SHA256SUMS) covers all 72 WAVs.

| Morph | CLOUD raw / matched | FREEZE raw / matched | STUTTER raw / matched | SHIMMER raw / matched |
|---|---|---|---|---|
| 0 | [raw](audio/grain-window-morph/cloud-m000-raw.wav) / [matched](audio/grain-window-morph/cloud-m000-matched.wav) | [raw](audio/grain-window-morph/freeze-m000-raw.wav) / [matched](audio/grain-window-morph/freeze-m000-matched.wav) | [raw](audio/grain-window-morph/stutter-m000-raw.wav) / [matched](audio/grain-window-morph/stutter-m000-matched.wav) | [raw](audio/grain-window-morph/shimmer-m000-raw.wav) / [matched](audio/grain-window-morph/shimmer-m000-matched.wav) |
| 16 | [raw](audio/grain-window-morph/cloud-m016-raw.wav) / [matched](audio/grain-window-morph/cloud-m016-matched.wav) | [raw](audio/grain-window-morph/freeze-m016-raw.wav) / [matched](audio/grain-window-morph/freeze-m016-matched.wav) | [raw](audio/grain-window-morph/stutter-m016-raw.wav) / [matched](audio/grain-window-morph/stutter-m016-matched.wav) | [raw](audio/grain-window-morph/shimmer-m016-raw.wav) / [matched](audio/grain-window-morph/shimmer-m016-matched.wav) |
| 32 | [raw](audio/grain-window-morph/cloud-m032-raw.wav) / [matched](audio/grain-window-morph/cloud-m032-matched.wav) | [raw](audio/grain-window-morph/freeze-m032-raw.wav) / [matched](audio/grain-window-morph/freeze-m032-matched.wav) | [raw](audio/grain-window-morph/stutter-m032-raw.wav) / [matched](audio/grain-window-morph/stutter-m032-matched.wav) | [raw](audio/grain-window-morph/shimmer-m032-raw.wav) / [matched](audio/grain-window-morph/shimmer-m032-matched.wav) |
| 48 | [raw](audio/grain-window-morph/cloud-m048-raw.wav) / [matched](audio/grain-window-morph/cloud-m048-matched.wav) | [raw](audio/grain-window-morph/freeze-m048-raw.wav) / [matched](audio/grain-window-morph/freeze-m048-matched.wav) | [raw](audio/grain-window-morph/stutter-m048-raw.wav) / [matched](audio/grain-window-morph/stutter-m048-matched.wav) | [raw](audio/grain-window-morph/shimmer-m048-raw.wav) / [matched](audio/grain-window-morph/shimmer-m048-matched.wav) |
| 64 | [raw](audio/grain-window-morph/cloud-m064-raw.wav) / [matched](audio/grain-window-morph/cloud-m064-matched.wav) | [raw](audio/grain-window-morph/freeze-m064-raw.wav) / [matched](audio/grain-window-morph/freeze-m064-matched.wav) | [raw](audio/grain-window-morph/stutter-m064-raw.wav) / [matched](audio/grain-window-morph/stutter-m064-matched.wav) | [raw](audio/grain-window-morph/shimmer-m064-raw.wav) / [matched](audio/grain-window-morph/shimmer-m064-matched.wav) |
| 80 | [raw](audio/grain-window-morph/cloud-m080-raw.wav) / [matched](audio/grain-window-morph/cloud-m080-matched.wav) | [raw](audio/grain-window-morph/freeze-m080-raw.wav) / [matched](audio/grain-window-morph/freeze-m080-matched.wav) | [raw](audio/grain-window-morph/stutter-m080-raw.wav) / [matched](audio/grain-window-morph/stutter-m080-matched.wav) | [raw](audio/grain-window-morph/shimmer-m080-raw.wav) / [matched](audio/grain-window-morph/shimmer-m080-matched.wav) |
| 96 | [raw](audio/grain-window-morph/cloud-m096-raw.wav) / [matched](audio/grain-window-morph/cloud-m096-matched.wav) | [raw](audio/grain-window-morph/freeze-m096-raw.wav) / [matched](audio/grain-window-morph/freeze-m096-matched.wav) | [raw](audio/grain-window-morph/stutter-m096-raw.wav) / [matched](audio/grain-window-morph/stutter-m096-matched.wav) | [raw](audio/grain-window-morph/shimmer-m096-raw.wav) / [matched](audio/grain-window-morph/shimmer-m096-matched.wav) |
| 112 | [raw](audio/grain-window-morph/cloud-m112-raw.wav) / [matched](audio/grain-window-morph/cloud-m112-matched.wav) | [raw](audio/grain-window-morph/freeze-m112-raw.wav) / [matched](audio/grain-window-morph/freeze-m112-matched.wav) | [raw](audio/grain-window-morph/stutter-m112-raw.wav) / [matched](audio/grain-window-morph/stutter-m112-matched.wav) | [raw](audio/grain-window-morph/shimmer-m112-raw.wav) / [matched](audio/grain-window-morph/shimmer-m112-matched.wav) |
| 127 | [raw](audio/grain-window-morph/cloud-m127-raw.wav) / [matched](audio/grain-window-morph/cloud-m127-matched.wav) | [raw](audio/grain-window-morph/freeze-m127-raw.wav) / [matched](audio/grain-window-morph/freeze-m127-matched.wav) | [raw](audio/grain-window-morph/stutter-m127-raw.wav) / [matched](audio/grain-window-morph/stutter-m127-matched.wav) | [raw](audio/grain-window-morph/shimmer-m127-raw.wav) / [matched](audio/grain-window-morph/shimmer-m127-matched.wav) |

### Representative objective results

CLOUD, fixed source/settings, raw RMS / peak / maximum adjacent same-channel jump:

| Morph | RMS | Peak | Max jump | Total squared energy |
|---:|---:|---:|---:|---:|
| 0 | 2112.303 | 13407 | 1475 | 1169640727358 |
| 16 | 2248.302 | 13936 | 1438 | 1325101307294 |
| 32 | 2409.715 | 14459 | 1418 | 1522199059402 |
| 48 | 2591.391 | 14970 | 1506 | 1760377241034 |
| 64 | 2788.967 | 15484 | 1598 | 2039044566744 |
| 80 | 2993.915 | 15635 | 1760 | 2349734383758 |
| 96 | 3226.082 | 15781 | 1980 | 2728290456434 |
| 112 | 3479.284 | 16367 | 2196 | 3173363049296 |
| 127 | 3727.465 | 16684 | 2397 | 3642227020000 |

Across the 128-control GUITAR sweeps, RMS increases monotonically in these particular renders. Largest adjacent-control RMS change is **0.042265 dB** across the four personalities. This is a measured smooth level trend, not a universal monotonic perceived-loudness guarantee. Peak and sample-jump statistics need not be monotonic.

Live GUITAR maximum sample jumps are **1605 / 1828 / 1328 / 3376** for CLOUD/FREEZE/STUTTER/SHIMMER. None of 28 live tests exceeded twice the comparable static maximum (a deliberately loose suspicion heuristic, not an audibility threshold or a click-free proof). Source phase, overlaps and existing gates contribute to sample differences. No active-envelope mutation was found by latch tests. Listening review remains necessary.

Intermediate results can differ slightly from the audit's offline pre-blended-table probes: this implementation blends already-interpolated neighboring lookups with declared integer rounding. Exact anchors are unaffected.

## Host resources and target-build status

Identical `cc -Os -ffp-contract=off` builds of the same new audio/canary fixture:

| Build | text/rodata bytes | data | BSS |
|---|---:|---:|---:|
| Previous three-window source | 535324 | 19660 | 310568 |
| Current, morph compiled out | 535324 | 19660 | 310568 |
| Current, morph compiled in | 535868 | 19676 | 310568 |
| Enabled delta | **+544** | **+16** | **0** |

The previous/current morph-disabled fixture objects are byte-identical. Native browser unity objects with only the discrete-window flag are also byte-identical (text 886668, data 31060, BSS 1603232). The shipping/no-experiment unity object is also byte-identical to the retained base (text 885503, data 31060, BSS 1603232). These are compiler-object identities, not successful target links. A morph-capable object is naturally different even when its runtime mode starts OFF; PCM identity is the applicable runtime guarantee.

There are **zero new table/rodata arrays**: the existing three 514-byte windows are reused. The eight logical test-control bytes (four mode, four value) occupy one four-byte BSS symbol and one four-byte data symbol. Aggregate object data grows by 16 due to layout/alignment; BSS fits existing padding at this measurement. This is real test-control state, not an assertion of zero added state. It is compiled out by default.

Grain record stays **64 host bytes**, GRAIN pool **7,296 bytes/track**, common track **1,840 bytes**, engine reservation **12,888 bytes/track / 51,552 total**. No record, track, source-index, reverse-cache or audio-buffer expansion occurs. A normal 32-bit grain ABI is expected to remain 56 bytes because only byte values changed, but no target measurement is claimed.

Host stack reports: `gr_spawn_one` **112→112 bytes**; `grain_render` **336→328**; outlined `gr_experiment_win` **8→16**. Caller reduction and helper growth reflect compiler layout, not a proven target stack saving. No new per-sample stack buffer exists.

An intermediate window needs two table lookups and one blend per existing endpoint, versus one lookup for an anchor/discrete grain. At 48 occupied slots, that is at most 96 extra lookups per 32-frame block (three aggregate extra lookups per output frame across tracks), plus blends. This is a static operation count, not measured target cycles. No ADPCM decode/load charge, source-head speed, scheduler RNG order or admission cap changed; hardware CPU deadline needs profiling.

The unchanged `./build.sh` target preflight failed with exact error:

```
build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)
```

No toolchain was installed/downloaded, no firmware link occurred, and no flash/RAM fit or target timing is claimed. A future experimental target build must explicitly enable both experiment flags; a default-off link alone would not validate the morph path. The **581,564-byte image limit remains unverified**.

## Readiness and changed files

**DSP morphing is validated for further experimental integration work.** Eventual GRAIN EDIT integration still needs an approved public control/descriptor, storage/default/downgrade policy, editor/MIDI/motion routing, safe control publication and actual target link/deadline/stack measurements. This task adds none of those interfaces. Keep exact legacy Hann, direct anchors and spawn latching when designing that next phase. Short-grain behavior and subjective listening remain explicit limitations.

Changed production-source scope is only the guarded experiment additions in `eng_grain.c` plus the new pure helper `grain_morph.h`. New tests are `grain_morph_{numeric,kernel,latch,audio}_test.c` and `grain_morph_compare.py`; documentation includes this report, measurements/checksums and 72 listening captures. Existing tables, generators, UI, presets, IDs, original sample assets and formats are unchanged. No merge, deployment or flashing.
