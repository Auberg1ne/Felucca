# Gaussian envelopes and grain-window morphing audit

## Scope, source identity and conclusion

Base: `experiment/coffee-grains-grain-windows`, **aeed25c6869a0735399b7c7adb494e7c3a9c439d**. Audit branch: `audit/coffee-grains-grain-envelope-morphing`. Architecture reference: `f4f23dbe65cedff297750ff4e13034f5d3726423`.

**Recommendation:** investigate an endpoint-corrected Gaussian with exponent coefficient 9 as a third discrete window, retaining exact legacy Hann. Support a latched, linear neighboring-window morph only after the short-grain approximation and persistent-control design are resolved. Keep tapered rectangular experimental initially; defer a separate skew control. No production feature, control, ID, format, preset, UI or asset was changed here.

Actual reference source: the supplied `Granular-FM1-main.zip`, SHA-256 `76b5f034393832b06dd9e6fab8cc279cc858195c4eedbc44a7b72d4ba0b970c1`, extracted to `/tmp/graine-reference/Granular-FM1-main`. There is no upstream Git commit metadata in that archive. GRAINE paths/line numbers below refer to these bytes. COFFEE GRAINS paths refer to the exact base above. Reference documents were read as evidence, not instructions.

All experiments were isolated under **`/workspace/work/grain-envelope-probes`**, outside both repositories. An external disposable copy of the existing host fixture/engine was instrumented for measurements; the actual working firmware was not modified. Only this report is committed. Numerical measurements are not subjective listening judgments or hardware measurements.

## 1. What GRAINE actually does

GRAINE `app/dsp/engine.c`:

| Location | Verified behavior |
|---|---|
| 12–13 | Control chunk `CH=64`, window table `WIN_N=512` |
| 101–103 | Four base float tables and one current float table, each 513 entries; cached shape/skew |
| `shape_base`, 104–112 | Gaussian `exp(-9*x*x)`, `x=2*t-1`; Hann `0.5-0.5*cos(2*pi*t)`; Tukey with flat central half; rectangle-like flat center `abs(x)<0.94` and cosine-tapered edges |
| `win_init`, 114–120 | Base tables generated once using the platform fastmath functions |
| `base_at`, 122–129 | Linear interpolation in a base table; explicit final-entry case |
| `win_make`, 132–151 | Normalized morph `m` multiplied by 3; adjacent shapes in GAUSS→HANN→TUKEY→RECT order. For `m<1`, segment `k=floor(3*m)`, blend `a=3*m-k`; at `m=1`, `k=2,a=1`. Linear amplitude blend, not equal-energy normalization |
| 136, 145–147 | Skew peak `pk=0.5+0.47*skew`; piecewise phase warp maps `[0,pk]` to `[0,0.5]` and `[pk,1]` to `[0.5,1]` |
| 137–143 | Skip rebuild only when both shape and skew changed by less than 0.002 from the cached values; otherwise rebuild all 513 current entries |
| 149–150 | Force current window's first and last entries to zero, after morph/skew |
| `eng_render`, 1667–1672 | Once per control chunk, read global normalized `gn[P_WIN]`/`gn[P_SKEW]` and call `win_make` before grain workers run |
| `grain_run`, 911–925 | Every output sample interpolates the current global window using Q32 grain phase; source interpolation and window multiplication are also per sample |

`app/ui/params.c:65–66` defines SHAPE raw 0..100, default 33, and SKEW -100..100, default 0. `pval`, `engine.c:56`, maps normalized skew to -1..1. Consequently peak lies at approximately 0.03..0.97, never at a singular endpoint. SHAPE=33 is nominally 99% of the way from Gaussian to Hann, **not an exact Hann anchor** if normalized to 0.33. Our legacy default must not inherit that quantization choice.

### Endpoints, active grains and discontinuity risks

The mathematical GRAINE Gaussian has nonzero endpoints `exp(-9)=0.0001234098`. `win_make` forcibly replaces its endpoint samples with zero; it does not subtract the Gaussian baseline across the table. The rectangular choice is **tapered**, not a hard rectangular gate: each edge occupies 3% of duration (Tukey alpha=0.06).

Grains do not own a private window or morph snapshot. Rebuilding `win[]` changes what **all already-active grains** read at their current phase. The 0.002 cache threshold is not interpolation in time and provides no click-free guarantee. Skew can change an active grain's effective amplitude substantially at a fixed phase. For illustration, our independently quantized tables have maximum pointwise deltas of 0.562426 for Hann→Tukey, 0.999756 for corrected Gaussian9→tapered rectangle, and 0.974792 for Gaussian9 skew -0.5→+0.5. These are possible amplitude changes, **not measured GRAINE output clicks**.

Window evaluation is audio-rate, but global table updates are control-rate. Their boundaries depend on render/chunk partitioning; a 513-entry rebuild and float skew divisions add callback work. Workers see one completed table during a chunk in this app-level code; actual cross-core memory visibility depends on its platform. The ZIP omits the `base/` submodule/fastmath/platform implementation. Therefore exact approximation errors, callback cycles, synchronization and GRAINE audio could not be executed here. Formula probes use standard offline math, not a claim of bit-identical GRAINE fastmath.

## 2. Gaussian choices for our Q15 system

Use `x=2*t-1` for `t in [0,1]`. Compare:

- **Forced-endpoint Gaussian:** `g(t)=exp(-k*x*x)` for interior entries; explicitly zero endpoints.
- **Endpoint-corrected Gaussian:** `g0(t)=(exp(-k*x*x)-exp(-k))/(1-exp(-k))`. It reaches zero naturally and unity at center; quantize and explicitly set endpoints to zero anyway.

Both are useful hypotheses. The corrected formula is preferable for broad Gaussians because simply zeroing a substantial tail creates an edge step. It does not make the endpoint derivative zero; it removes the finite baseline. Coefficient 9 is a practical first candidate, **not a proven best-sounding width**.

For the uncorrected Gaussian, normalized-time sigma is `1/sqrt(8*k)`; full half-amplitude width is `sqrt(log(2)/k)`. At k=4/9/16 these are approximately 41.6% / 27.8% / 20.8% of grain duration. Larger k gives a narrower pulse and less overlap energy at the same nominal density.

### Measured 257-node Q15 tables

Values below come from our exact retained Hann/Tukey tables and independent Gaussian/taper generation, not GRAINE table extraction. Energy is the trapezoidal integral of squared quantized nodes, divided by `32767^2`. It is a table metric; it differs slightly from integrating the piecewise-linear interpolant exactly and from the block-rendered audio.

| Window | Energy | Envelope RMS | First interior node | Maximum adjacent-node jump |
|---|---:|---:|---:|---:|
| Existing Hann | 0.375000 | 0.612372 | 5 | 403 |
| Existing Tukey alpha=0.5 | 0.687499 | 0.829156 | 20 | 804 |
| Gaussian k=4, forced endpoints | 0.313307 | 0.559738 | 639 | 639 |
| Gaussian k=9, forced endpoints | 0.208884 | 0.457039 | 5 | 659 |
| Gaussian k=16, forced endpoints | 0.156664 | 0.395807 | 0 | 878 |
| Gaussian k=4, endpoint-corrected | 0.308692 | 0.555601 | 39 | 448 |
| Gaussian k=9, endpoint-corrected | 0.208864 | 0.457017 | 1 | 659 |
| Tapered rectangle alpha=0.06 | 0.962491 | 0.981066 | 1352 | 6591 |
| Wider-taper rectangle alpha=0.12 | 0.925002 | 0.961770 | 341 | 3343 |

All generated Gaussian and tapered tables are exactly symmetric with zero endpoints and peaks 32767. Retained Hann has one-Q15-unit mirror differences, inherited from its existing table; **do not regenerate it just to impose symmetry**. Broad k=4 forced-endpoint edges deserve particular caution (tail `exp(-4)≈0.0183`, roughly -34.7 dB relative to peak). Endpoint correction greatly reduces its first-node jump. k=16 concentrates energy so much that low-density/short grains may expose gaps.

A chosen Gaussian costs **514 flash bytes**, with no runtime generation or RAM table. Width need not become a continuous control: one carefully tested fixed width is cheaper and easier to preserve. A second width would add another 514 bytes and another musical option, not inherently better quality.

### Interpolation and bounds

Keep 257 int16 entries, Q32 phase and Q15 linear interpolation. Phase top byte is index 0..255; the next 15 bits supply fraction 0..32767. A signed adjacent difference times that fraction has magnitude at most `32767*32767=1,073,676,289`, inside int32. Use explicit floor or declared rounding; negative right shifts must not accidentally change existing Hann rounding. The next-entry access stays within 256. Endpoints, UINT32_MAX and table knots need explicit tests.

Gaussian is an amplitude window over output-time phase, so the same table works for forward/reverse source playback. Do not reverse the decoder or source head to implement skew; asymmetric window reversal would be a separate musical policy. Keep it output-time oriented for both source directions initially. Full-length source-head, span fitting, ADPCM caches, DTUNE, DRIFT and STRETCH remain independent.

## 3. Short and long grain behavior is the main constraint

COFFEE GRAINS `firmware/src/eng_grain.c:399–427` evaluates a window at the two ends of a render segment and ramps the gain over up to **CTL=32** frames. It does not evaluate the full window on every audio sample. All four personalities share this path (`grain_render:585–589`). The approved experiment leaves it unchanged.

External gain-envelope probes emulated the exact phase increment, Q15 grain gain and endpoint ramp policy before the source multiply, then compared it with per-sample window lookup. This is an envelope model, not an independently executed full gr_run capture. The following error is RMS of the envelope difference normalized to full scale; it is **not relative error in an actual recording**.

| Length | Hann error | Gaussian9 corrected error | Tukey error | Rectangle alpha=.06 error |
|---:|---:|---:|---:|---:|
| 16 | 0.612357 | 0.457003 | 0.829148 | 0.968246 |
| 32 | 0.612359 | 0.457005 | 0.829150 | 0.984250 |
| 33 | 0.608566 | 0.456933 | 0.812561 | 0.556330 |
| 64 | 0.075451 | 0.250086 | 0.308517 | 0.544450 |
| 128 | 0.075448 | 0.046111 | 0.053349 | 0.362275 |
| 441 | 0.006446 | 0.013411 | 0.016359 | 0.117761 |
| 4410 | 0.000083 | 0.000146 | 0.000194 | 0.003892 |
| 86432 | 0.000047 | 0.000045 | 0.000040 | 0.000052 |

Grains beginning and finishing in a single block have two zero endpoints and can render **entirely silent**. At 64 frames, several distinct windows collapse to nearly the same triangle because the renderer samples their common center peak. This means a mathematically excellent short Gaussian or tiny rectangular edge is not necessarily represented faithfully by the current kernel. At 441 frames (10 ms), a 3%-duration rectangular edge is only ~13 samples, still below one control block. Narrow windows/skew exacerbate phase-dependent undersampling.

For long grains, the same approximation becomes accurate; FREEZE HOLD and normal sustained clouds are more promising first audition cases. Short/boundary-fitted grains need explicit testing independent of nominal SIZE. Remedies such as sub-block or per-sample envelope evaluation are **future DSP changes**, not applied here. Preserve legacy Hann exactly; a refined evaluation mode must be independently approved/tested rather than silently changing old presets.

## 4. Discrete selection versus continuous morphing

| Approach | Advantages | Limitations / decision |
|---|---|---|
| Discrete windows latched at spawn | Lowest cost; one existing-style lookup per endpoint; clear identity | Best first implementation. New grains change shape, existing grains finish naturally |
| Linear neighboring-window amplitude blend | Convex, bounded, deterministic; simple fixed point; only two neighboring tables | Recommended eventual morph. Energy changes with shape and coherent grain overlap |
| Equal-power sin/cos crossfade | Familiar for uncorrelated audio | These are correlated, nonnegative windows: it boosts the middle and can exceed unity; not appropriate as a default |
| Constant/equal-energy normalization | Can reduce raw envelope-energy bias | Needs declared reference energy, cross terms, headroom/gain proof; does not guarantee constant audio/perceived loudness |
| Live morph/skew of active grains | Immediate control response | Amplitude jumps unless smoothed; more work and active-state ownership; do not adopt GRAINE's global table replacement |

The sequence GAUSS→HANN→TUKEY→TAPERED RECT is coherent as increasing width/plateau/energy, although perceptual texture is not guaranteed monotonic. Hann must be an **exact anchor** and exact default, not the nearest value on a coarse scaled slider. Keep a direct legacy lookup branch at that anchor.

For neighboring envelopes A/B and fraction a, linear energy is:

`E(a)=(1-a)^2*EA + 2*a*(1-a)*C + a^2*EB`, where `C=integral(A*B)`.

Measured normalized correlations are 0.928884 (Gaussian9 corrected/Hann), 0.947516 (Hann/Tukey), and 0.921820 (Tukey/rectangle .06). At the midpoint, equal-power blending gives energies **0.551894, 1.012353, 1.574856**, respectively: it can raise the envelope peak above one. Linear blending gives **0.275947, 0.506176, 0.787428**. Scaling those linear midpoints to the mean of endpoint energies needs gains **1.028557, 1.024468, 1.023576**; even that modest boost can exceed a unity Q15 peak.

Normalizing every shape to Hann energy would need about **1.340×** amplitude for Gaussian9 corrected and about **0.624×** for rectangle .06. Applying that inside the existing grain-gain path requires renewed overflow, overlap and clipping analysis. Clipping a normalized table back to 32767 defeats equal-energy accuracy. Attenuating all shapes to the lowest energy avoids positive gain, but would change Hann unless it is a separate mode. Therefore recommend **no automatic energy compensation in the first implementation**; measure and audition level differences explicitly.

### Latch at spawn

Capture mode/morph/skew with the grain's initial state. A changed control affects future admitted grains, not active ones. Existing overlaps provide a gradual population transition; this avoids an in-flight envelope discontinuity but is not a guarantee of perceptually constant level. Low density makes the control feel slower. Audio-rate modulation of a latched value is sampled at spawn and can alias to the grain rate; document that instead of silently reusing existing modulation semantics.

Continuously updating existing grains would require independent target/current state and a precisely defined smoothing time, endpoint guarantees and per-grain update policy. Merely slewing a global morph value does not bound the amplitude derivative of every phase/skew combination. It is a separate, higher-risk feature.

## 5. Low-cost fixed-point proposal (not implemented)

1. Keep Hann/Tukey exactly as validated. Generate one endpoint-corrected Gaussian9 and, if later justified, one tapered-rectangle table offline with explicit rounding/check mode. Total four tables: **2,056 flash bytes**, +1,028 over the two-window experiment (+1,542 over Hann alone). No 10,260-byte float window buffer as in GRAINE.
2. Use a **per-grain latched compact morph code**, with an explicit zero/legacy-Hann representation. Decode to adjacent table indices and a Q15 fraction. Do not force the saved/default value to be the left end of the Gaussian sequence.
3. At each existing endpoint, lookup A and B, then compute `(A*(32768-a)+B*a+16384)>>15` for a in 0..32768. Products plus the rounding term are at most 1,073,725,440, safely inside signed int32. The sum is nonnegative; no negative-shift ambiguity. Exact anchors should use one original lookup, especially legacy Hann.
4. Preserve the original grain gain and block ramp at first. No extra source reads, random draws, decoder steps or per-sample operations. Head scanning, admission and four track ownership stay unchanged. Prototype higher-resolution short-grain evaluation separately if required.
5. Use no runtime table generation, per-track tables, heap allocation or locks. The callbacks remain fixed point. Measure linked code, integer divisions/reciprocal mapping and actual device deadlines rather than assuming host costs transfer.

A feasible **DSP-state encoding**, not a parameter allocation, is one byte: 0=legacy Hann; 1..253 traverse 252 intervals of the four-anchor sequence (84 per segment); 254/255 fall back to Hann. This gives exact Gaussian/Hann/Tukey/rectangle anchors at codes 1/85/169/253. Duplicate Hann at zero is intentional for legacy initialization. Decode only at endpoints or precompute bounded coefficients at spawn. This is an illustrative design to validate, not a new public ID or approved UI mapping.

### Structure padding and CPU estimates

Base `gr_grain_t:82–99` has owner/reverse/zone bytes and an optional experimental `window` byte. The prior experiment measured 64 bytes on the host with/without that byte; ordinary 32-bit layout is expected to be 56 bytes with that byte consuming its last alignment byte. Reusing the optional byte for a compact morph code could preserve this size. **Do not assume more target padding exists.** Adding a uint16 fraction and separate mode/indices can increase a 32-bit record to 60 bytes: up to 192 extra bytes across 48 grain slots. Compile on the target to confirm.

At 48 occupied slots, the current two endpoint lookups cost 96 lookups/control block. A two-table morph adds at most 96 extra lookups/32-frame block, or three aggregate extra lookups/output frame across four tracks, plus blends. This is a **static upper-bound count**, not measured cycles; real occupancy is further constrained by decode budgets. It does not include skew mapping or fastmath because neither is recommended in the initial callback path. No additional audio buffer is needed.

## 6. Skew: useful, separate, deferred

Skew changes attack/decay balance rather than width. Treat it as a separate musical axis if eventually exposed; combining it implicitly with the shape knob makes anchors harder to understand and recall. A symmetric base under GRAINE's piecewise warp has approximately conserved integral energy, but not constant energy against a real source waveform. Our Gaussian9 +/-0.5 skew tables both measured energy 0.208865; their actual source audio differed substantially.

Start, if approved later, with a moderate peak range such as 0.265..0.735, latch it per grain, and define the same time orientation for reverse playback. Extreme 0.03..0.97 peaks can place attacks inside a fraction of a 32-frame control block and miss the intended maximum. A fixed-point phase warp needs bounded denominators and reciprocal/multiply or division work at endpoints. Two precomputed slopes per grain would add state; static banks of skewed tables multiply flash. Neither is free. Resolve short-grain rendering fidelity before a general skew UI.

## 7. External audio experiments

**64 successful sanitized four-track renders:** 16 shapes × four personalities, source GUITAR ID 3, SIZE 80, DENS 127, RAND 127, personality A/B 127, CUTOFF 0, TONE 127, STRETCH 0, master 4096; other fixture settings identical. Each fixture asserts nonzero sound, both grain directions and all four channel levels. Shapes: Hann, Tukey, Gaussian k4/k9/k16 forced-endpoint, Gaussian k4/k9 corrected, rectangle alpha .06/.12, Gaussian9-corrected→Hann at 25/50/75%, Hann→Tukey at 50%, Tukey→rectangle at 50%, Gaussian9-corrected skew +/-0.5.

Morph audio uses independently precomputed intermediate Q15 tables in the disposable copy; it is **not a production runtime morph implementation**. The strict numerical probe also tests endpoint-time integer blending against a wide reference across all three neighboring pairs. Table-before-interpolation versus interpolation-before-blending can differ by quantization units; future runtime code needs an explicit rounding policy.

**8,254,128 strict ASan/UBSan numerical checks passed**, without category exclusions, for bounds, endpoints, lookup interpolation, symmetry (skew excluded appropriately), and neighboring-window blends. Audio integration retains the repository's existing exclusions for signed-integer-overflow, shift, bounds, object-size and pointer-overflow; it is not a fully strict whole-mixer proof. No new sanitizer diagnostic occurred. The eight Hann/Tukey audio measurement sets match the base experiment's retained measurements exactly. Other UI/MIDI/project suites were not rerun for this report-only change.

### Measured CLOUD example (raw output, before matching)

| Shape | Peak | RMS | Largest adjacent same-channel jump |
|---|---:|---:|---:|
| Hann | 15484 | 2788.967 | 1598 |
| Tukey | 16684 | 3727.465 | 2397 |
| Gaussian4 corrected | 14951 | 2543.555 | 1498 |
| Gaussian9 corrected | 13407 | 2112.303 | 1475 |
| Gaussian16 forced | 12164 | 1834.918 | 1508 |
| Gaussian9→Hann 25% | 13936 | 2248.300 | 1438 |
| Gaussian9→Hann 50% | 14456 | 2409.687 | 1418 |
| Gaussian9→Hann 75% | 14970 | 2591.380 | 1506 |
| Hann→Tukey 50% | 15778 | 3218.457 | 1969 |
| Tukey→rectangle 50% | 16927 | 4123.923 | 2356 |
| Rectangle alpha .06 | 17519 | 4655.412 | 2314 |
| Gaussian9 skew -0.5 | 10708 | 1803.812 | 1645 |
| Gaussian9 skew +0.5 | 12828 | 2281.095 | 2456 |

RMS/peak and jump statistics are reported for all four personalities in the external JSON. Larger sample differences are not proof of a click; source phase, note overlap, spectral content and STUTTER's existing gate also contribute. No subjective audition or preference ranking is asserted.

### Paired, RMS-matched listening files

Each external WAV is all 131,072 frames (~2.972 s), 44.1 kHz stereo PCM16. Offline gain is **attenuation only**: for each personality, target RMS is `min(-24 dBFS RMS, 0.8 * minimum raw RMS across its 16 shapes)`. Files are never shortened, resampled or clipped. Within a personality, measured output RMS spread is below **0.000022 dB** after quantization. This removes gross level bias but is **not perceptual/LUFS loudness matching**, and does not make different spectral shapes equally loud to every listener. Normalization was applied only to derived listening captures, never source recordings or firmware.

External listening bundle (not committed as a firmware/documentation asset):

- Path: `/workspace/work/grain-envelope-probes/grain-envelope-listening-and-probes.zip`
- Size: **17,889,206 bytes**
- SHA-256: `a1b55a035329944f32e4a465944163fc6d582e241524c6ec439d78dd072c7a79`
- Contains 64 named matched WAVs, per-file checksums, raw-output audio metrics and table/block measurement files and external probe scripts.
- Individual files: `/workspace/work/grain-envelope-probes/results/{cloud,freeze,stutter,shimmer}-{shape}-matched.wav`; useful pairs include `hann`, `gauss9-corrected`, `gh50`, `tukey`, `rect06`.

Source reproducibility: copy the base `firmware/`, `tests/` and existing `build/gen/` into an external `project/` directory next to the bundled `probe.py`, then run it **once on that fresh copy**. It instruments only that disposable copy, builds the existing audio fixture and emits tables/captures. `block_probe.py` evaluates its generated tables against the exact endpoint-ramp policy. Compile `numerical.c` with `cc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all` after table generation. Existing pinned engine dependencies are required; no tool was installed/downloaded during this audit.

Script SHA-256 values: `probe.py` `e5fa66ab2bfbc91d615cfed9a631e2e20da6123626f254b2feb9c09e8632a294`; `numerical.c` `e6d967e39bc5cfe1b99877a29cf4df4f196a4a7eb23f52769514037d2489a604`; `block_probe.py` `8a150b13cd7bd1dc3ee59dea9e8cead0048285fa05e0ba58efb378ea6b164859`. This independently implemented external probe copies no GRAINE implementation. Standard envelope formulas do not require importing its source; any future copied MIT implementation would still need its notices retained.

## 8. Four-personality suitability

| Personality | Plausible value | Required caution |
|---|---|---|
| CLOUD | Gaussian gives a more localized amplitude pulse; width/morph changes cloud overlap texture | Narrow windows reduce coverage at low density; preserve DRIFT/WIDTH/G-SPRD meanings |
| FREEZE | Long HOLD makes Gaussian and gradual window changes relatively well represented | Preserve HOLD, motion, scan and source boundaries; long population transitions are expected with spawn latching |
| STUTTER | Tukey/rectangle can retain a flatter grain body | Existing gate can itself make edges; tiny-grain ramp errors are largest here. Do not promise rectangle means click-free percussion |
| SHIMMER | Gaussian may focus each harmonic pair; morph can alter blend texture | Both spawned grains latch a coherent control snapshot; preserve interval, harmonic gain and 2× rate cap |

These are sound-design hypotheses supported by nonzero renders, not claims that one envelope sounds better. The audio probes cover one real factory recording and one main grain-size state; the block probes cover additional lengths but are not all-source musical validation. A future implementation must repeat factory/user/sine, extreme HOLD, density and pitch matrices.

## 9. Compatibility and future UI

- The current test selector is not persisted or user-facing. Old projects/presets have **no window field** and must continue to imply exact Hann, including pre-existing TONE/DRIFT/DTUNE behavior. Do not reinterpret an old hidden value as Gaussian/morph.
- `firmware/src/core.h:75–78` retains P_COUNT=118 and stable engine slots. `params.c:181–182` contains reserved filter slots; **ID 108 is already STRETCH** through `param_desc_of:242–248`. ID 109 remains a reserved descriptor, not an approved free public parameter. No ID is allocated here.
- `project.c:8–10,179–186,659–675` keeps FUN11 in 3,840 bytes with overlap checks; `:759` rejects counts above P_COUNT. Nominal spare bytes do not automatically authorize a new schema. Appending parameters also interacts with the last-eight engine-field mapping, old-count conversion, editor metadata, MIDI learn and motion/lock encoding.
- Public parameter range is currently byte-packed within -64..127 (`proj_pack:672–673`), so an illustrative 0..253 **internal grain code is not directly a compatible persistent parameter range**. Public control and DSP encoding must be separate; new persistence needs its own approved design and migration/downgrade tests.
- `params.c:555–557` has empty GRAIN grid cells, but visual space is not storage capacity. A future WINDOW control could occupy an approved empty cell without moving existing controls only after descriptor/route review; this audit changes no layout or navigation.
- New window modulation should latch a resolved value at spawn. Existing motion, locks, editor and MIDI targets must retain their IDs and timing. Do not hijack SHP, RAND, G-SPRD, TYPE or personality A/B for envelopes. A simultaneous SHIMMER pair should read one resolved snapshot; no asynchronous control writes mid-spawn.

## 10. Direct answers and staged plan

1. **Is Gaussian worthwhile?** Technically feasible and meaningfully distinct; yes as a candidate for controlled audition. Corrected k=9 is a sensible first width, with k=4 corrected as a broader comparison. Musical superiority remains unproven.
2. **Discrete, morphing or both?** Discrete first; support optional adjacent linear morph later if listening supports it. Exact anchors/default and spawn latching are mandatory.
3. **Tapered rectangle?** Potentially useful, but defer general approval until short-grain fidelity and click behavior are characterized. Try alpha=.12 alongside GRAINE-like .06.
4. **Separate skew?** Yes if eventually exposed; defer it and keep it neutral by default. It is not another name for width.
5. **Lowest CPU/RAM design?** Static 257-entry flash tables, one latched byte, one lookup per endpoint for discrete/exact anchors; two lookups and a convex blend for intermediate morphs. No runtime tables, callback float, allocations or extra audio buffers.
6. **Next prototype?** A separately authorized Gaussian9-corrected, discrete, test-only experiment, plus an external/adaptive evaluation study for short grains. Do not ship a general morph/skew UI first.

| Stage | Locations / behavior | Tests and acceptance criteria |
|---|---|---|
| A: Discrete Gaussian experiment | `eng_grain.c:gr_spawn_one`, `gr_experiment_win`, independent generator beside `gen_grain_tukey.py`; no public ID | Exact Hann PCM and default object identity; strict table/lookup/kernel sanitizers; all personalities, five factories, real/synthetic user and sine, forward/reverse, pitch and length extremes, FREEZE HOLD, density/four-track bounds; matched listening review. Admit no silent preset conversion |
| B: Short-grain fidelity experiment | `gr_run:415–427`; compare per-sample or bounded sub-block envelope evaluation in an opt-in path | Lengths 16,17,31,32,33,64,128,441 and long legal lengths; phase offsets, source boundaries and window edge peaks. Define acceptable envelope error and maximum CPU work before changing shipping behavior. Preserve legacy Hann path explicitly; no hidden golden refresh |
| C: Latched linear morph | Existing window byte or explicitly sized state; adjacent flash lookups at endpoints, exact Hann bypass | Wide-reference blends/rounding, all anchors and intermediate codes, invalid controls, active-grain latch invariance, no RNG/scheduling changes, paired SHIMMER coherence, live future-spawn changes, high-density CPU/stack profiling. No unity overflow or new clipping; documented energy behavior |
| D: Approved persistent/UI integration | `core.h`, canonical descriptors, `project.c`/presets/editor/motion tests and `GRAIN_GRID` only after separate design approval | Old imports default to exact Hann; all existing values unchanged; project/preset/backup round trips, storage bounds, explicit downgrade policy, stable existing IDs/routes, no invisible editable cells; target link passes before any hardware readiness claim |
| E: Optional skew | Latched bounded peak and integer phase warp, only after B/C | Denominator/phase bounds, short-edge error, held-note edits, reverse orientation, four-track numerical/audio tests, click listening and target deadline/stack/flash measurements. No global active-table mutation |

All resource amounts in this report are source-derived estimates or host probe measurements, **not FM1 target measurements**. This documentation-only branch changes native text/data/BSS/RAM/stack by **zero**. Proposed Gaussian/rectangle tables and morph code require a future linked size report. No target build, successful link, hardware CPU/stack profiling, flashing, merge or deployment occurred. The **581,564-byte image fit remains unverified**.
