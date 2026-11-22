# Low-cost CLOUD stereo width

Base: `c421084fdd0462b01116981fc9c362277af7b5f1`, branch
`feat/coffee-grains-granular-quality`. This activates personality-C/ID118 only
for CLOUD. No per-grain stereo rendering/filtering is enabled. Existing default
`FELUCCA_GRAIN_STEREO_INTEGRATION=0` remains unchanged. No parameter, preset,
project, editor, MIDI, sample, UI or engine callback ABI change.

## Algorithm and routing

Two fixed Schroeder allpasses, coefficient 1/2, delay lengths31 and79 frames,
process the existing **post-mono-filter, post-DIST/INSERT** track signal:
`y = delayed_state - x/2; state = x + y/2`. Integer division truncates toward
zero symmetrically. S is `(allpass31(x)-allpass79(x))*gain/16384`.
The Q14 target gain is `floor(clamp(C,0,100)*4096/100)`. Thus C25/50/100
provide 1/16, 1/8 and1/4 difference gain. There are no moving delay taps, added
dry-path latency, oscillators, RNG calls, grain allocations or decoder reads.
This is track decorrelation, **not genuine per-grain placement**.

The original mono GRAIN decoder, window, TONE, bipolar SVF, resonance and256-frame
filter transition are untouched. CLOUD side is independent of V-SPRD's voice-group
side. Both side contributions receive the same existing slicer/performance-mute
ramps, advanced once. The new side receives track level and
`(4096-64*abs(PAN))/4096`, then adds to L and subtracts from R. A hard-left
PAN(-64) therefore receives no new width; maximum right PAN(+63) retains the
existing one-unit pan-law asymmetry. V-SPRD's routing and rounding are unchanged.

Sends and performance throws retain their original mono signal. DIST/INSERT
remain mono before decorrelation; shared mono wet returns and common BASS+
can narrow the result. Existing stereo master EQ/DC filters and linked limiter
remain authoritative. Pre-master L+R is exactly unchanged for legal unsaturated
intermediates; **final nonlinear master histories need not match after stereo
use**. Wide intermediates and defensive int32 narrowing protect additions; no
clipping thresholds or master behavior changed.

C gain slews by at most16 Q14 units/sample (full travel256 samples,5.8ms at44.1kHz).
Rapid changes reverse from the current gain without resetting histories. A
personality change or last-voice end targets zero; no-voice tracks remain live
only for this bounded fade. Once gain reaches zero, delay state and then consumed
side scratch are cleared and normal idle routing resumes. Engine fade-out uses
32 units/sample(128 frames); engine-memory handover clears the departing state.
Explicit defaults/reset publishes a part-specific reset epoch, consumed by the
mixer, rather than clearing audio-owned buffers from control code. These are
existing control/audio-boundary operations, not a new asynchronous publication
API. Grain CPU2 jobs do not access the decorrelator; each part's mixer owner does.

Fresh C0 takes the original mixer branch without unity multiplication or ring
processing. SPARKLE, CHAOS and DRIFT are unchanged. Old saved CLOUD sounds with
nonzero C intentionally acquire width; values are not rewritten. Turning C off
restores local mono routing after the fade, without clearing downstream ROOM tails.

## Candidate comparison

`cloud_width_probe.c` compares fixed delay-difference and allpass-difference using
identical seeded broadband input. Both use580 bytes/part and exact pre-master
fold-down. Representative host -O2 results (including probe bookkeeping):

| Candidate | ns/sample | L/R correlation | L/R balance | Power growth |
|---|---:|---:|---:|---:|
| Delay difference31/79 |4.665|0.777457|+0.010dB|+0.512dB|
| Allpass difference31/79 |5.795|0.827919|+0.018dB|+0.391dB|

A uniform-frequency analytic scan gives channel-response dB RMS deviation2.227
versus1.935. Both can reach approximately-6/+3.52dB channel coloration at maximum
width. The allpass is selected for lower average coloration and energy growth
at a small extra cost. It is not coloration-free, nor does correlation establish
subjective quality. Fixed taps avoid modulation-induced pitch wobble; equal and
opposite side avoids mono cancellation. Listening on FM1 remains necessary.

Five factory renders had C100 correlations0.971/0.922/0.890/0.973/0.771
(source IDs0/2/3/13/14), versus1 at C0. Source0's C25/50/100 correlations were
0.9983/0.9931/0.9707 and side RMS181/364/729. Width is source-dependent; mellow
material can remain highly correlated. Source0 peak increased6637→8000;
RMS1544.82→1586.58. All tested final peaks remained within the unchanged limiter.
No subjective claim of spaciousness or freedom from coloration is made.

## Validation

Reproduce (clean exact-base worktree required):

```
python3 tests/run_cloud_width.py --base-tree /tmp/coffee-cloud-c421 --out /tmp/cloud-width-study/complete
python3 tests/cloud_width_bench.py --out /tmp/cloud-width-study/bench
python3 tests/run_granular_quality.py --sanitize --oracle-tree /tmp/coffee-grc-fd7 --out /tmp/cloud-width-study/quality
python3 tests/run_editor_stable.py --baseline-tree /tmp/coffee-grc-fd7 --e-tree /tmp/coffee-e-371 --out /tmp/cloud-width-study/editor
python3 tests/run_grc_migration.py --oracle-tree /tmp/coffee-preset-evidence-606 --out /tmp/cloud-width-study/migration
```

The new strict kernel/ownership/component-gate test passes46,783,560 checks.
It covers C0..100 for both candidates, signed source extremes, bounded rings,
exact fold, all128 PAN positions, zero retirement, voice end, reset/engine
ownership, unchanged RNG and slicer/mute equivalence. New PCM harness compares
24 fresh C0 plus45 positive-C non-CLOUD cases exactly to the base; six additional
PAN/V-SPRD and three effect/slicer C0 pairs are exact. Five factories at
C0/25/50/100 repeat exactly; CLOUD scheduler/RNG/admission/decoder-state hashes
remain unchanged. Four-track rapid C changes with DIST/INSERT, sends, ROOM,
BASS+ and slicer pass. Real threaded CPU2 GRAIN/mixed-engine scenes match every
split (four selected scenes; the old runner's '29 scenes' denominator is not
coverage). Freestanding32-bit structure bounds pass.

Existing gates passed:24 frozen PCM/state captures and repeat; short envelope
152,924,099 checks; STUTTER gate42,069,041; filter transitions3,553,312;
SPARKLE1,411,308; CHAOS3,804,742; DRIFT2,488,821; L/R qualification46,634,628;
bank ownership26,299; historical persistence/NOR5,804; user presets. Editor:
2909/WIN110/C2259/project bridge2189; browser89/WIN11; C persistence109038;
downgrade362. Historical migration model132084, bank counterexample8229,
WIN compatibility5818, persistence109035 and backup320 also pass, unchanged
binary fixtures. The counterexample intentionally retains evidence of unsafe
older firmware; new firmware protections are not claimed to repair old releases.

New local DSP, component and storage/editor tests use strict ASan+UBSan,
including signed-overflow checks. Full-mixer/native threaded fixtures retain
existing signed-overflow/shift exclusions for unrelated historical code; this
is not whole-firmware strict-UB qualification. Broad historical editor/motion/
backup/robustness failures were not rerun or represented as fixed. No frozen C0
PCM goldens changed. Only the previously no-op positive-C CLOUD expectation was
updated to require changed audio and unchanged grain/RNG/decoder state.

## Resources and limitations

Full-render host median of3, sustained dense CLOUD, one/four tracks respectively:
mono72.567/176.724ns per frame; cheap width75.997/190.088; expensive per-grain
stereo119.679/337.080. Cheap incremental cost3.430/13.364ns versus47.112/160.356;
about12–14x less added host cost. This is a host benchmark, not target timing or
a worst-case FM1 deadline proof. Existing filters are executed once, unchanged.
Each active sample adds two bounded allpass recurrences, ring indexing and
side scaling/routing; no variable division, allocation, lock or unbounded work.

Native -O2 benchmark executable: text543575→548059(+4484), data16540 unchanged,
BSS306056→306088(+32, including alignment). Actual new global state is16 bytes
of reset epochs. All per-part state reuses the existing51552-byte reservation:
580 bytes/part; gr_part_t host7576→8160(including alignment), 32-bit7480→8060,
4828 bytes remaining per12888-byte partition. Grain records remain64 host/56
32-bit. No new framebuffers or bitmaps. Compiler stack reports new cloud_prepare
56 bytes, mix_part304 bounded, grain_render320 and mix_block192; base optimizer
inlined a different mix_part layout(mix_block320). These are function frames,
not measured target stack high-water or a comparable whole-call-chain bound.

The JieLi toolchain is unavailable here; no target link, flash-fit or FM1 CPU/
stack qualification is claimed. Host text growth is not target flash growth.
Required next step: target build with the existing supported toolchain, then
hardware listening and callback timing with four dense CLOUD tracks, V-SPRD,
resonance and live C changes. No pushing, flashing, release or merge performed.

Candidate probe reproduction:

```
cc -O2 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src tests/cloud_width_probe.c -lm -o /tmp/cloud-width-probe
/tmp/cloud-width-probe 0
/tmp/cloud-width-probe 1
```

Changed production files: `cloud_width.h`, `eng_grain.c`, `engines.c`, `fx.c`,
`slicer.c`, `perform.c`, `ui.c` (reset hook only). New tests: probe, strict test,
PCM/CPU2 runner and benchmark runner. Existing size-only test assertions now
include the explicit decorrelator allocation; parameter-grid assertions and
frozen PCM hashes were preserved. The quality runner explicitly recognizes the
new positive-C CLOUD effect. No build flags or build scripts changed.
