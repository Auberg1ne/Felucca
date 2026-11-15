# Functional GRAIN UNISON DTUNE

## Scope and compatibility

Exact base: `1f0cbb4d24cc26b5a4929f9a33cf1e457b080f1b`.
Audit reference: `0883922bcfc7106242914ba2b94e7de4aa1ef7a1`.

Only GRAIN grain-rate calculation changes. DTUNE remains ID 44, range 0–127,
default 40 (`params.c`). No allocator, common pitch helper, other engine,
parameter descriptor, UI, sample asset, preset value, MIDI/editor/motion route
or serialized layout changes. Saved UNISON patches with nonzero allocated fine
offsets now intentionally sound different. Values are not rewritten. The
CUTOFF-zero, POS/boundary, TONE, CLOUD DRIFT and GRAIN-only protections remain.

## Fine-pitch conversion

`voice.c:mono_play` sets each UNISON voice's `voice_t.fine` in units of
1/4096 of playback rate. For GRAIN's three voices and the common NVOICE=8
budget, its existing calculation gives:

```
k = [-7, 0, +7]
fine = trunc_toward_zero(k * DTUNE * 56 / 889)
```

`eng_grain.c:gr_spawn_one` retains all existing coarse pitch, PTCH, RAND and
SHIMMER interval arithmetic, then applies:

```
raw_step = (pow2_q16(clamp(d16, -1536, 576)) >> 8) * (zone_rate >> 8)
if (voice.fine != 0)
    step = ((uint64)raw_step * (4096 + voice.fine)) >> 12
else
    step = raw_step
step = clamp(step, 256, 131072)
```

This is the allocator's existing linear rate convention, not a new cents
scale. Full-width multiplication avoids losing low-rate precision by shifting
`raw_step` first. It rounds down by less than one Q16 step unit. Fine=0 skips
the operation entirely; the center voice retains its original rate. The signed
fine range is [-56,+56], so the multiplier is positive in [4040,4152].
The legal pre-cap rate can reach 2097152 for a 4× user source; multiplying by
4152 gives 8707375104, beyond uint32 but safely within uint64. The result fits
uint32 before applying the unchanged minimum and 2× maximum. No signed pitch
multiplication or negative-to-unsigned subtraction is introduced.

The existing `d16` coarse limit and SHIMMER interval calculation are untouched.
Fine acts after coarse pitch conversion and before the existing sample-rate
ceiling. Span fitting and forward/reverse bounds use the already-repaired wide
arithmetic on the final, capped step. Decode-load admission is still capped
at eight source decodes/sample per part (reverse grains count twice).

### Why use voice.fine rather than m.fine or m.inc?

`track_render` supplies `m.pitch16` for coarse pitch and `m.fine` as the sum
of UNISON fine, global-TUNE remainder and MIDI-bend remainder. `m.inc` is a
fine-adjusted oscillator phase increment, not an ADPCM source step. Applying
it would mix representations or double-count pitch.

This deliberately narrow fix reads only the allocator's `t->v[vi].fine`.
It therefore preserves DTUNE=0 even when global tuning/bend has a nonzero
residual. Coarse TUNE/bend behavior remains as before; previously ignored fine
residuals are not newly enabled. Numerical tests inject unrelated `m.fine=777`
and retain the expected allocator-only rates. Real mixer tests also include
nonzero global-TUNE and bend residuals in identity comparisons.

## Allocator lifecycle, stereo and scanning

A parameter edit during a held note does not recompute `voice.fine`. Future
grains keep that cached offset until the existing allocator calls `mono_play`
on a winning note event, a held-stack fallback or a fresh/retriggered note.
Repeated same-note events may take the existing early-return path. No new
live-update policy is added. Once the cached offset changes, newly spawned
grains use it; already-running grains retain their captured step and finish
normally. Tests verify both cached offsets and retained grain descriptors.

POLY allocations explicitly clear fine; MONO/LEGATO use one voice and allocate fine=0.
Those cases are byte-identical. If the common allocator supplies nonzero fine,
GRAIN now consumes that term regardless of how the parameter was edited.

For three voices the existing stereo-side pattern is **[0,1,0]**: the two
outer voices have opposite fine offsets but the same alternating side index.
This patch does not reinterpret them as opposite pan positions. V-SPRD routing
is preserved exactly, including that pre-existing pattern.

Head scanning still reads only the existing coarse pitch/STRETCH/CLOUD DRIFT
calculation. Fine is not added there. Actual head traces match base under
equivalent admitted-voice conditions. At decode-load limits a changed grain
rate can affect admission/first-head-seed timing through the existing safety
rules; this is not a new scan multiplier or scheduler policy.

## Measured grain rates and boundaries

At a 44.1 kHz zone's root, with PTCH=0 and RAND=0, actual spawned steps are:

| DTUNE | Allocated fine offsets | Negative / center / positive Q16 step |
| ---: | --- | --- |
| 0 | 0, 0, 0 | 65536 / 65536 / 65536 |
| 1 | 0, 0, 0 | 65536 / 65536 / 65536 |
| 40 | -17, 0, +17 | 65264 / 65536 / 65808 |
| 64 | -28, 0, +28 | 65088 / 65536 / 65984 |
| 127 | -56, 0, +56 | 64640 / 65536 / 66432 |

DTUNE=1 rounds to zero in the unchanged allocator. At 127, the actual rate
factors are 0.986328125 and 1.013671875 (approximately -23.83/+23.51 cents).
The allocator comment's approximate cents description is not a new scale or a
reason to change its formula. Increasing the tested nonzero offsets produces
strictly increasing separation where neither bound applies.

At the 2× rate cap, positive/center/negative voices may partially or completely
converge. The minimum step can likewise collapse separation at very low rates.
Tests expect those limits rather than requiring arbitrary PCM differences.
Forty fully upper-capped four-track renders at nonzero DTUNE remain identical
to base, explicitly confirming no ceiling increase.

Strict numerical tests pass **75537 assertions**, including:

- Actual common allocator, all four personalities and DTUNE 0/1/40/64/127.
- **6480 admitted rate cases**, all five factory recordings and a legal 4×
  user zone, PTCH -24/0/+24, RAND 0/127, forward/reverse starts and both SHIMMER
  interval endpoints. Expected steps use an independent wide reference;
  **2790** cases reach the upper cap.
- Center pitch, correctly ordered outer offsets, raw bounds, source-end fitting,
  output canaries and per-voice source-head movement independent of fine,
  including all four STRETCH/CLOUD DRIFT endpoint combinations.
- **360** full-grain short-zone renders at 19/35/4099 frames, all personalities,
  DTUNE 0/40/127, all three offsets, both directions and both source ends.
  **72** minimum-length refusals remain valid; no assertions are removed to
  force tiny zones to sound. Source surroundings are poisoned under ASan and
  cursor progression is checked after every block.
- Held edits, winning-note updates and unchanged already-running grain steps.

## Audio identity and intentional changes

**1400** deterministic final-output comparisons pass against the exact base:

| Case | Byte-identical captures |
| --- | ---: |
| Allocated/cached fine=0 (DTUNE=0, including held edits and residual-tuning controls) | 560 |
| DTUNE=1 allocator dead zone | 40 |
| Fresh POLY/MONO/LEGATO, including residual-tuning controls | 140 |
| Fully upper-capped nonzero-DTUNE four-track playback | 40 |
| Total | **780** |

The 780 identities compare **408944640 PCM bytes** directly. The other **620**
captures intentionally differ, with changed actual grain-step histories:
120 uncapped three-voice cases, 480 four-track parameter-extreme cases and
20 winning-note update cases. All four personalities and five factory sources
are covered. PTCH -24/0/+24, RAND 0/127, SHIMMER A endpoints, V-SPRD 0/127 and
STRETCH/CLOUD DRIFT endpoints are crossed in the four-track matrix. No cap case
is required to differ merely because DTUNE is nonzero.

All capture fixtures use master=4096 and explicitly check peak/RMS/nonzero
sample count before directly comparing PCM bytes. Actual grain-step ranges
and hashes are recorded alongside PCM, as are head-history checksums and voice-budget
counts. Exact per-block head-reference assertions are in the numerical suite. Uncapped three-voice cases require grain admissions for every voice.
No existing fixture, golden, waveform asset or parameter expectation is weakened.

Four tracks cannot each have three simultaneous sounding voices under the
unchanged global eight-voice budget. Tests separately exercise three UNISON
voices on one track and four sounding tracks sharing the existing budget,
asserting no more than eight non-fading voices. Existing decode-load limits
can also reject grains at extreme reverse/high-rate settings; they are retained.

## Sanitizers and regressions

The numerical suite and **720 four-track/three-voice audio runs** pass strict
ASan+UBSan with signed-overflow, bounds, object-size and pointer-overflow checks
active. Only existing DSP **shift** idioms are excluded:
`-fsanitize=address,undefined -fno-sanitize=shift
-fno-sanitize-recover=undefined`. No diagnostics occur. The audio matrix covers
all personalities/factories, every requested DTUNE, PTCH, RAND, stereo and
STRETCH/DRIFT endpoints, held/retriggered notes, shared voice limits and a
fractional-rate user fixture.

Broader legacy suites retain their existing exclusions for signed overflow,
shift, bounds, object-size and pointer-overflow. They are separate evidence,
not claimed as strict numerical coverage.

The registered GRAIN-only suite completes, retaining all previous audio checks
and adding 160 DTUNE renders. Passing totals include GRAIN-only selection and
legacy protections 380, editor/storage 70, persistence 24, four-track audio 330,
CUTOFF-zero 2355, source position 854920, TONE 722655, CLOUD DRIFT 2315750,
theme 8229, grid visuals 1935, STRETCH descriptors 1764, GRAIN grid 2329 and
FUN11/FUN10 project formats 41146. Full UI (850), editor (172), project (61),
preset (45) and backup (103) assertions pass. Focused web policy: 20 passing.

All **270 retained UI state/pixel snapshots are unchanged** relative to the
exact base's validated artifact. MIDI remains 134 passing / 15 failing and
motion 97 passing / 27 failing, with identical failing assertion names on base
and fix. Serial broad web results are 282 passing / 5 failing on both, with
identical five known failure names.

Two earlier web runs on the fix had 281 passing / 6 failing: the additional
`live: PING keeps WATCH on` assertion failed. That test uses a real 250 ms mock
watch expiry with 120 ms sleeps; both web test and editor JavaScript hash
identically to base. A subsequent serial run after other workloads ended
returned to the five known failures. A diagnostic of the unchanged base live
fixture also passed six consecutive trials. The observed timing-sensitive
failure is recorded here, not hidden by changing timeouts, assertions or
production web code. No new deterministic firmware failures were observed.

### Reproduction

```sh
sh tests/run_grain_only.sh
python3 tests/grain_dtune_compare.py --base-tree /path/to/1f0cbb4 --out /tmp/grain-dtune-compare
cc -O1 -g -ffp-contract=off -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=undefined -Ibuild/gen -Ifirmware/src tests/grain_dtune_test.c -lm -o /tmp/grain-dtune-test
/tmp/grain-dtune-test
```

The usual pinned engine dependency and generated tables must already exist.
`GRAIN_DTUNE_BASE_TREE` enables the exact-base matrix from the registered runner.
The comparison script uses only Python's standard library.

## Native size, engine memory and stack

Identical GCC 14.2 native `-Os -ffp-contract=off` compilation of
`web/emu/felucca_web.c`:

| Section | Base | Fix | Delta |
| --- | ---: | ---: | ---: |
| text | 879001 | 879053 | +52 bytes |
| data | 31060 | 31060 | 0 |
| BSS | 1603232 | 1603232 | 0 |

Native individual `gr_spawn_one` stack frame is 104→112 bytes (+8);
`grain_render` remains 328 bytes. These are not target worst-case call-stack
measurements. Multiplication is performed only at grain spawn, not per sample.

No persistent state is added: `voice.fine` is already allocator-owned. Native
private GRAIN state remains 7296 bytes/part, within 12888 reserved (5592 spare).
Four fixed engine regions remain 51552 bytes. The previous GCC 32-bit layout
estimate remains 7200 bytes/part (5688 spare); structures and voice state are
unchanged. No new buffer, framebuffer, bitmap, allocation or audio callback.

## Remaining limitations

The allocator's DTUNE dead zone, cached held-note behavior, eight-voice budget
and three-voice alternating stereo pattern remain. Coarse-pitch quantization,
RAND, STRETCH, CLOUD DRIFT, source-index capacity, FREEZE semantics, SHIMMER's
coarse clamp, TONE and resonant-filter headroom are unchanged. Only ADPCM grain
playback is connected here; the existing empty-source sine fallback and global
TUNE/bend fine-residual limitations are unchanged.

Emscripten/JieLi tools are unavailable; no dependencies were installed. Native
object size is not a target link or target CPU-cost measurement. Fit within the
581564-byte hardware image limit remains unverified until a target build succeeds.

## Changed files

- `firmware/src/eng_grain.c`: allocator fine factor before existing rate cap.
- `tests/grain_dtune_test.c`: allocator/rate/lifecycle/boundary checks.
- `tests/grain_dtune_audio_test.c`: real mixer PCM, steps, heads and budget evidence.
- `tests/grain_dtune_compare.py`: exact-base identity and changed-rate matrix.
- `tests/run_grain_only.sh`: register focused tests and optional comparator.
- `docs/grain-dtune.md`: this report.
