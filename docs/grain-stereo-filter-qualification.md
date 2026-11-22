# Independent L/R filter qualification — Task 244D1

Base: `4744b281c4b566ed42d8733fe36b0dfc593fc6e8`, branch
`feat/coffee-grains-granular-quality`. Qualification only: no firmware, mixer,
UI, parameter, build-profile, storage or protocol source changed. SPREAD remains
inactive and the existing mono callback/transport remains authoritative.

## Decision and numerical result

Reconstruct integer L/R from the grain prototype's doubled components **before**
TONE or the nonlinear SVF. Give each lane independent signal history, share
controls, and re-encode only after both channels have been processed. Do not
filter M/S independently: the integrator clamps do not commute with that basis
change. No clamp, filter coefficient, polarity or amplitude rounding is altered.

`tests/grain_stereo_filter_qualification.h` is an isolated model included only
by tests. It processes both lane histories with one TONE-coefficient update per
frame, one SVF coefficient setup per block, and one transition-weight update
per frame after both lanes. Two independently executed, unchanged `gr_tone()` /
`gr_filter()` instances provide the oracle. Comparison covers every output
sample and lane state after every block, including control state.

Strict ASan/UBSan, with signed-overflow/shift checks enabled and no exclusions:

- **46,634,628 checks, 20,000,768 frames; zero PCM or state mismatch.**
- Every CUTOFF -50..50 and every RESON 0..127; steady TONE0/64/127, live
  TONE changes including126, full-wet resonance stress, raw signed input extremes,
  amplitude ramps and output accumulation close to signed32 limits.
- 5,672 clamped lane-state endpoints, with exact reference equality; saturation
  within a block may occur more often. No sanitizer findings.
- Rapid zero/polarity changes and invalid initial packed transition states;
  block lengths1..32, output canaries and untouched out-of-block samples.
- Explicit256 one-frame entry steps,256 reversal-to-dry steps, integrator reset
  at the dry boundary and subsequent opposite-polarity entry. A one-frame TONE
  assertion catches accidentally advancing its coefficient twice.
- Warm mono-history cloning into separate L/R objects followed by independent
  lane input and filter transitions matches both reference histories exactly.
- Settled CUTOFF0/TONE127 equals the original `gr_output` dry/amplitude result.
  CUTOFF0 with darker TONE still applies TONE, as the existing engine does.
- Wide M2/S2 encoding/reconstruction has **zero error**, including signed32
  extrema and odd-valued channels. A separate bounded handoff-fade test verifies
  exact endpoints, monotonic constant-endpoint blending and bounded added steps.

Result hash: `13831782403312797772` (explicit generated output values, not pointer
or padding bytes). This establishes the filter policy, not a completed audible
stereo instrument, full mixer test or subjective click-free listening result.

## Source contracts preserved

- `firmware/src/eng_grain.c`: `gr_tone`, `gr_filter`, `gr_output`,
  `grain_note_on`, `grain_render_transport`, `grain_stereo_probe_transport`.
- `firmware/engines/src/dsp.c`: `tsvf_coef`, `tsvf_lp`, `amp_at`, `VOICE_FS`.
- `firmware/src/voice.c`: `track_render`, `track_grain_stereo`, SPREAD_BUF.
- `firmware/src/fx.c`: `mix_spread`, `mix_part`, `mix_block`, `master_out`.
- `firmware/src/slicer.c`: `sl_seg`, `slicer_track_sd`.
- `firmware/src/perform.c`: `perf_mute_sd`.

Original voice storage is s2=last SVF output, s3/s4=integrators,
s5=TONE signal state, s6=TONE coefficient and s7=packed wet/running polarity.
The qualification object owns two distinct `{tone_y,last,ic1,ic2}` records;
no record aliases those mono fields, V-SPRD, RNG, grain phase or another voice.

The shared TONE coefficient retains the exact Q14 minimum-step smoother.
CUTOFF frequency coefficients still change on the existing block control
boundary; this task does not add frequency smoothing. The shared256-frame
wet/dry weight and running polarity obey the existing state machine. Both lanes
use the same weight before its once-per-frame increment/decrement. On reversal,
the old polarity fades out before a subsequent block starts the new polarity.
When wet reaches zero, the existing block-end reset semantics are retained.
Calling stock filters with different block partitions is not promised identical
when control changes/reset boundaries differ; each tested partition is compared
with its identically partitioned oracle. The isolated model rejects zero or
oversized blocks as a no-op; this is not a new public engine ABI contract.

## Bounds and transport representation

The coefficient enumeration also proves conservative interval bounds for legal
CUTOFF/RESON, x in +/-65535 and each integrator in +/-150000:

- Maximum absolute coefficient-product sum:1,556,237,700, within signed32.
- Conservative filtered signal magnitude:554,578, including HP reconstruction.
- With legal amplitude endpoints0..32767 and VOICE_FS24000, a conservative
  zero-origin per-voice output bound is812,348, including rounding margin.
- Three voices/part give |M2| and |S2| <=4,874,088, comfortably within signed32.
  Even the conservative eight-voice total doubled bound fits signed32.

The proof uses int64 arithmetic and outward rounding; it does not rely on the
stress signal actually attaining all extrema simultaneously. TONE's clamped
convex update preserves its signal bounds. Dry/filter interpolation is convex
and preserves the established endpoints; no new saturation policy is introduced.

Use int64 for `M2=L+R`, `S2=L-R` and `M2 +/- S2` intermediate calculations.
General signed32 L/R can require33 bits in the encoded components: the endpoint
matrix explicitly tests that case with int64 storage. Do not blindly cast a
possibly saturated/cumulative generic output into the existing int32 transport.
The proven smaller bound applies only to zero-origin voice output, legal
amplitudes and the stated bounded histories. D2 must enforce that contract and
use checked narrowing or retain wide components outside it.

There is also a useful exact normal-path simplification: `gr_output` adds an
even contribution and, under this zero-origin bound, never saturates to an odd
INT32_MAX. Ordinary filtered L/R outputs and their part sums are therefore even.
After filtering/amplitude, normalized `M=(L+R)/2`, `S=(L-R)/2` reconstruct L/R
exactly and fit the existing mono-mid/side lane scale. This parity property is
not valid for arbitrary prefilled/saturated output buffers; D2 must test it at
the actual transport boundary. The raw grain prototype's earlier M2/S2 must
still be reconstructed before nonlinear filtering. Never treat M2 as existing
mono b, which would double the level.

## Ownership, entry and stereo-to-mono contract

Proposed runtime ownership is one40-byte object/voice: two16-byte L/R signal
histories plus shared4-byte TONE coefficient and4-byte packed filter control.
At three GRAIN voices,120 bytes/part inside the existing reservation; no grain
record growth. The current prototype's target-layout part7496 plus120 becomes
7616, leaving5272 bytes of12888. Allowing a separately owned8-byte part handoff
state still leaves5264. Four engine reservations remain51552 allocated bytes.
The test runner checks40-byte state and the proposed capacity with freestanding
32-bit compilation. This is a layout proof, not a JieLi link or live allocation.
The actual part is not changed in this task.

For D2 use the following explicit state machine:

1. **MONO:** execute only the unchanged legacy route. No stereo work or state
   mutation; existing inactive PCM/state remains exact.
2. **ENTER/ACTIVE:** on entry clone warm mono signal histories into distinct L/R
   histories and snapshot the shared coefficient/transition controls. Existing
   centered grains stay centered; only newly admitted grains capture test width.
   Do not rewrite active pan values or reseed/retrigger grains. New stereo signal
   begins through the existing grain windows. Test actual entry for clicks.
3. **DRAIN:** on width0, newly admitted grains center, while already placed grains
   retain placement until they end. Continue processing their L/R histories and
   filter tails. A quick width-on reversal continues that history; no reset.
4. **HANDOFF:** after the last off-center grain ends, crossfade continuing L/R
   output to the continuing original mono route using a separate256-frame
   endpoint-inclusive handoff, not s7's CUTOFF fade. Do not wait indefinitely for
   exact numerical zero: fixed-point filter tails can persist. Do not abruptly
   clear different L/R histories when C becomes zero. Resume ACTIVE smoothly if
   width rises again; the reversal/handoff details require integrated tests.
5. **MONO:** only after handoff reaches the exact mono endpoint retire stereo
   state. There is no claim of immediate legacy-state identity during DRAIN or
   HANDOFF, and no fixed total tail duration independent of active grain length.

For the strongest eventual return-to-legacy guarantee, keep the original mono
signal history warm throughout ACTIVE/DRAIN using the *original unpanned grain
sum*. That sum can be accumulated from the same decoded/windowed sample: no
extra reads, grains or RNG. It is **not** generally the attenuated stereo mid.
This shadow uses the existing mono signal storage but must consume the same
shared control snapshot, not advance smoothing again by calling full helpers.
That means three signal-lane filter steps in active stereo, not two. D2 must
qualify the additional shadow factoring and exact return before claiming it.
An average of nonlinear L/R histories cannot substitute for this original mono
history. The current two-lane qualification and benchmark do not pretend to
implement or measure the shadow handoff end-to-end.

Fresh-voice initialization must match `grain_note_on`: zero signal histories,
current TONE coefficient, s7=-1 first-render behavior. A source-index reset is
not automatically a mono-filter reset: the current `gr_reset` does not clear
voice integrators. D2 must follow actual mono lifecycle and keep tails on source
changes where mono does, clear on fresh voice/ownership reassignment, and retire
buffers only after the existing engine-switch fade. All state is written only
by the owning part's render core; no cross-core scratch or main-thread writer.

## Proposed mixer policy (not implemented)

After L/R filtering and amplitude, keep two voice-group mid/side pairs:
MA/SA and MB/SB. The current transport's b is MA+MB, SPREAD_BUF is MA, and
intrinsic-side buffers carry SA and SB separately. Existing V-SPRD has its own
clean whole-voice difference MA-MB; it must never be overwritten by SA/SB.
Track PAN remains a live post-grain control, with the existing group pan offsets.

For clean dry group gains, compute contributions as
`L=gLA*(MA+SA)+gLB*(MB+SB)` and
`R=gRA*(MA-SA)+gRB*(MB-SB)`, with existing level/scaling conventions and wide
intermediates. This reduces to existing V-SPRD when both intrinsic sides are
zero. Without V-SPRD, both groups use the same live PAN gains.

Keep DIST/INSERT on MA+MB as before, the whole-voice difference bypassing those
mono processors as currently. Add the separately weighted intrinsic-side
contributions once. Sends take the resulting mono mid under the existing policy;
do not run distortion, insert or sends twice. Mono wet returns can narrow the
perceived image; changes to the processed mid relative to the clean side can
also alter width. No promise of invariant width with nonlinear mono effects.

Slicer and mute must advance each existing envelope once, applying that same
gain to mid, voice-group difference and both intrinsic sides. Calling existing
`slicer_track_sd` or `perf_mute_sd` repeatedly is incorrect. Slicer STUT repeat is
currently mono and fades side out; retain that width-reduction policy unless a
separate feature is approved. Keep per-part effects/lifecycle ownership on the
same CPU2 job, join before master, and compute HOME peaks from actual L/R.

Keep final interleaving L then R, shared-peak limiter, separate channel DC/EQ and
existing softclip unchanged. BASS+ intentionally injects a common bass signal
and narrows low-frequency separation. Test hard-pan zero-opposite output with
mono wet returns/BASS+ disabled and below limiter thresholds; with them enabled,
intentional mono crossfeed is not a channel-swap or transport failure.

## Host cost and target limits

Five-run -O2 medians, filter-stage only, steady TONE64/CUTOFF+25/RESON127,
including synthetic input construction/output hashing:

| Workload | Existing mono | Qualified L/R | Increment |
|---|---:|---:|---:|
| One track, three voices |21.954 ns/frame|35.572 ns/frame|62.0%|
| Four tracks, eight voices |57.438 ns/frame|93.586 ns/frame|62.9%|

Hashes repeat exactly per mode. This is not a grain decoder, full mix, stereo
shadow, effects or FM1 timing benchmark. Shared setup saves duplicate control
work but does not remove the extra signal-lane arithmetic. With the existing
8-voice global budget, two L/R lanes require up to705600 SVF steps/second at
44.1kHz; a warm mono shadow raises this to1058400. A conservative12-voice bound
ignoring that global limit would be1587600 with shadow. No new RNG or decode work
is implied. FM1 CPU2 scheduling costs need recalibration only after target data.

The qualification model has256 bytes of temporary signal storage per call.
Host -O2 inlines it into a1008-byte benchmark main frame; -fno-inline reports a
416-byte q_lr_block frame, versus separate mono TONE16/filter152-byte frames in
that probe. These frames are not the FM1 call-chain high-water mark. Core1 has a
4096-byte stack with guards in fm1_cpu1.h; target stack analysis/watermarks remain
mandatory. Reusing bounded caller-owned scratch may reduce D2 stack, but must
not alias V-SPRD or a different CPU's part.

Production text/data/BSS and callback work delta: **zero**, because no production
file includes this model. The combined host benchmark itself is text545179,
data16540, BSS307080 bytes, mostly existing host/firmware fixtures; it is not an
integration code-size estimate. No target build, flash-fit or real-time claim.

## Reproduction and regression gates

```
python3 tests/run_grain_stereo_filter_qualification.py --out /tmp/lr-qualification
python3 tests/run_grain_stereo_prototype.py --base-tree CLEAN_e43141e --out /tmp/lr-baseline
python3 tests/run_granular_quality.py --sanitize --oracle-tree CLEAN_fd7f6f0 --out /tmp/lr-quality
```

The runner compiles/runs the strict numerical oracle,32-bit state-capacity probe,
repeatable benchmark and stack-usage builds. No generated artifacts enter Git.

Rerun results:

- 105 three-way base/default/prototype normal-playback PCM/state cases identical;
  every normal side buffer stays zero, including positive-C DSP and PAN/V-SPRD.
- Existing dry stereo probe77,032,084 strict checks, exact decoder/RNG/state.
- 24 frozen granular PCM/state captures and independent repeat pass.
- Short envelopes152,924,099 checks; STUTTER42,069,041; filter transitions3,553,312.
- Historical NOR/persistence5804 checks and user presets pass.
- Editor2909, WIN110, C2259, project bridge2189, browser89, WIN provenance11,
  C persistence109038 and downgrade oracle362 pass.

No new failure in these gates. Full-mixer comparison still has its documented
historical signed-overflow/shift sanitizer exclusions; the new qualification
has none. Broad historical editor/motion/backup/robustness failures were not
rerun or claimed resolved. Production source diff against the exact base is
empty; no goldens or saved formats changed.

## Exact requirements for Task 244D2

1. Keep the shipping branch default-off and retain the untouched mono callback.
2. Add explicitly owned L/R state within the proven reservation; share controls
   physically once per frame. Extend the model to the warm mono shadow and prove
   its legacy PCM/state identity using the original unpanned sum.
3. Join grain placement to real voice L/R filtering without double decoding;
   re-encode with the proven zero-origin bounds/parity and checked wide arithmetic.
4. Add separate once-per-frame multi-component slicer/mute handling, PAN/V-SPRD
   reconstruction, mono-effect/send behavior and actual L/R peak measurement.
5. Implement/test entry, off-center grain draining, finite handoff, reversal of
   handoff, fresh/source/engine lifecycle and CPU2 buffer ownership. Never clear
   audible histories on a C change alone.
6. Test full-render output, clipping, decoder/RNG/admission identity, all effects
   and four-track worst load, preserving the 105-case default route exactly.
7. Measure actual complete callback/stack and compact target link/timing before
   deciding whether the three-lane shadow policy fits. If it does not, stop for
   a revised explicit return-compatibility policy rather than silently dropping
   the shadow or changing filter saturation.

Files added: tests/grain_stereo_filter_qualification.h,
tests/grain_stereo_filter_qualification_test.c,
tests/grain_stereo_filter_qualification_bench.c,
tests/run_grain_stereo_filter_qualification.py, and this report.
