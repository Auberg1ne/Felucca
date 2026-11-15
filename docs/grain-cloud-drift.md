# Functional CLOUD DRIFT

## Scope and compatibility

Exact base: `9a924c895ba257c38c34706d77d12dc93fac71b6`.
Audit reference: `0883922bcfc7106242914ba2b94e7de4aa1ef7a1`.

Only CLOUD's automatic source-head movement changes. DRIFT remains personality
A, parameter ID 104, range 0–127, default 64 (`params.c`). No descriptors,
parameter IDs, preset values, assets, MIDI/editor/motion routes, UI or stored
formats change. Existing CLOUD projects/presets with DRIFT other than 64 now
intentionally sound different; no saved values are rewritten. GRAIN-only legacy
load protections and the CUTOFF-zero, source-position and TONE fixes remain.

## Exact mapping and arithmetic

In `firmware/src/eng_grain.c:grain_render`, immediately before the existing
bounded head advance, CLOUD scales the already-computed base movement:

```
base_Q16 = ((hs >> 8) * (stretch_factor >> 8)) * block_frames
move_Q16 = TYPE == CLOUD ? (base_Q16 * DRIFT) >> 6 : base_Q16
```

| DRIFT | Multiplier |
| ---: | ---: |
| 0 | 0× |
| 1 | 0.015625× |
| 32 | 0.5× |
| 63 | 0.984375× |
| 64 | exactly 1× |
| 65 | 1.015625× |
| 96 | 1.5× |
| 127 | 1.984375× |

The mapping is linear and monotonic. It is stateless and evaluated once per
voice/control block, with one uint32 multiply and shift. No per-sample work or
expensive operations are added. Scaling **after** the original STRETCH/pitch
rounding preserves neutral movement exactly.

For the fixed CTL=32 bound, `hs` is capped to 65535, hence `hs>>8 <=255`.
The unchanged STRETCH factor has `factor>>8 <=256` throughout its valid range.
Thus `base<=2088960` and `base*127<=265297920<2^28`, safely within uint32.
The scaled maximum is 4145280 Q16 units per block. The existing bounded
whole-sample + fraction head representation and wrapping helper are unchanged.
Fractional movement accumulates in the existing Q16 fraction, including at
small DRIFT values. Scaling rounds down by less than one Q16 unit
(1/65536 frame) per block; no additional residual state is necessary.

## STRETCH, live controls and personality isolation

STRETCH retains its existing base scan-speed calculation. DRIFT multiplies
that result in CLOUD only; it never changes individual grain playback step,
pitch, duration, density, overlap gain, envelope or randomization.

At DRIFT=0 the automatic head increment is exactly zero, preserving even the
existing fractional position. This does **not** disable POS/SHP: the previous
next-admitted-grain target-delta policy still moves future start positions.
Active grains continue naturally. WIDTH may still place grains around the
stationary head; G-SPRD retains its existing spatial/jitter behavior, including
CLOUD's existing WIDTH override of spatial spread. Neither control is tied to
DRIFT. Tests vary WIDTH and G-SPRD independently and compare complete spawn
descriptors and RNG state at an identical head across DRIFT values.

Live DRIFT edits change scan velocity on the next control block, retaining
head phase. There is no head reset, retrigger or new smoothing state. On a TYPE
switch, non-CLOUD restores the original STRETCH-derived movement, while its
shared A value keeps its existing interpretation: FREEZE HOLD, STUTTER RATE,
SHIMMER INTERVAL. Existing personality code remains untouched. Tests verify
active grain cursor/window progression across these switches.

## Numerical and audio validation

All audio comparisons use the exact base above, the same generated tables,
compiler options and a nonzero master gain. Every capture verifies peak/RMS,
nonzero sample count and all four channel meters before comparison. PCM is
compared directly, with SHA-256 recorded as additional evidence.

- Numerical suite: **2315750 checks passed**. Actual `grain_render` movement
  is compared with a uint64 reference for every DRIFT 0–127 × STRETCH 0–127 ×
  block length 0–32. Checks include exact neutral/zero, monotonic movement,
  fraction preservation, output canaries and the maximum-speed overflow bound.
- Wide wrap references include legal short/user domains, 65535,65536,65539,
  275098 and 482272 frames. Index-limited domains are head-arithmetic tests,
  not a claim that the existing seek index can play them.
- Guarded source tests cover all five factories and STRETCH 0/1/64/127,
  stationary live POS 0→127→0, fractional SHP in both directions, active-grain
  continuity and TYPE/speed changes. Fast scans wrap with forward/reverse grains;
  source surroundings are poisoned under ASan and active cursors are checked.
- **670 final-output captures pass**: the static matrix is four personalities ×
  five factories × four STRETCH values × eight A values. Twenty additional
  four-track captures exercise live POS, SHP, DRIFT and TYPE changes; ten run
  full-length traversal at DRIFT 64/127 for 640000 frames each (~14.5 seconds),
  reaching source ends and wrapping at least twice for every factory.
- **20 neutral CLOUD cases are byte-identical** to base (10485760 PCM bytes).
- **480 non-CLOUD cases are byte-identical** across all requested A/STRETCH
  values and factory sources (251658240 bytes). HOLD/RATE/INTERVAL are preserved.
- The other **140 static CLOUD cases differ intentionally**. DRIFT=0 head traces
  remain stationary; live target/speed/TYPE traces move without a note retrigger.

Test A values: 0,1,32,63,64,65,96,127. Factories retain IDs 0,2,3,13,14.
No goldens or existing expected assertions were weakened or regenerated.

## Sanitizers and retained regressions

The numerical suite and **734 four-track audio runs** pass ASan+UBSan with
strict signed-overflow, bounds, object-size and pointer-overflow checks enabled.
Only the existing DSP **shift** idioms are excluded:
`-fsanitize=address,undefined -fno-sanitize=shift
-fno-sanitize-recover=undefined`. No sanitizer diagnostics occur. The audio
runs include all 670 matrix cases and 64 extra CLOUD user-zone cases (two legal
user zones, including fractional sample rate, all A values and STRETCH endpoints).

Broader historical host tests retain their existing exclusions for signed
overflow, shift, bounds, object-size and pointer-overflow. They are reported
separately rather than described as strict UBSan coverage.

Passing broader totals: GRAIN-only selection/protections 380, editor/storage
70, persistence 24, four-track audio 330, CUTOFF-zero 2355, source position
854920, TONE 722655, theme 8229, grid visuals 1935, STRETCH descriptors 1764,
GRAIN grid 2329 and FUN11/FUN10 project formats 41146. Full UI (850), editor
(172), project (61), preset (45) and backup (103) assertions pass. The focused
web policy suite passes 20 checks.

Known failures match the exact base, including failing assertion names:
MIDI 134 passing / 15 failing; motion 97 passing / 27 failing. These were not
changed or hidden. The registered GRAIN-only suite also retains all previous
cutoff, position and TONE audio renders alongside 180 new DRIFT static/live
four-track renders.

All 270 retained UI state/pixel snapshots are byte-identical to the exact base
(the base snapshot artifact was generated/validated on the TONE branch).
Broad web remains 282 passing / 5 failing on base and fix, with identical
failing assertion names. No UI source is changed.

### Reproduction

```sh
sh tests/run_grain_only.sh
python3 tests/grain_drift_compare.py --base-tree /path/to/9a924c8 --out /tmp/grain-drift-compare
cc -O1 -g -ffp-contract=off -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=undefined -Ibuild/gen -Ifirmware/src tests/grain_drift_test.c -lm -o /tmp/grain-drift-test
/tmp/grain-drift-test
```

The usual generated tables and pinned engine dependency must already exist.
`GRAIN_DRIFT_BASE_TREE` enables exact-base comparison from the registered suite.
The comparison script uses only Python's standard library. The source-position
unit main has a test-only include guard so the new tests can reuse its guarded
source fixtures; its normal execution and assertions remain unchanged.

## Native code and RAM

Identical GCC 14.2 `-Os -ffp-contract=off` native compilation of
`web/emu/felucca_web.c`:

| Section | Base | Fix | Delta |
| --- | ---: | ---: | ---: |
| text | 879035 | 879001 | -34 bytes |
| data | 31060 | 31060 | 0 |
| BSS | 1603232 | 1603232 | 0 |

The small native text reduction reflects compiler optimization/register layout;
it is not a claim of target firmware savings. Native individual render stack
frame is 336→328 bytes (-8), not a target worst-case stack measurement.

No new state is added. Native private GRAIN state remains 7296 bytes/part,
within the existing 12888-byte reservation (5592 bytes remaining). Four fixed
part regions remain 51552 bytes total. The previous GCC 32-bit layout estimate
remains 7200 bytes/part (5688 spare); structures and voice state are unchanged.
No assets, framebuffers, buffers, allocations, locks or audio callbacks are added.

## Remaining limitations

STRETCH's existing quantization and pitch-dependent/capped base scanning remain.
A stationary head does not imply identical grains when WIDTH/RAND/jitter are
nonzero. At source ends the previous spawn fitting and live-target saturation
policies still apply. Sine fallback has no source head, so DRIFT appropriately
has no scanning effect there. No claims are made about new index capacity,
DTUNE, FREEZE/SHIMMER semantics, resonant headroom or other audited limitations.
TONE's coefficients, smoothing and exact settled bypass are untouched.

Emscripten and the JieLi target toolchain are unavailable; no tools were installed.
Native size is not a target link. Hardware fit within the 581564-byte image
limit remains unverified until a successful target build.

## Changed files

- `firmware/src/eng_grain.c`: CLOUD-only multiplier before existing head advance.
- `tests/grain_drift_test.c`: numerical, guarded live/boundary and spawn isolation checks.
- `tests/grain_drift_audio_test.c`: real four-track output/head trace harness.
- `tests/grain_drift_compare.py`: exact-base identity and intentional-change matrix.
- `tests/grain_position_test.c`: test-only main guard for fixture reuse.
- `tests/run_grain_only.sh`: register tests and optional exact-base comparator.
- `docs/grain-cloud-drift.md`: this report.
