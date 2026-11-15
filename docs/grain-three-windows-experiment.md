# Three-window GRAIN DSP experiment

Base: `experiment/coffee-grains-grain-windows`, **aeed25c6869a0735399b7c7adb494e7c3a9c439d**.
Branch: `experiment/coffee-grains-three-windows`.
Reference audit: **a012723f699721f9834804050596e82af31720e4** (not merged).

## Result and scope

The test-only experiment now supports **0 HANN / 1 TUKEY / 2 GAUSS**. Hann remains the exact legacy default. Gaussian is the endpoint-corrected k=9 envelope requested in the audit. All three are latched per newly spawned grain. Invalid selector bytes 3..255 safely become Hann; the lookup also treats an invalid latched byte as Hann.

Gaussian is ready for a **separate, opt-in latched linear-morph experiment**, subject to the short-grain limitation below. This is not approval of a shipping control, preset format or hardware release. No UI, parameter IDs, preset defaults, modulation routing, serialized fields, personalities, sample recordings, ADPCM decoder, source-head behavior, grain scheduling, overlap normalization or admission limits changed. No normalization was added to firmware.

## Minimal DSP addition

`firmware/src/eng_grain.c` adds the Gaussian header inside the existing `FELUCCA_GRAIN_WINDOW_EXPERIMENT=1` guard, admits selector 2 at spawn, and dispatches that grain's endpoint lookup to `gr_gauss_win`. The existing `GR_HANN`, `gr_win`, Tukey header/helper and `gr_run` approximation remain unchanged. There is no active-grain table mutation: changing `gr_test_window` changes only future admitted grains. The selector remains translation-unit-local and accessible only to test builds; it is not a public protocol/UI control or persisted setting.

`tools/gen_grain_gauss.py` independently generates `firmware/src/grain_gauss.h` offline:

`g(t) = (exp(-9*(2*t-1)^2) - exp(-9)) / (1-exp(-9)), t=i/256`.

The generator computes the first 129 nodes, quantizes with explicit nearest rounding `floor(32767*g+0.5)`, forces endpoints to zero and center to 32767, and mirrors the first half exactly. There are **257 int16 Q15 entries / 514 const bytes**. The table is const/rodata (mapped to XIP by the existing linker script), not a runtime RAM buffer. No GRAINE implementation or table was copied.

Lookup uses the existing Q32 phase interpretation: index `phase>>24`, fraction `(phase>>9)&32767`, then floor-rounded interpolation between adjacent nodes. The product magnitude is at most `32767*32767`, and the explicit negative-floor adjustment fits int32. Index+1 never exceeds 256. The only new division is by the compile-time constant 32768 at an endpoint lookup; there is **no variable division, float, exponential, allocation or table generation added to the audio callback**. Existing unrelated arithmetic/divisions are not rewritten.

## Reproducible validation

With existing generated headers and pinned engine dependencies available:

```sh
python3 tools/gen_grain_tukey.py --check
python3 tools/gen_grain_gauss.py --check
python3 tests/grain_three_window_compare.py \
  --base-tree /path/to/clean/aeed25c-worktree \
  --legacy-tree /path/to/clean/d9e352f-worktree \
  --out /tmp/grain-three-window-comparison
```

The runner verifies both exact base SHAs/clean source, builds the fixtures, executes strict numerical/kernel tests and retained regressions, checks the generators, compares PCM and emits listening WAVs, measurements and checksums. No existing assertion was removed or weakened. Prior Hann/Tukey fixture behavior remains testable with the existing comparison runner.

### Results

| Test | Exact result |
|---|---|
| Existing Tukey numerical test | **8,000,773 checks passed** |
| New Gaussian numerical test | **33,555,205 checks passed** |
| Three-window latch test | **146,461 checks passed** |
| Expanded production grain-kernel test | **45,289 checks passed** |
| Audio matrix | **84 states × 7 renders = 588 successful renders** |
| Retained source position/boundary regression with Gaussian selected | **854,920 checks passed** |
| Retained GRAIN-only policy regression with Gaussian selected | **380 checks passed** |

The Gaussian numerical test exhausts **8,388,608 distinct index/fraction combinations**, checking both endpoints of each 512-value low-bit equivalence class. Those nine phase bits are discarded by the lookup, so this covers the full effective Q32 input space without claiming to iterate all 2^32 raw values. It includes UINT32_MAX, table knots, zero/center, mathematical formula error, symmetry, bounded interpolation, and a wide-integer reference.

The kernel test invokes actual production `gr_run` under **strict ASan/UBSan with no category exclusions**, across three windows, forward/reverse, signed source extremes, Q16 rates 256/32768/65536/131072, and lengths 16/17/31/32/33/441/86432 frames. It additionally builds a real factory seek index and renders maximum-rate reverse grains through at least five reverse-cache refill boundaries for each window, with output/pool canaries and nonzero-energy checks. It checks invalid latched-window fallback, engine-memory capacity and expected grain-record size.

Strict flags for numerical/kernel targets: `-O1 -g -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=all`. Signed-overflow checks are enabled. No diagnostics occurred.

The full UI/mixer integration and latch fixtures retain the pre-existing host-suite exclusions: signed-integer-overflow, shift, bounds, object-size and pointer-overflow. They use ASan/UBSan with no new exclusions. These are not represented as fully strict whole-mixer numerical proofs. Retained regression wrappers set the selector to Gaussian before invoking the unchanged original test body; manually fabricated zero-initialized grains within those fixtures still default to Hann. The wrapper renames only the test entry point; it changes no assertion.

### PCM identity and audio coverage

Each of the 84 identical input states renders:

1. Original legacy d9e352f Hann.
2. Previous aeed25c experimental Hann.
3. Previous aeed25c Tukey.
4. Current normal/default build Hann.
5. Current experimental Hann.
6. Current experimental Tukey.
7. Current experimental Gaussian.

All **84 Hann comparisons are byte-identical across all four Hann builds**. All **84 Tukey comparisons are byte-identical to the previous experiment**. For 72 sample-backed states, all three windows produce meaningfully nonzero and mutually different audio. For 12 sine-fallback states, all three are byte-identical: fallback creates no grains, so a grain-window selector must not change it.

Coverage is four personalities × seven sources (CELLO 0, FLUTE 2, GUITAR 3, SYNTH 13, VOX 14, synthetic user-zone fixture, empty-slot sine fallback) × three scenarios:

- SIZE 0 / personality A 0, including minimum FREEZE HOLD.
- SIZE 80 / personality A 127.
- SIZE 127 / personality A 127, including maximum FREEZE HOLD.

Every run has four simultaneous sounding tracks, explicit master 4096, DENS 127, RAND 127, TONE 127 and CUTOFF 0. Every sample-backed run observes forward and reverse grains; active-grain direction observations, grain bytes, part bytes and track bytes agree across all three windows. Every audio block has output canaries. User-source testing is a synthetic legal zone pointing at valid ADPCM test material; no actual user flash upload, journal or allocator operation is performed.

The latch test completes old grains after changing the selector, comparing cloned pools/PCM and verifying the following grain uses the new choice. It covers all personalities, SIZE/HOLD endpoints and both directions. It tests **every invalid byte 3..255**, not only 255. No existing grain changes its window during its lifetime.

## Known short-grain silence remains intentionally unchanged

**480 direct-kernel single-block cases remain silent**, explicitly asserted across all three windows. If a grain begins and ends in one CTL=32 render call, both window endpoints are zero and the existing linear block ramp can be zero throughout. Very short near-one-block envelopes can also be poorly approximated. This is the previously documented renderer limitation, not a Gaussian regression, and it was not repaired here.

Ordinary minimum SIZE grains are longer; boundary fitting may create these tiny grains. The audio matrix's sample-backed states remain nonzero. Passing the known-silence assertions is not offered as evidence that every short grain sounds. Improving intra-block window evaluation requires a separate experiment and legacy-identity decision.

## Raw and offline RMS-matched listening files

All captures are real GUITAR/source-3 four-track renders, identical seed, notes and parameters, scenario 1. Every WAV contains all **131,072 stereo frames at 44.1 kHz**, about **2.972 seconds**, PCM16, **524,332 bytes**. The raw exporter preserves actual mixer integers and refuses out-of-range PCM16 rather than clipping or silently rescaling. No source recording was trimmed, resampled or normalized.

The newly generated raw Hann/Tukey WAVs were byte-compared to the existing committed captures and are reused below rather than duplicated. New Gaussian raw captures and all three matched versions are under `docs/audio/grain-three-windows/`.

| Personality | Raw Hann | Raw Tukey | Raw Gaussian | Matched Hann | Matched Tukey | Matched Gaussian |
|---|---|---|---|---|---|---|
| CLOUD | [WAV](audio/grain-windows/cloud-hann.wav) | [WAV](audio/grain-windows/cloud-tukey.wav) | [WAV](audio/grain-three-windows/cloud-gauss-raw.wav) | [WAV](audio/grain-three-windows/cloud-hann-matched.wav) | [WAV](audio/grain-three-windows/cloud-tukey-matched.wav) | [WAV](audio/grain-three-windows/cloud-gauss-matched.wav) |
| FREEZE | [WAV](audio/grain-windows/freeze-hann.wav) | [WAV](audio/grain-windows/freeze-tukey.wav) | [WAV](audio/grain-three-windows/freeze-gauss-raw.wav) | [WAV](audio/grain-three-windows/freeze-hann-matched.wav) | [WAV](audio/grain-three-windows/freeze-tukey-matched.wav) | [WAV](audio/grain-three-windows/freeze-gauss-matched.wav) |
| STUTTER | [WAV](audio/grain-windows/stutter-hann.wav) | [WAV](audio/grain-windows/stutter-tukey.wav) | [WAV](audio/grain-three-windows/stutter-gauss-raw.wav) | [WAV](audio/grain-three-windows/stutter-hann-matched.wav) | [WAV](audio/grain-three-windows/stutter-tukey-matched.wav) | [WAV](audio/grain-three-windows/stutter-gauss-matched.wav) |
| SHIMMER | [WAV](audio/grain-windows/shimmer-hann.wav) | [WAV](audio/grain-windows/shimmer-tukey.wav) | [WAV](audio/grain-three-windows/shimmer-gauss-raw.wav) | [WAV](audio/grain-three-windows/shimmer-hann-matched.wav) | [WAV](audio/grain-three-windows/shimmer-tukey-matched.wav) | [WAV](audio/grain-three-windows/shimmer-gauss-matched.wav) |

Offline matched copies use attenuation only. For each personality, target RMS is `min(32767*10^(-24/20), 0.8*minimum(raw RMS across three windows))`. Quantization follows gain application; every result is checked within PCM16 bounds. Measured within-personality RMS spread is below **0.000006 dB**. This is RMS matching, **not perceptual/LUFS loudness matching**. No normalization runs in firmware, no note timing/source duration changes, and no subjective quality ranking is claimed.

### Measurements and differences

[Full measurements CSV](audio/grain-three-windows/measurements.csv) contains all 252 current window/state measurements, including peak, RMS, nonzero count, total squared energy, squared adjacent differences, maximum same-channel adjacent jump and direction observations. [Checksums](audio/grain-three-windows/SHA256SUMS) cover the 16 newly stored WAVs.

Representative raw-level RMS / peak / maximum adjacent jump:

| Personality | Hann | Tukey | Gaussian |
|---|---|---|---|
| CLOUD | 2788.967 / 15484 / 1598 | 3727.465 / 16684 / 2397 | 2112.303 / 13407 / 1475 |
| FREEZE | 2946.172 / 16639 / 1805 | 3930.291 / 17184 / 1870 | 2234.335 / 14537 / 1663 |
| STUTTER | 2766.152 / 16838 / 1405 | 3717.660 / 16912 / 1584 | 2094.134 / 16354 / 1378 |
| SHIMMER | 2988.285 / 18571 / 3365 | 3964.533 / 18405 / 3308 | 2253.008 / 18608 / 2944 |

For CLOUD, total output energies are **2,039,044,566,744** (Hann), **3,642,227,020,000** (Tukey), **1,169,640,727,358** (Gaussian). Across these four examples Gaussian RMS is 2.40–2.45 dB below Hann and Tukey is 2.46–2.57 dB above Hann; unchanged overlap normalization intentionally preserves that difference. Gaussian peak is not uniformly lower (see SHIMMER). Adjacent differences are signal-dependent, not a complete click detector. Equal mathematical window energy would not guarantee identical perceived loudness against correlated source samples.

## Host resources and callback cost

Identical host `cc -Os -ffp-contract=off` object comparisons use the same updated audio/canary harness against the previous and current engine sources:

| Object build | text/constants | data | BSS |
|---|---:|---:|---:|
| Previous two-window experiment | 534527 | 19660 | 310568 |
| Current three-window experiment | 535091 | 19660 | 310568 |
| Delta | **+564** | **0** | **0** |

Of that host delta, **514 bytes** are the Gaussian table (`nm` read-only symbol size 0x202); remaining code/alignment is 50 bytes. This is a **host test-fixture object** measurement, not target firmware flash use. The disabled/default native browser unity object remains **byte-identical** to the exact legacy base: text 885503, data 31060, BSS 1603232. Default footprint delta is zero.

State remains **64-byte host grain record, 7,296-byte private GRAIN state per track, 1,840-byte common track object**, with 12,888 bytes reserved per track (51,552 total). Existing experimental selector and window byte are reused; no new state or buffer is allocated. Ordinary 32-bit grain layout is expected to remain 56 bytes, but no target ABI measurement was made. Tests assert engine reservation and expected record layouts.

Host stack-usage reports: `grain_render` **336→336 bytes**, `gr_spawn_one` **112→112**, harness main **448→448** against the two-window experiment. The inherited experiment already used eight more renderer-frame bytes than pre-experiment Hann; Gaussian adds none in this host compile. These are individual compiler frames, not target stack high-water guarantees.

Added callback work is a bounded selector clamp at spawn and one extra choice in the two endpoint lookup dispatches. Gaussian uses the same number of table reads/interpolation operations as Tukey; the per-sample decode/render loop is unchanged. No runtime window generation, variable division, modulation recalculation, extra RNG call, source decode, audio buffer or worker synchronization was added. Target instruction timing/cache effects remain unmeasured.

## Readiness and limits

**Ready for the next isolated morphing prototype:** Gaussian is bounded, reproducible, latched, sounding and preserves both validated legacy choices. Start with convex fixed-point neighboring-window blends latched at spawn and exact Hann/Tukey/Gaussian anchors. Do not mutate active grains or silently add energy compensation. Audit rounding, gain bounds, state packing and partial-block behavior separately.

**Not shipping-ready:** short-grain fidelity remains limited; subjective audition is still needed; no persistent control or downgrade policy has been designed. No browser build or target build/link was performed. The default object comparison proves unchanged native default code, not a successful FM1 link. Hardware flash fit against **581,564 bytes**, callback deadlines and target stack remain unverified. Broader UI/MIDI/motion suites were not rerun; historical failures are not claimed green.

Changed files: `eng_grain.c`; new `grain_gauss.h` and generator; expanded existing audio/kernel/latch fixtures; new Gaussian numerical test and three-window comparison runner; this report, measurement CSV/checksums and listening captures. Original Hann/Tukey tables/helpers/generator and all original factory/user recordings remain unchanged. No merge, deployment or flashing.
