# FROZEN DRIFT state/lifecycle foundation — Task243A

Base `79fd756709ff37fb7e82bbd8c814110999213dec`, branch
`feat/coffee-grains-granular-quality`. No audible DRIFT, SPREAD, format, protocol,
MIDI, UI or asset change. This implements the approved decision: future DRIFT
follows the existing scanning head; there is no new captured freeze anchor.

## Ownership and exact layout

`gr_part_t` now ends with `gr_drift_t drift[GR_POLY]`, three records/track:

| Record field | Offset | Representation |
|---|---:|---|
| phase |0 |uint32 Q0.32 turns, modulo2^32 |
| displacement |4 |int32 signed Q24 fraction of current zone length |

Each record is exactly8 bytes. Displacement is zero after all initialization/reset
paths and **never calculated, applied or read by spawning/rendering in243A**.
`GR_DRIFT_LIMIT=1<<21` documents the proposed future±1/8-zone bound. It is not an
active depth mapping or waveform. Tests deliberately poison future displacement
and prove it cannot affect the existing grain prefix or RNG/decoder behavior.

| Layout | Task242 | Task243A | New array offset | Free bytes/region |
|---|---:|---:|---:|---:|
|64-bit host gr_part_t |7296 |7320 |7296 |5568 |
|32-bit layout probe |7200 |7224 |7200 |5664 |

Host record offsets are7296,7304,7312; 32-bit offsets7200,7208,7216. All previous
member offsets remain unchanged. Grain records stay64 host/56 target bytes;
track_t stays1840 on the host. The32-bit result was checked by compiling the actual
structure declarations with a32-bit layout probe; this is not a JieLi target link.
Production static assertions enforce both expected sizes and ENG_MEM_FITS.

Every track already owns a12888-byte,8-byte-aligned engine region (`eng_mem` in
engines.c). The four-region reservation stays **51552 bytes**. Occupied GRAIN
runtime state increases24 bytes/track,96 total, **without allocating more memory**.
No heap, separate static state array, audio buffer or framebuffer is added.
`ENG_GRAIN.mem` derives from sizeof(gr_part_t), so `eng_mem_claim` clears the new
extent when ownership changes. The same-owner fast path retains it. Diagnostic
PHYS→GRAIN→PHYS tests prove clearing and cross-part boundaries, rather than assuming
that unused bytes may alias another engine. No voice ph/s slot, LFO or RNG is used.

State follows the existing serialized note/block/render ownership of each part:
its owning audio job reads/writes it. Control/editor code cannot access it. The
existing IRQ/C1 job barriers publish parameters, as for the source head and WIN;
no new locks, global clock, timer or concurrent writer is introduced.

## Timebase and lifecycle contract

For an eligible FROZEN sample voice, once per render fragment:

```
phase += 4869u * processed_output_frames  // unsigned modulo2^32
```

One turn is about20.002371876 seconds at44100Hz. This is independent of pitch,
STRETCH, HOLD, tempo, LFO, RNG and grain admission. The multiplication is one
block operation, not a per-sample loop. Actual processed n is bounded to CTL32.
All positive C values use the same clock; depth is future work. C0 pauses the
phase, rather than resetting it. Displacement stays zero at every C in243A.

| Event | State behavior |
|---|---|
| Engine claim/power-on |New GRAIN extent zeroed by existing ownership mechanism |
| SRC change or upload-generation/stamp change |gr_reset resets all three records |
| Fresh voice (`env==0`) |Note-on resets that voice |
| Sounding retrigger/recapture, same zone |Retains phase and displacement; legacy head recapture still occurs |
| Note reassignment, same zone and still sounding |Retains clock, matching the sounding-retrigger policy |
| Note reassignment to another zone |Resets that voice before its new grains |
| Voice becomes inactive |Clears state at next grain_block |
| Invalid zone or sine fallback |Clears state at block boundary; no clock advance |
| Leave FROZEN personality |Clears state at next block; no stale DRIFT in other personalities |
| Select another track in UI |No reset or migration; active track's own job continues |
| Transport stop/start alone |No explicit oscillator reset; actual note/release lifecycle governs it |
| C0→positive |Resumes retained phase, or starts from zero for a new voice |
| Positive C changes |Same rate, continuous phase |
| Positive→C0 |Pauses phase; zero displacement in243A |
| Phase wrap |Defined uint32 wrap, no lifecycle reset |

The lifecycle loop skips already-zero records to avoid unnecessary inactive-state
writes. Same-zone sounding recapture deliberately preserves oscillator state even
though legacy grain_note_on re-seeds the scanning head. This is **relative-offset
continuity**, not a promise that an intentionally recaptured or edited scanning
head never changes absolute sample position. Existing FROZEN HOLD still extends
grain duration; MOTION still randomizes position. Their behavior is unchanged.

Forward/reverse grains will eventually use the same zone-relative displacement
around that existing head, then existing span fitting/clamps. Reverse decoding
and scanning-head wrap do not own/reset this phase. No such offset is applied now.

## Strict qualification

Reproduce with clean pinned reference trees and existing generated headers and
engine submodules; all outputs remain outside Git:

```sh
python3 tests/run_grain_drift_state.py --base-tree /tmp/coffee-drift-79f \
  --out /tmp/grain-drift-foundation/final
python3 tests/run_grain_sparkle.py --base-tree /tmp/coffee-sparkle-745 \
  --out /tmp/grain-drift-foundation/sparkle
python3 tests/run_grain_chaos.py --base-tree /tmp/coffee-chaos-93f \
  --out /tmp/grain-drift-foundation/chaos
python3 tests/run_granular_quality.py --sanitize \
  --oracle-tree /tmp/coffee-grc-fd7 --out /tmp/grain-drift-foundation/regression
```

The foundation runner compares actual Task242 DSP against current DSP using the
same harness. New fields have no Task242 counterpart: all **legacy state fields**
and PCM must match, while new clock state is qualified independently.

- **4311478 shipping lifecycle checks**, **4392314 diagnostic checks** pass strict
  ASan/UBSan. These cover1..32-frame clock steps against a wide reference, repeated
  wraps, fresh/sounding notes, real user-zone reassignment, inactive/fallback/type/
  source/stamp resets, actual seq_start/seq_stop, track/voice isolation, future-state
  poisoning, pool boundaries and different-engine ownership. No sanitizer exclusion.
- **156 exact PCM/legacy-state pairs** against Task242:24 scenarios ×C0/25/50/100
  (96 pairs), plus five-source FROZEN/SPARKLE/CHAOS static and live reverse cases
  (60). Thus24/24 baseline C0 captures remain exact, and C>0 remains equally exact.
- SPARKLE kernel **1411308** and CHAOS kernel **3804742** checks pass; their complete
  five-source audio/regression runners pass. C values25/50/100 retain prior output.
- Frozen24 PCM/state fixtures pass unchanged. Short-envelope gate:1851392 cases /
  **152924099** checks,306→0 known silent cases. STUTTER gate **42069041** checks.
  Strict filter transitions **3553312** checks, worst first edge0.
- Size assertions updated to the explicitly measured+24-byte occupied state, not
  weakened to a loose reservation check. Modified experimental assertions were
  exercised too: fidelity **12379206** checks/493346 cases; hybrid **32824526** checks/
  985800 cases; WIN latching **1981446**; WIN UI **62537**. They pass strict sanitizers.
- Existing editor/persistence runner passes: stable2909, WIN110, C2259, project
  bridge2189, persistence109038, old-reader downgrade362, browser compatibility89
  and WIN provenance11. Frozen persistence/NOR5804 and user presets pass.

Full mixer capture harnesses retain their existing signed-overflow/shift exclusions
for unrelated legacy arithmetic. New state, strict kernels and storage probes do
not disable signed-overflow checks. No golden, audio asset or binary fixture is
regenerated. Historical broad editor/motion/backup/robustness failures remain outside
this task; those failing broad suites are not claimed green or rerun here.

## Resource measurements

Same current benchmark source, exact Task242 header/DSP versus current, host
`cc -Os -ffp-contract=off -fstack-usage`:

- Text **+189 bytes**; rodata/data/BSS **0 delta**.
- Existing engine allocation **0 delta**, occupied state **+24/track**.
- gr_spawn_one stack112, grain_block40, grain_render272 bytes: unchanged.
  grain_note_on40→48 bytes (**+8 host bytes**); target stack not measured.
- Bounded lifecycle checks for3 slots/track/block; one integer multiply/add/store
  per eligible rendered voice fragment at positive C. No added source decoding,
  grain scheduling, RNG calls or per-sample DSP work. New owner claim clears24 more
  bytes/track; same-owner claim does not clear it.

Five-run interleaved CPU-time medians,32768 high-density FROZEN blocks, SYNTH,
no file I/O (all output hashes exact across versions and C values):

| Tracks/C | Task242 ns/frame | Foundation |
|---|---:|---:|
|1/C0 |138.205 |136.997 |
|1/C100 |137.389 |138.113 |
|4/C0 |471.512 |474.866 |
|4/C100 |469.808 |467.471 |

Variation is under about0.9% in this host sample; it is not proof of a speedup or
FM1 CPU timing. Compile tests/grain_sparkle_bench.c with the above flags and run
`benchmark C tracks 1`; base adds
`-DSPARKLE_UI_SOURCE="/tmp/coffee-drift-79f/tests/ui_test.c"`. Compare size -A and .su.
Measurements and the32-bit layout probe are in /tmp/grain-drift-foundation/resources.
No installed default JieLi compiler was available; no installation/download or
hardware action occurred. Full target flash/RAM/POOL fit and stack remain unverified.

## Mandatory Task243B gates

1. Keep the clock/ownership/lifecycle contract; no ph/LFO/RNG reuse. Keep C0 legacy
   fields/PCM exact when never activated, and retain SPARKLE/CHAOS exact comparisons.
2. Define and test a bounded continuous periodic waveform and C→depth mapping.
   Map signed Q24 to current zone length with safe wide/split arithmetic; clamp
   before absolute-value conversion, including invalid/INT32_MIN state tests.
3. Slew **displacement**, including positive→C0 tail, toward zero without snapping.
   Phase pauses at C0; re-enable may resume at any phase, so direct target assignment
   is forbidden. Return to exact legacy positions once the tail reaches zero.
4. Apply only at new grain creation. Never mutate existing grain decoder positions,
   pitch, envelopes or head scanning. Use the established forward/reverse span bounds;
   do not invent a latched anchor. Handle head wrap without resetting the oscillator.
5. Preserve same-zone held recapture state. Treat source/zone changes as new coordinate
   systems starting neutral; never carry raw sample-frame offsets between them.
6. Replace the foundation's unused-displacement guards with explicit bounded-placement/
   slew tests, retaining ownership/canary tests. Test C0→100→0, C100→0, phase wrap,
   very short zones, sample reload, transport/reassignment and four tracks.
7. Re-measure CPU/stack and attempt target qualification only with available tooling.
   Do not claim smooth audible DRIFT based on this silent foundation alone.

Changed files: firmware/src/eng_grain.c; tests/grain_drift_state_test.c,
tests/run_grain_drift_state.py; exact-size assertions in grain_chaos_test.c,
grain_sparkle_test.c,grain_short_envelope_test.c,grain_stutter_gate_test.c,
grain_fidelity_kernel_test.c,grain_hybrid_kernel_test.c,grain_win_i2_latch_test.c,
grain_win_i3a_ui_test.c; this report. No architectural blocker remains for the
state foundation; audible modulation is intentionally deferred.
