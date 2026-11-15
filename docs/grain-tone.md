# Functional GRAIN TONE

## Scope and compatibility

Base: `00f21a456e0dce4029a3a57bbd1481c2f26956b1`.
Audit: `0883922bcfc7106242914ba2b94e7de4aa1ef7a1` (previously ineffective TONE).

TONE remains ID 117, range 0–127, default 127. No descriptors, presets,
parameter routing, UI, sample assets or storage formats change. Saved presets
with TONE below 127 now intentionally sound darker; their values are not
rewritten. CUTOFF-zero and source-position/boundary corrections remain intact.
Other engines and the common resonant filter helper are untouched.

## Algorithm and coefficients

`firmware/src/eng_grain.c` applies an independent one-pole low-pass to the
mono grain accumulator (or fallback sine), after personality synthesis/gating
and before the existing CUTOFF/RESON stage. It uses SAMPLE's inexpensive
one-pole convention, with a separate coefficient mapping and state:

```
a_target = TONE == 127 ? 16384 : pow2_q16(-(127 - TONE) * 10) >> 2
x = clamp(accumulator, -65535, 65535)
y += trunc_toward_zero((x - y) * a / 16384)
```

The coefficient grows approximately exponentially with TONE, from about
0.0102 at 0 (~72 Hz) through 0.103 at 64 (~760 Hz), toward unity. Frequencies
are approximate discrete one-pole values at 44.1 kHz, not new parameter units.
The existing approximate `pow2_q16` is used once per rendered voice/block;
127 explicitly returns exact unity rather than relying on approximation.
There are no per-sample coefficient-table calculations or general divisions.
The fixed power-of-two division is optimized by the compiler.

Input limiting matches the existing downstream limit. Given `x,y` in
[-65535,65535] and `a` in [0,16384], the largest signed multiply is
`131070 * 16384 = 2147450880 < INT32_MAX`. The update stays in the convex
input/state interval. Signed division truncates symmetrically, avoiding a
negative-shift DC bias. No common helper or CUTOFF/RESON coefficient changes.

CUTOFF=0 continues to reset its own filter state and apply the unchanged
amplitude ramp/dry accumulation, but now receives the TONE-shaped signal.
Negative/positive CUTOFF retain the existing HP/LP behavior and resonance.
TONE is not routed to FLT modulation; existing modulation routing is preserved.

### Exact bypass and live edits

Fresh notes initialize `a` to the current target. During sustained notes,
each sample moves the coefficient toward its target by
`sign(delta) * max(1, abs(delta) >> 8)`. The main time constant is about
256 samples / 44.1 kHz = 5.8 ms. The minimum step guarantees finite convergence
and exact return to bypass. Rapid direction changes continue from the current
coefficient/state rather than restarting a fade or resetting the filter.

At settled TONE=127, the accumulator is never modified. Only its last limited
sample is cached for smooth re-entry. On approaching bypass during a live
edit, the coefficient progresses to unity, then the original sample is passed
unchanged. The ordinary input clamp and downstream filters remain as before.
Numerical output values, note scheduling, grain state and active envelopes are
not reset by a TONE edit.

Tests bound the first-sample contribution of any parameter jump relative to
continuing the previous coefficient at <=512 integer sample units for the
full ±65535 input range. Constant input remains flat through all 128×128
parameter transitions. This checks the absence of a hard filter-state or bypass
switch; it is not a claim that arbitrary modulated audio has zero waveform slope.

## State ownership and lifetime

Reuse `voice_t.s[5]` for the TONE sample state and `s[6]` for its Q14
coefficient. A source search confirms neither slot is otherwise consumed by
GRAIN. GRAIN's existing `s[0..4]`, source heads, phases and random state remain
unchanged. These are engine-private slots in the existing voice structure,
not shared filter state or additional part memory.

`grain_note_on` initializes the slots on a fresh (`env==0`) note and retains
them on sounding retriggers. `voice_start` calls engine note-on; GRAIN has no
keep bits that overwrite these slots afterward. Engine switching in
`voice.c:engine_block` fades old voices, clears their active/envelope state,
then starts waiting notes on the new engine. Thus another engine's slot values
cannot enter a fresh GRAIN render. Source changes during a held note preserve
the TONE state, avoiding a new discontinuity. No allocations, audio buffers,
new fields, locks, timers or audio callbacks.

## Audio and spectral validation

All comparisons use the exact base above. No snapshots or goldens were
regenerated. Tests explicitly set master=4096 and verify peak/RMS/nonzero
sample count before comparing PCM; four active tracks must each meter nonzero.

- **722655 strict numerical checks passed**: parameter invariants, monotonic
  coefficients, full-scale arithmetic bounds, buffer canaries, zero-length
  calls, block partition independence, state initialization/retrigger lifetime,
  all-pair transition bounds and frequency-response measurements.
- **784 final-output captures passed**, covering all four personalities, all
  five factory recordings, a test-only legal user zone and empty-user sine
  fallback. Values 0,1,32,64,96,126,127 are tested.
- **280 exact-base TONE=127 comparisons are byte-identical**, totaling
  146800640 compared PCM bytes. This covers every personality/source and all
  CUTOFF -50,-25,0,25,50 × RESON 0/127 combinations with four tracks.
- Every personality also crosses all seven tones with all cutoff/resonance
  combinations, rotating factory source material. Live four-track runs switch
  127→0→127→64→1→126→96→32 during held notes; voice age and bounded TONE
  state are checked throughout.

### High-frequency evidence

The numerical test measures steady sine RMS at 100,500,2000,8000,14000 and
20000 Hz. For each frequency, all seven tones produce strictly increasing RMS.
At 8 kHz, measured RMS is **201.004 / 2123.282 / 21212.404** for TONE
0 / 64 / 127 respectively (about -40.5 / -20.0 / 0 dB).

The final-output matrix measures first-difference energy: its frequency
weighting is `4*sin(pi*f/Fs)^2`, so it is a high-frequency energy measurement,
not a PCM-hash proxy. Absolute energy is used because a low-pass reduces a
pure sine's amplitude without materially changing normalized spectral shape.
All 64 complete curves (including 28 personality/source curves and the
cutoff/resonance combinations) increase with TONE within a 0.01% finite-window
and integer rounding tolerance. TONE=0 has less than half the bypass HF energy
on every curve; the sine fallback also remains sounding.

Example CLOUD four-track HF energies (mean-square first difference):

| Source | TONE 0 | TONE 64 | TONE 127 |
| --- | ---: | ---: | ---: |
| CELLO | 645.715 | 8320.341 | 163263.484 |
| FLUTE | 8873.910 | 874166.303 | 3584907.346 |
| GUITAR | 2221.094 | 44538.126 | 92245.635 |
| SYNTH | 499.158 | 4611.137 | 30092.941 |
| VOX | 1694.945 | 117595.573 | 634477.379 |
| User zone | 22.387 | 306.900 | 3676.855 |
| Sine fallback | 17170.161 | 120608.435 | 123560.212 |

The user fixture references existing ADPCM bytes through a test-only user-zone
descriptor; no actual upload or flash fixture modification is involved. The
sine fallback is forced by an empty user source, not by substituting an asset.

## Sanitizers and regressions

The new numerical suite and 140 four-track audio runs pass ASan+UBSan with
only existing **shift** idioms excluded:
`-fsanitize=address,undefined -fno-sanitize=shift
-fno-sanitize-recover=undefined`. Strict signed-overflow, bounds, object-size
and pointer-overflow checking remain enabled. Audio runs include every source
and personality, live tone changes, cutoff signs/extremes and maximum resonance.
No diagnostics occurred on the new path or these integration runs.

The historical analytic CUTOFF-zero fixture now explicitly sets TONE=127
rather than inheriting CLOUD PAD's TONE=100. Its synthetic voice also calls normal `grain_note_on` before constructing the
grain pool, so it has initialized engine state. Its exact dry/ramp/output-canary
assertions are unchanged; the independent TONE tests cover the newly audible
low-tone path. This is required fixture isolation, not relaxation of CUTOFF.

The broader host suite retains its existing DSP exclusions for signed overflow,
shift, bounds, object-size and pointer-overflow. It is reported separately from
strict runs. Passing totals: GRAIN-only selection/protections 380,
editor/storage 70, persistence 24, four-track audio 330, theme 8229, grid visuals
1935, STRETCH descriptors 1764, GRAIN grid 2329 and FUN11/FUN10 formats 41146.
The complete UI (850), editor (172), project (61), preset (45) and backup (103)
assertions pass. All 270 retained UI state/pixel snapshots are byte-identical
to the exact base. The focused web policy suite passes 20 checks. The registered suite also passes
the retained 854920-check source-position suite, the corrected 2355-check
CUTOFF fixture (also strict-sanitizer validated), 16 cutoff renders, 28 position
renders and 96 TONE static/live renders. All registered tests complete.

Known failures match the exact base, including failing assertion names:
MIDI 134 passing / 15 failing; motion 97 passing / 27 failing; broad web 282
passing / 5 failing. No baseline expectations or goldens were changed.

### Reproduction

```sh
sh tests/run_grain_only.sh
python3 tests/grain_tone_compare.py --base-tree /path/to/00f21a4 --out /tmp/grain-tone-compare
cc -O1 -g -ffp-contract=off -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=undefined -Ibuild/gen -Ifirmware/src tests/grain_tone_test.c -lm -o /tmp/grain-tone-test
/tmp/grain-tone-test
```

The ordinary pinned engine dependency and generated build tables must exist.
`GRAIN_TONE_BASE_TREE` enables the comparison from the registered suite.
Only Python's standard library is needed for the comparison script.

## Resource measurements

Identical GCC 14.2 native `-Os -ffp-contract=off` compilation of
`web/emu/felucca_web.c`:

| Section | Base | Fix | Delta |
| --- | ---: | ---: | ---: |
| text | 878738 | 879035 | +297 bytes |
| data | 31060 | 31060 | 0 |
| BSS | 1603232 | 1603232 | 0 |

Private GRAIN state remains 7296 native bytes / 12888 reserved per part,
5592 bytes remaining. The four-part fixed reservation stays 51552 bytes.
`voice_t` and all persistent layouts are unchanged: eight existing bytes per
voice are assigned ownership, with **zero added RAM**. The prior 32-bit layout
estimate remains 7200 bytes/part; no state structure changed in this patch.
Native render stack frame is 328→336 bytes (+8), and note-on is 32→40 (+8); this is an individual compiler
frame measurement, not a target worst-case call-stack estimate.

Emscripten and the JieLi target toolchain are unavailable. No tools were
installed and no target firmware link is claimed. Native text growth is not
proof of fit within the 581564-byte target firmware image limit.

## Limitations

A one-pole is intentionally gentle (6 dB/octave), not a brick-wall filter or
loudness-compensated EQ. Dark settings reduce energy on bright material without
adding normalization. Integer-state quantization can leave a sub-update residual
of fewer than `16384/a` sample units; envelopes still terminate released voices.
Near bypass, tiny finite-window energy differences below the documented 0.01%
floor can occur on low-frequency signals. Settled 127 remains exact.

The existing resonant-filter headroom issue, CLOUD DRIFT, DTUNE, STRETCH scaling,
FREEZE semantics, SHIMMER pitch limits and seek-index capacity are unchanged.
No audible claims substitute for the measured response or hardware validation.

## Changed files

- `firmware/src/eng_grain.c`: TONE filter, coefficient smoothing and state lifecycle.
- `tests/grain_tone_test.c`: strict arithmetic, response, transition and ownership checks.
- `tests/grain_tone_audio_test.c`: final mixer render, live controls and state bounds.
- `tests/grain_tone_compare.py`: exact-base identity and HF matrix.
- `tests/grain_cutoff_zero_test.c`: explicit transparent TONE in the dry fixture.
- `tests/run_grain_only.sh`: registered tests and optional exact-base comparison.
- `docs/grain-tone.md`: this report.
