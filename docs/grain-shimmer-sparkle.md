# SHIMMER SPARKLE — Task241

Base `74533962cf44d7362497c5d3ed101a885e0e295d`, existing development branch
`feat/coffee-grains-granular-quality`. Local-only change; no push or hardware flash.

## DSP decision

SHIMMER already invokes `gr_spawn_one` for a base grain and optionally a harmony
grain (INTERVAL/HARMONICS, IDs104/105). SPARKLE reuses occasional **base** grains
as octave-up accents. It does not add a third grain or require HARMONICS>0.
The optional harmony's interval, gain, RNG and initialization remain exact.

At spawn, for TYPE3 and base pitch_offset0 only, C118 controls a probability
`C/200`: C0=0%, C25=12.5%, C50=25%, C100=50%. The threshold uses unused high16
bits of the existing second xorshift draw; source positioning uses its low16.
No RNG draw is added. The threshold is nested/monotonic for a fixed draw. This
is continuous probability control, not a promise of monotonic waveform RMS or
spectral energy on a finite, phase-sensitive granular capture.

An accent adds192 pitch16 units (12 semitones) before the unchanged pitch helper,
UNISON fine factor and existing2× rate cap. All current pitch, PTCH, RAND and
harmony calculations remain in place. Accents receive the existing window,
normalization and gain, without a gain boost. Existing fitting, reverse decode,
load/admission checks and filter paths then operate normally. Source scanning,
scheduling code and grain pool sizes are untouched. C0 takes the old arithmetic
path exactly. CLOUD/FREEZE/STUTTER never read C here.

C is a per-track16-bit scalar, published through the existing parameter path.
Like existing personality controls it is sampled at spawn; its effect is captured
in the grain's existing `step`. No callback keeps a pointer to a changing C value
or retunes an active grain. Live edits therefore change subsequent grains only;
there is no hard output switch, new fade, allocation, lock or new persistent state.
Invalid negative values behave as0; positive values above100 are bounded to100.
Persistence, command82 negotiation, MIDI quarantine and the empty UI cell remain
unchanged. This task exposes no physical knob or new editor control.

## Tests

```sh
# base-tree must be a clean worktree of7453396 with its existing engine submodule
python3 tests/run_grain_sparkle.py --base-tree /tmp/coffee-sparkle-745 \
  --out /tmp/grain-sparkle/full
python3 tests/run_granular_quality.py --sanitize \
  --oracle-tree /tmp/coffee-grc-fd7 --out /tmp/grain-sparkle/regression
python3 tests/run_editor_stable.py --baseline-tree /tmp/coffee-grc-fd7 \
  --e-tree /tmp/coffee-e-371 --out /tmp/grain-sparkle/editor
```

The first runner compiles both DSP revisions against the same current test harness
and generated source assets. It produces raw PCM, measurements.json and unchanged-
level stereo SYNTH-SPARKLE-C0/25/50/100.wav listening files outside Git. There is
no firmware normalization or listening judgment in these numerical results.

- New strict kernel: **1411308 checks**, including five sources, all personalities,
  C0/1/25/50/100/out-of-range100-clamp, known seeds, exact expected octave rates,
  unchanged two-draw RNG consumption, harmony identity, active-grain latching,
  forward/reverse fitting, pitch extremes, high RAND, UNISON fine, pool/load caps
  and output canaries. Grain64/part7296/track1840 byte assertions pass.
- For1280 independent source/seed combinations: accent counts atC0/1/25/50/100
  are0/15/240/415/650. These finite seeded counts demonstrate nested selection,
  not an empirical estimate of a uniform full-cycle RNG distribution.
- **24/24** base-versus-new C0 final-mixer PCM and explicit state records match
  exactly. **15/15** pure CLOUD/FREEZE/STUTTER C100 cases match the base exactly.
- All five factory sources produce substantive nonzero differences atC25/50/100;
  each capture repeats exactly, and the three nonzero C outputs are distinct.
  Live0→100→25→50 changes during held reverse playback repeat exactly for all five
  sources. Four simultaneous tracks pass C25/50/100, pool and decoder-load bounds.
- Existing protocol/storage gates: stable editor2909, WIN editor110, C editor2259,
  project bridge2189, persistence109038 and exact old-reader downgrade362 checks
  pass; browser compatibility89 and WIN provenance11 pass; user presets pass.
- Existing24 frozen PCM/state cases still pass with C0, including RNG, admission,
  reverse decoding, modulation and the corrected bipolar filter. Existing C100
  regression classification is intentionally updated:15 pure non-SHIMMER cases
  stay exact;9 SHIMMER/four-track cases now differ. No golden or fixture changed.
- Strict short-envelope gate: **1851392 cases /152924099 checks**,306→0 silent Hann
  cases. Strict STUTTER/GLITCH gate: **42069041 checks**,16384 rate/gate settings.
- Strict bipolar transition gate: **3553312 checks**, worst first edge0, all four
  personalities, resonance extremes and manual/modulation-driven transitions.

Strict grain/kernel/filter probes run ASan+UBSan with recovery disabled and
signed-overflow checks enabled. The full mixer harness retains its pre-existing
signed-overflow/shift exclusions for unrelated legacy mixer/ADPCM paths. No new
exclusion or weakened boundary assertion is introduced.

Representative high-density captures (unchanged master/gain,65536 stereo frames):

| Source | C0 RMS | C100 RMS | C100 peak | C100 difference RMS vs C0 |
|---|---:|---:|---:|---:|
| CELLO0 |1497.97 |1376.26 |6854 |1277.64 |
| FLUTE2 |3608.47 |2940.55 |16162 |see measurements.json |
| GUITAR3 |1761.58 |1367.10 |9062 |see measurements.json |
| SYNTH13 |2445.01 |1998.82 |7926 |2180.37 |
| VOX14 |1391.84 |1092.42 |8055 |1189.42 |

Peak over all new/base regression captures is19771, below32767. Adjacent-sample
jumps and first-difference energy are recorded per capture. For SYNTH, C0/25/50/100
first-difference energies are3136.29/3252.29/3491.89/3550.50. Other recordings need
not show monotonic aggregate energy: pitch changes alter phase, overlap and
rate-cap admission. Substantive PCM difference plus exact octave-rate tests is
the evidence of an active accent; musical preference/click acceptability still
requires listening on FM1.

## Resources and realtime cost

Host `cc -Os -ffp-contract=off -fstack-usage`, same benchmark translation unit
against exact base and new DSP: **text+76, rodata0, data0, BSS0 bytes**.
`gr_spawn_one` stack112 and `grain_render`272 bytes are unchanged (host compiler
frames, not a measurement of the target interrupt stack). No grain/part growth.

No per-sample envelope/filter operation is added. A base SHIMMER grain candidate
adds a bounded C check, high-bit threshold comparison and an integer octave add
when selected. C's multiply by65536 is a shift; the other multiply is constant200.
No variable division, float, table generation or new source decoder is introduced.
Accents can decode faster or fit shorter; they still count against the existing
GR_LOAD_MAX8 and2× grain-rate ceiling. Rejections and subsequent state/RNG can
therefore intentionally differ for nonzero C; C0 remains exact.

Five-run CPU-time medians,32768 high-density blocks, SYNTH, RAND127, no file I/O:

| Tracks | Base C0 ns/frame | New C0 | New C100 |
|---|---:|---:|---:|
|1 |138.673 |137.768 |134.691 |
|4 |444.972 |457.969 |444.257 |

These are noisy host measurements (C0 four-track variation+2.9%), not FM1 timing
or proof of a speedup. C100 has a different bounded admission workload. Use
`tests/grain_sparkle_bench.c` with the same flags, C and track-count arguments;
compile the base with `-DSPARKLE_UI_SOURCE="/tmp/coffee-sparkle-745/tests/ui_test.c"`.
Read host sections with `size -A`; files under /tmp/grain-sparkle/resources record
all measurements. No installed default JieLi pi32v2 compiler was found. Target
flash/RAM/POOL/CPU and stack fit are **unverified**; no toolchain was installed.

## Compatibility and remaining limits

The Step240F storage/protocol versions are unchanged. The existing editor/WIN and
old-reader downgrade gates are rerun separately; historical broad editor, motion,
backup and robustness failures documented at the base are not fixed or claimed
green here. C0 identity is checked against the exact starting commit rather than
against those already-failing broad tests.

At the existing2× source-rate cap an octave accent can converge to the ordinary
rate. This is intentional; the ceiling is not raised. Empty/unavailable sources
use the existing sine fallback, which has no grains and no SPARKLE accents.
Linear-resampling aliasing, loudness/overlap normalization and source-index limits
remain unchanged. No blanket clipping guarantee is made for arbitrary resonant
patches; the tested sounding fixtures and existing safety paths remain bounded.

Changed files: firmware/src/eng_grain.c; tests/granular_quality_test.c,
tests/run_granular_quality.py,tests/grain_sparkle_test.c,
tests/grain_sparkle_bench.c,tests/run_grain_sparkle.py; this report.
Next safe step: hardware listening/CPU qualification through the already supported
explicit scalar C path, with no default/preset changes or UI exposure in this task.
