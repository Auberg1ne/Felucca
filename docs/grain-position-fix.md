# GRAIN source position and boundary repair

## Scope and compatibility

Base: `fc0fc15dac1f11d4b17a9196b42a282676cb51eb` (CUTOFF-zero).
Audit: `0883922bcfc7106242914ba2b94e7de4aa1ef7a1`, findings F2, F3 and F6 only.
The CUTOFF-zero correction, GRAIN-only selection and non-destructive legacy
project protections remain intact. No parameter IDs, descriptors, serialized
formats, sample assets, MIDI/editor protocol, UI rendering or DSP algorithms
outside source positioning and boundary fitting were changed.

## Head representation and movement policy

`firmware/src/eng_grain.c` replaces each overflowing packed uint32 Q16 head
with an eight-byte tuple: uint32 whole sample, uint16 fraction, uint16 last
admitted effective POS/SHP target. The usable whole-sample domain is
`[0, zone_frames - 4]`, wrapping modulo `zone_frames - 3`. Fractional Q16
movement and the original pitch/STRETCH movement calculation are retained.
The whole index can represent all supported factory and user zone lengths;
this does not expand the separate seek-index capacity.

The fraction addition is at most 131070. Carry is added to the movement's
whole part, not to an overflowing packed position. When movement is smaller
than the distance to the end, addition stays within the domain. Otherwise,
subtract that distance and reduce modulo the domain. Zero domains are guarded.
Division occurs only on a block-level wrap, never in the per-sample loop.
Tests also exercise arbitrary uint32 domains/movement against a wide reference.

The original normalized interior POS mapping is retained, with its endpoint
bounded to the usable domain. Its intermediate `target * 65535` fits uint32
because target is at most `127 << 8`. The long-source integer part no longer
truncates to 16 bits.

### Live POS and SHP

At the next **admitted grain spawn**, compare the clamped effective POS/SHP
target with the cached target. Apply the difference between their mapped
sample anchors to the current scanning head, saturating at either source end.
Preserve the fractional scan phase. An unchanged target does not re-anchor;
normal scanning continues. Continuously varying SHP uses this same delta rule,
including sub-parameter Q8 changes. Natural scanning wraps; manual target
movement saturates rather than wrapping a high POS immediately to the start.

A candidate head is committed only after seek readiness and load checks admit
the grain. Rejected starts do not consume a pending target. This also ensures
that a new POS=127 reaches a grain before normal scanning can wrap it away.
Existing grains finish naturally; POS/SHP do not clear grains or retrigger notes.
Source/generation changes retain the existing pool/index reset and now invalidate
all head caches, including switches between sources with the same zone index.
Initial anchoring occurs on the first admitted grain, so cold index/readiness
timing can differ from the old prematurely cached head.

## Span arithmetic and boundaries

Grain span uses a uint64 product. Forward progression uses
`floor(length * step / 65536)`; reverse progression uses the corresponding
**ceil**, because its initially zero fractional accumulator subtracts the step.
The reverse rounding is important: using floor can underestimate travel by one
frame even without multiplication overflow.

If span exceeds `zone_frames - 3`, fit length with
`floor(((uint64)domain << 16) / step)`. Steps aligned to 256 retain the exact
32-bit division fast path; fractional user rates use the full-width quotient.
The existing minimum length of 16 remains enforced. For example, the extreme
68703-frame, 131072-step product is 9005039616, beyond uint32; its correct span
is 137406 rather than the wrapped result. Starts are constrained so forward
and reverse progression remain within the valid source interval. Tests verify
actual decoder cursor travel after every block, not just eventual guarded reads.
No audio allocation or additional persistent buffer is introduced.

## Validation

All results below compare the exact base above; no goldens were regenerated.

| Check | Result |
| --- | --- |
| Numerical/head/live/boundary unit suite | 854920 checks passed |
| Guarded grain cases | 1176 admitted; 1080 sounding; 192 separate unchanged index-capacity refusals |
| Strict audio matrix | 85 runs passed, four simultaneous tracks |
| Retained strict CUTOFF-zero unit suite | 2355 checks passed |
| Exact-base audio captures | 481: 99 byte-identical, 382 intentional differences |
| Designated unaffected forward/aligned controls | 64 byte-identical captures, 67108864 bytes |
| Retained UI state/pixel comparison | 270 snapshots, byte-identical |
| Determinism | 20 repeated four-track renders identical |

Every comparison capture explicitly verifies meaningful nonzero final output
with nonzero master gain. The matrix covers all four personalities, all five
factory recordings, POS 0/64/127, STRETCH 0/1/64/127, live POS 0→127→0,
SHP in both directions, same-zone source changes, fractional-rate user zones,
and FREEZE SIZE/HOLD extremes. Twenty four-track traversal captures run 640000
frames each (about 14.5 seconds), reaching the end and wrapping at least twice
for every factory source, including 275098-frame SYNTH.

Head/boundary fixtures cover lengths 65535, 65536, 65539, 275098, short legal
zones, pool/protocol bounds and UINT32_MAX head arithmetic. ADPCM source
surroundings are poisoned under ASan; forward/reverse cursor assertions verify
fitting rather than relying on decoder guards. All three voices and four part
pools remain within the existing engine memory reservation. Retargeting one
track also checks the other tracks' grain pools and RNG states remain unchanged.

Intentional audio differences are full long-source traversal, live positioning,
correct overflow fitting, fractional-rate fitting and the reverse one-frame
span correction. Mixed reverse captures were retained and classified rather
than forcing inappropriate byte identity. Forward controls isolate unaffected
behavior. Factory audio can consequently change throughout a render, rather
than only at the first previously truncated position. RNG/scaling, source audio,
filters and personalities are otherwise unchanged.

### Sanitizers and broader regressions

Strict numerical/audio/CUTOFF runs use ASan+UBSan with only **shift** checks
disabled for existing DSP idioms: `-fsanitize=address,undefined
-fno-sanitize=shift -fno-sanitize-recover=undefined`. Signed overflow, bounds,
object-size and pointer-overflow checks remain enabled in these runs.

Broader legacy host suites retain their existing exclusions for signed integer
overflow, shift, bounds, object-size and pointer-overflow; they are not claimed
as fully strict UBSan coverage. No sanitizer diagnostics occurred.

Passing broader results: GRAIN-only policy 380, editor/storage 70, persistence
24, four-part audio 330, theme 8229, grid visuals 1935, STRETCH descriptor 1764,
GRAIN grid 2329 and FUN11/FUN10 project-format 41146 checks. Full UI (850
assertions), editor (172), project (61), presets (45) and backup (103) complete
successfully. Eight retained CUTOFF audio renders and four STRETCH audio
renders pass. Diagnostic multi-engine tests use `FELUCCA_GRAIN_ONLY=0`; shipping
GRAIN-only policy is tested separately.

Known baseline failures are unchanged, including identical failing assertion
names: MIDI 134 passing / 15 failing; motion 97 passing / 27 failing. Broad web
has 282 passing / 5 failing on both base and fix. The focused GRAIN-only web
suite passes 20 tests. Factory source verification passes for all five original
WAVs and reproducible ADPCM, including rejection of altered/missing originals.
No existing fixture was weakened or skipped to hide these failures.

### Reproduction

Run `sh tests/run_grain_only.sh` for registered host suites. Set
`GRAIN_POSITION_BASE_TREE` to a clean checkout of the exact base to enable the
new comparison matrix, or run:

```sh
python3 tests/grain_position_compare.py --base-tree /path/to/fc0fc15 --out /tmp/grain-position-compare
cc -O1 -g -ffp-contract=off -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=undefined -Ibuild/gen -Ifirmware/src tests/grain_position_test.c -lm -o /tmp/grain-position-test
/tmp/grain-position-test
```

Normal generated tables and the pinned engine dependency must already exist.
Historical audio comparators remain unchanged: their older whole-render identity
claims are not suitable for intentional F2/F3/F6 factory-audio differences.

## Resource measurements and build limitations

GCC 14.2 native `-Os -ffp-contract=off` compilation of
`web/emu/felucca_web.c`, with identical tables and dependencies:

| Native object section | Base | Fix | Delta |
| --- | ---: | ---: | ---: |
| text | 878410 | 878738 | +328 bytes |
| data | 31060 | 31060 | 0 |
| BSS | 1603232 | 1603232 | 0 |

Native private GRAIN state is 7288→7296 bytes (+8 per part after padding).
A freestanding GCC 32-bit pointer-layout probe of the actual structs gives
7188→7200 (+12 per part); this is a layout estimate, not a pi32 target result.
The existing `ENG_MEM_PART` reservation remains 12888 bytes per part, 51552
across four tracks. Remaining private capacity is 5592 bytes/part natively,
5688 in that 32-bit layout. Compile-time `ENG_MEM_FITS` remains satisfied.
Native measured individual stack frames: spawn 88→104 bytes; render remains
328 bytes, bounded. These are not a target worst-case call-stack measurement.
No bitmap, framebuffer, dynamic allocation or static RAM increase.

Browser build attempt: `emu: no emcc (Emscripten)`.
Target build attempt: `build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)`.
No tools were installed. Native text growth is not an actual firmware flash
measurement. Hardware flash fit remains unverified until a target link succeeds.

## Remaining limitations

The unchanged 1075-entry seek index still rejects sufficiently long or aggregate
multi-zone sources (including the tested 275201-frame case). Fixing the head
does not resolve F7/index capacity. The 96 admitted but silent short-window
cases retain the known Hann-window limitation; no claim is made that every
legal short zone sounds. TONE, CLOUD DRIFT, DTUNE, STRETCH scaling/quantization,
FREEZE semantics and filter headroom remain outside scope. No source-index
capacity or sample metadata was changed.

## Changed files

- `firmware/src/eng_grain.c`: bounded heads, live deltas, safe span/fitting.
- `tests/grain_position_test.c`: numerical, boundaries, live/source and isolation checks.
- `tests/grain_position_audio_test.c`: real final-output rendering and traversal harness.
- `tests/grain_position_compare.py`: exact-base classification and unaffected identity.
- `tests/run_grain_only.sh`: register new tests and optional comparison.
- `web/emu/grain_stretch_audio_test.c`: test trace supports split/full-width heads.
- `docs/grain-position-fix.md`: this report.
