# Default-off integrated GRAIN stereo — Task 244D2R

Base `b4442c5c1a018adc69fbd7d1bf5f2418360e00a7`, branch
`feat/coffee-grains-granular-quality`. Normal firmware remains mono. No C mapping,
UI control, protocol, preset/storage format, sample or engine callback ABI change.

## Gate and ownership

The explicit test build requires both `FELUCCA_GRAIN_STEREO_PROTOTYPE=1` and
`FELUCCA_GRAIN_STEREO_INTEGRATION=1`. Neither is enabled in build scripts. The
integration macro defaults to0. Existing experimental fidelity/hybrid rendering
cannot be combined with this integration: compilation refuses that unqualified
combination. Standalone placement/morph experiments remain available separately.

`gr_test_spread[part]` remains a test selector0..100, not parameter118. Set it only
at the existing control/audio boundary, before C1 jobs start or after C1_WAIT;
there is no new concurrent main-thread control publication API. Only CLOUD
activates stereo. Other personalities use the existing renderer unless finishing
a previously activated local handoff. Fresh positive-C SPARKLE/CHAOS/DRIFT are
unmodified. Track/default/new-project initialization clears the test request;
it does not directly erase audio-owned histories. No visual UI code changed.

Each part owns L/R histories for its three voices, one block-use marker, and
reusable three-lane input/output scratch inside its existing engine region.
Control progression is taken from the original voice's TONE coefficient and
packed CUTOFF transition state. It advances physically once per frame, then
updates separate L/R signal histories and the original mono signal history.
There are no shared mono integrators, borrowed phases, additional RNG draws,
locks, allocations, extra grains or duplicated decoder traversals.

Engine-memory claiming initializes the expanded region. Fresh note initialization
resets the corresponding stereo voice state alongside the original mono filter.
Inactive voices are retired at grain_block. Source changes follow existing mono
history behavior: they reset grain/index state and side scratch, but do not
arbitrarily zero continuing filter histories. Engine switch fades retain the
existing amplitude lifecycle; another engine cannot obtain a GRAIN side view.
Returning through a memoryless engine still clears side scratch on render and
initializes fresh voices. Every track block clears the entire intrinsic-side
capacity, even for short blocks or no active voices. CPU2 jobs use only their
own part's region; master sums are joined through the existing C1 barrier.

## Decoder, nonlinear filters and shadow

For each owned grain, `gr_run_lr` calls the unchanged `gr_run` once into32-frame
scratch. That one decoded/windowed signal contributes to the original unpanned
sum and the existing center-unity, edge-attenuating L/R pan coefficients. Packed
pan placement/pairing and successful-admission rules remain those from244C.

L/R are reconstructed **before** TONE and SVF. `gr_lr_filter` applies the
qualified coefficient mapping, +/-150000 integrator clamps, negative LP/positive
HP and existing256-frame dry/filter transition. Its two L/R histories and third
mono-shadow history consume the same coefficient and wet/running values. TONE
smoothing and filter-transition counters do not advance once per lane.

The original voice s2..s7 retain the exact warm mono state for the original
unpanned sum; the shadow is not a second grain renderer. The scheduler, source
head, envelope input, grain record, reverse cache and RNG are advanced once.
Steady CUTOFF0 is the original dry filter bypass on each lane, including the
existing amplitude rounding. TONE remains independent and still applies below127.

After amplitude, normal zero-origin L/R outputs are even and bounded by the244D1
proof. Wide intermediates encode normalized M=(L+R)/2 and S=(L-R)/2 exactly at full
stereo weight. M/S are in the existing mono-output scale, not the isolated probe's
doubled scale. The old doubled dry-probe entry points are therefore excluded
from the integration profile; they remain in the standalone prototype profile.
Unrepresentable final additions saturate like gr_output; qualified legal voice
and part signals never reach that defensive limit.

## Local activation, drain and settlement

The per-voice state machine is explicit in `on`, `mix` and the placement snapshot:

1. **MONO:** `on=0`, no request. The original renderer and original mixer route
   are authoritative; the stereo processor is not called.
2. **ACTIVE:** a valid CLOUD request clones warm mono signal history into owned
   L/R histories. Existing grains are centered, new grains latch test placement.
   During ordinary entry the windowed new grains introduce the stereo signal.
   The mono shadow stays warm from the same decoded samples.
3. **DRAIN:** request0 centers newly admitted grains; previously placed grains
   retain their pan. If a placed grain existed at block entry or remains after
   rendering, the current stereo weight is retained for that entire block.
   Rejected admissions cannot extend or invent a panned grain.
4. **HANDOFF:** with no owned panned grain at block entry or after render, the
   weight decreases by1/output frame from256 to0. Output mid approaches the warm
   mono result and side approaches zero. L/R histories keep processing; tails
   are faded, never abruptly reset or required to decay numerically to zero.
   A request returning during handoff raises the same weight by1/frame, without
   clearing or recloning history. Weight stays in0..256.
5. **RETIRE:** after weight0 at block end, owned L/R histories are cleared. The
   following block uses the authoritative original mono renderer. A track uses
   the legacy mixer after no voice used stereo during the current block.

**Settled condition:** request remains0, no owned noncenter grain remains, every
voice has `on=0`, and the current block's `lr_block=0`. Side buffers are zero and
local mono output/state match the untouched renderer. The final handoff block
can still contain nonzero stereo weight, so it is not labelled settled merely
because `on` became0 at its end. Stable off requests settle after the longest
remaining panned-grain lifetime, conservative block-boundary padding, and256
handoff frames (about5.8ms). No immediate historical equality is claimed.

The fade uses wide normalized-mid/side blending. At endpoints0/256 it is exact;
intermediate reconstructed L/R may differ by at most2 pre-mixer integer units
from separately rounded ideal L/R blending. The active mixer reconstructs in
wide arithmetic before its final level/pan rounding, eliminating hard-pan
cancellation leakage. Its single-rounding precision differs from the legacy
multi-shift mixer while active; the legacy route is restored after settlement.
A small quantization change at that boundary is not a side-buffer truncation.

Downstream DIST/INSERT, ROOM and other effects retain their actual histories.
They are not shadowed, cleared or forced to drain, and full-mixer PCM/state after
a stereo session is deliberately not compared to never-stereo history. This is
the corrected compatibility contract, not an unresolved requirement from244D2.

## Mixer policy

The private callback still adds mono mid into the current voice-group buffer.
After track_render, b=MA+MB and V-SPRD's existing buffer is MA. Intrinsic-side
buffers independently contain SA and SB. The mixer derives MA-MB separately,
then retains the existing live PAN and V-SPRD group offsets/clamps. Wide dry
reconstruction is equivalent to:

- L = gLA*(MA+SA) + gLB*(MB+SB)
- R = gRA*(MA-SA) + gRB*(MB-SB)

Track level applies once. No group side is overwritten or counted twice.
DIST/INSERT process the mono mid once; the clean voice-group difference and
intrinsic sides bypass those nonlinear mono effects. Established sends take the
processed mono mid. Mono wet returns can narrow the image; processing mid while
leaving side clean can also alter perceived width. No width invariance is claimed.

New private slicer/mute helpers advance their original gain histories once per
frame and apply those values to mid, V-SPRD difference and both intrinsic sides.
STUT repeat remains mono and fades the live side, following existing policy.
No limiter, master DC/EQ, BASS+, global effect or interleaving code changed.
BASS+ deliberately adds common bass, and the linked limiter/softclip may alter
channel balance at high levels. Dry hard-pan zero-opposite checks disable these
intentional crossfeeds. HOME level peaks measure the actual new L/R contribution.

## Validation

Reproduce the integration gate, baselines, channel probes, CPU2 tests and resources:

```
python3 tests/run_grain_stereo_integrated.py --base-tree CLEAN_b4442c5 --out /tmp/stereo-integration
```

Generated PCM, executables and measurement JSON stay outside Git. No goldens
were regenerated. Results:

- 105 exact baseline cases across base, current default, placement-only and
 integration-enabled-but-width0 builds. Includes positive-C DSP and PAN/V-SPRD.
- 12 additional default-off positive-C mixed FX/slicer PCM/state pairs exact.
-**2,476,742 strict ASan/UBSan checks**, no signed-overflow/shift exclusions:
 integrated renderer L/R output and histories versus independently decoded
 and mono-filtered references; three-lane filtering versus three mono instances for every
 CUTOFF/RESON, shared control progression, amplitude ramps, actual new-project reset traces, component equations,
 slicer/mute state/recording equivalence, reset/ownership and shadow handoff.
- Five factory sounds and32/257/65535-frame user-zone fixtures: every compared
 block preserves legacy head/grain/cache/filter state, RNG and decoder calls.
 Rapid toggles are included. Depending on source,1650..1781 of2048 captured
 blocks after an on/off session were exact settled local mono blocks.
- 30 factory/PAN/V-SPRD configurations repeated exactly; each matches mono
 scheduling/RNG/admission/load/legacy-state hashes. Two hard-pan cases have
 **opposite-channel peak0** at final output.
- Six four-track effects/slicer/live cases, low/high level, active limiter and
 live mute tests pass. During steady mute, all intrinsic-side samples are zero.
-The ROOM oracle snapshots its state before each checked block500..800 across
 deactivation and compares the actual result to an ordinary ROOM update with
 the actual send. Buffers/history match exactly; a later note-off tail remains
 in the room buffers. No forced reset or complete downstream shadow.
- Threaded CPU2: four selected GRAIN-containing scenes, including four GRAIN
 parts and mixed engines, are byte-identical for rotating, automatic and five
 fixed part splits. The inherited summary counts29 enumerated scenes; only the
 four selected scenes were run.

Centered PAN/no V-SPRD final L-minus-R RMS for the five factory probes:
CELLO799.46, FLUTE1837.09, GUITAR1024.60, SYNTH1804.56, VOX990.60 output units.
Peaks were6896..9756 for those probes. Broader effects/level fixtures remain
bounded by the unchanged final limiter/softclip. Numerical separation is not a
subjective quality claim or a physical analog-channel measurement.

Existing gates also rerun successfully:24 frozen PCM/state fixtures and repeats;
short-envelope152924099 checks; STUTTER42069041; filter transitions3553312;
SPARKLE1411308; CHAOS3804742; DRIFT2488821; bank ownership26299; historical
persistence/NOR5804; user presets; editor2909/WIN110/C2259/project bridge2189;
browser89/WIN provenance11/C persistence109038/downgrade362; isolated L/R
qualification46634628; placement prototype77032084.

Full-mixer fixtures retain their pre-existing signed-overflow/shift sanitizer
exclusions. New DSP, mixer-equation, gate and local-shadow checks are strict.
No existing assertion was weakened. A new ROOM test initially selected the
wrong reverb model and was corrected to explicitly select ROOM. An optional
native64 `FELUCCA_CPU2=0` build fails the existing track_t multiple-of16 assertion
on both the exact base and current code; it was not bypassed. The default CPU2
profile and real threaded split tests pass. Broad historical editor/motion/
backup/robustness failures were not rerun or represented as fixed.

## Resources and timing

Default build text/data/BSS and allocated engine reservation are unchanged.
With the explicit integration gate, host part size is8472; the actual declaration
passes a freestanding32-bit layout check at8376. Grain records remain64 host /
56 target-layout bytes. Against placement-only state, the integration occupies
**880 bytes/part inside the existing12888-byte region**:36 bytes x3 voices,
4-byte block marker and768 bytes of reusable scratch. Remaining capacity4512
bytes/part; four reservations remain51552 allocated bytes. No downstream delay,
chorus or reverb arrays are duplicated in firmware.

Identical optimized host fixture builds: base/default text551521, data16876,
BSS329864. Gated text559345 (**+7824**), data unchanged, BSS329896 (**+32**, test
selector/alignment). Host harness includes oracle state and is not a target
image-size measurement. No FM1 flash-fit claim.

Three-run median CPU ns/output-frame, maximum-grain fixtures:

| Component | One track: old -> new | Four tracks: old -> new |
|---|---:|---:|
| Full dry render |91.972 ->162.579|215.008 ->407.532|
| Full render with FX/BASS+/slicer/V-SPRD |122.520 ->191.326|298.809 ->496.891|
| Mono filter -> independent L/R |23.857 ->45.151|58.010 ->111.645|
| L/R filter -> L/R plus mono shadow |45.151 ->58.350|111.645 ->152.130|
| Grain kernel -> stereo plus unpanned accumulation |17.315 ->30.247|65.834 ->113.350|
| V-SPRD mixer -> integrated stereo mixer |5.329 ->6.378|15.831 ->19.947|

Full render has three voices on one track and eight voices across four tracks.
All four parts reached12 simultaneous grains, with decode-load caps respected.
The kernel microbench uses constant-source samples to isolate weighting/scratch;
it is not a decoder throughput estimate. Component measurements overlap and
must not be added as independent callback costs. All benchmark hashes repeat.

The host worker-stack fill test reports8584 bytes for both baseline and stereo,
including pthread startup; that startup dominates, so this does not establish
zero callback stack growth. Compiler reports in the PCM fixture give grain_render
304 ->496 bytes; benchmark context gives400, filter helper56, mixer248 and worker
944 bytes. Host stack reports are not FM1 call-chain high-water marks. Target
core1's4096-byte stack and its guard/watermark must be checked on the target.

Work is bounded by existing12-grain/part,3-voice/part,8-voice global and32-frame
limits: at most48 grain weighting passes and24 filter lanes per frame, no new
source reads/RNG. Shadow filtering raises the active path to three signal lanes,
but controls progress once. No new blocking, variable allocation or unbounded
scheduler work. The existing CPU2 cost estimates remain uncalibrated for this
experimental path and must be profiled before enabling it.

**Real-time risk is substantial:** full dry host overhead is about77%/90% in
these dense cases. Host timing does not prove FM1 deadline feasibility. The path
fits the proven reservation and has bounded work, but target timing, target
stack and actual flash link remain unqualified. Keep the gate off until those
measurements exist; optimize the once-decoded weighting/scratch path if needed,
without sacrificing the mono shadow or weakening identity tests.

## Changed files

- firmware/src/eng_grain.c: guarded ownership, lifecycle and once-decoded routing.
- firmware/src/grain_stereo_integrated.h: three-lane filter, local handoff/reset.
- firmware/src/voice.c: block clear/marker; public engine ABI unchanged.
- firmware/src/fx.c: private wide stereo mixer and gated selection.
- firmware/src/slicer.c, firmware/src/perform.c: once-per-frame component gates.
- firmware/src/ui.c: guarded test-request reset at existing track-default boundary;
  no graphics, navigation or knob mapping change.
- tests/grain_stereo_integrated_test.c, tests/grain_stereo_integrated_bench.c,
  tests/run_grain_stereo_integrated.py, and this report.

No production SPREAD, parameter-C mapping, push, merge, release or flashing.
