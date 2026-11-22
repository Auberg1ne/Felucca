# Isolated per-grain stereo prototype — Task 244C

Base: e43141e44f9985e4c27bcb09ce47bdfed4193ea1.
No shipping stereo activation, C mapping, filter/effects integration, UI, protocol
or persistence changes. The existing engine callback ABI is untouched.

## Isolation and transport

`FELUCCA_GRAIN_STEREO_PROTOTYPE` defaults to zero and is not enabled by a build
profile. Normal `grain_render_transport` still invokes the original mono
renderer, even in a prototype build. Only an explicit test call to
`grain_stereo_probe_transport(t,owner,mid2,n,group)` renders isolated dry grains
into mid2 and that part's existing intrinsic-side lane. It rejects invalid
lengths/groups/owners and non-GRAIN/unclaimed regions without writes. This entry
is not called by track_render, mixer, audio ISR or engine callback. Its caller
must own the part's render core and provide/clear n <= CTL mid frames, clear both
side lanes before the block, and call once for each relevant voice owner. A
transport view remains borrowed until the next part render or ownership change.
The existing V-SPRD buffers are never reused or overwritten.

The kernel calls the unchanged gr_run once into 32 int32 scratch frames, then
weights each decoded/windowed sample. There are no extra source reads, grain
allocations, scheduler calls or RNG draws. The short-envelope correction and
reverse cache traversal remain inside the unchanged decoder kernel.

## Placement and pairing

Prototype-only `gr_test_spread[part]` is an explicit test selector 0..100, not
production parameter118. Zero/invalid widths and non-CLOUD admissions latch
center and clear the pending pair; all other personalities remain unmodified.

`int8_t pan[12]` is indexed by grain slot, range -64..64. First successfully
admitted CLOUD grain selects magnitude
`((((r2 >> 16) & 63) + 1) * width) / 100`, sign from r2 bit31. r2 already exists;
CLOUD position consumes its low16 bits. Next admitted CLOUD grain captures the
exact opposite pan, including zero. An explicit pending flag distinguishes a
centered pair from no pending pair. Pairing is per part across voice owners.

Placement commits only after pool, length, index and decode-load admission
checks. Rejection retains pending placement and all existing grain data; the
legacy RNG draws still occur exactly where they always did. Reused slots always
replace pan, including center. Grains retain their pan until retired. Source/
engine reset clears the array and pending state. Positive test-width changes
finish an outstanding pair at the previous magnitude; width0 closes it and
centers subsequent admissions without rewriting active grains. These are test
policies, not a production live-SPREAD transition design.

Opposite coefficients prevent systematic coefficient bias; different source
positions, amplitudes, durations and voice envelopes mean instantaneous or
per-pair energy equality is not guaranteed. An unmatched last grain may lean to
one side. No extra draw compensates for that.

## Exact integer channel reconstruction

For decoded/windowed x, pan p and a = trunc_toward_zero(x*abs(p)/64):

- L = x - (p > 0 ? a : 0)
- R = x - (p < 0 ? a : 0)
- M2 = L + R, S2 = L - R
- L = (M2 + S2)/2, R = (M2 - S2)/2

M2/S2 are doubled components, explicitly not the existing mono accumulator plus
an interchangeable V-SPRD side. Both reconstruction sums are even, including
negative samples. No channel contribution exceeds the legacy sample magnitude;
center has exact unity without Q15 multiplication. Fold-down is bounded per
grain; it is not promised equal to an already-cancelled multi-grain mono sum.
Full 12-grain summed doubled components fit int32 comfortably: windowed x is
bounded by the int16 decoded source and Q15 gain. The new product is at most
32768*64, well inside signed32. No clipping or gain boost is added.

The real shipping C0 route is untouched, rather than reconstructed through M/S.
No filter, amplitude envelope, PAN, gate, limiter or effects state is advanced
by this isolated dry entry. Therefore this experiment is not a finished stereo
voice or mixer path.

## Memory and measured host cost

Production/default: text/data/BSS unchanged. Identical -O2 host PCM builds before
and after: text549213, data16868, BSS306056 bytes. Grain record remains host64 /
32-bit layout56, part7576/7480. Existing 51552-byte reservation unchanged.

Prototype-only part state adds exactly16 bytes: pan12, pending int8, pending
flag1, explicit reserved2. Part becomes host7592 / 32-bit layout7496, leaving5392
bytes per12888-byte reserved region. Four parts occupy64 more already-reserved
bytes. Test selector logically adds4 static bytes when retained; optional read
accounting adds two uint32 counters only in the strict harness. No state is
borrowed from host grain padding. Static layout assertions guard both pointer
sizes; a freestanding `cc -m32 -fsyntax-only` probe compiles the actual structure
declarations and verifies56/7496 and reservation bounds. This is host layout
evidence, not a JieLi target build.

Dedicated -O2 microbench with/without prototype: text543151 ->543695 (+544),
data16540 and BSS313672 unchanged in that linked harness (alignment/dead-code
elimination can hide logical state growth). This measures the isolated helper,
not a target firmware image or a full production stereo implementation.

Scratch is128 bytes per kernel invocation. Production grain_render304-byte and
mix_block320-byte host stack frames remain unchanged. The optimized microbench
main frame grows256 ->512 bytes with inlining; an additional -fno-inline probe
reports a176-byte stereo-wrapper frame, plus its called gr_run56 and kernel120
frames. These are host compiler results, not FM1 stack limits.

Five-run medians, 12 constant-source long Hann grains (no decode work), excluding
accounting: mono1.386 vs stereo2.215 ns/grain/frame (~60% kernel-only increase).
This intentionally isolates weighting/scratch overhead; it is not a full callback
CPU percentage. At most one extra fixed-point weighting multiplication, signed
attenuation and two component accumulations per grain/frame, plus bounded scratch
clear/write/read traffic. No added shipping callback work. The prototype is not
claimed optimal or ready for target real-time use.

## Validation

Reproduce the strict probe and three-way actual PCM/state comparison:

```
python3 tests/run_grain_stereo_prototype.py --base-tree CLEAN_e43141e --out /tmp/stereo244c
```

The runner builds base, current-default and prototype-enabled normal playback;
the last enables test width100 on all tracks, without calling the dry probe.
All 105 cases match PCM and legacy state across all three builds: four
personalities x six scenarios x C0/25/50/100, plus nine PAN/V-SPRD combinations.
Every intrinsic-side sample remains zero after each normal playback block.
Nonzero master/output assertions and frozen RNG/scheduler/admission/load hashes
are retained. The full-mixer harness retains its documented historical signed-
overflow/shift sanitizer exclusions; this is not a claim of globally clean old
mixer arithmetic.

The isolated probe uses strict ASan/UBSan without those exclusions:

- 77,032,084 checks;30,720 admitted/7,680 rejected attempts.
- 1,204,900 matching kernel decoder calls; no logical source-bound violation.
- All pre-existing part/grain/decoder/cache/head fields match mono, including
  admission read counts and exact RNG updates with width active versus zero.
- Four independent parts, all12 slots, three owners/two lanes, forward/reverse,
  five factory sources, legal32/257/65535-frame user-zone fixtures, short grains
  16..128, three window anchors, signed bounds, slot reuse and rejected writes.
- 8,350,862 separated frames among13,802,940 probe frames.
- Aggregate L/R/difference RMS:3045.104 /3037.495 /1985.953. Hard-pan opposite
  channel suppression is exact100%; center reconstruction is exact. These are
  mixed numerical fixtures, not subjective listening quality judgments.

Other rerun gates: transport9506; filter3553312; SPARKLE1411308; CHAOS3804742;
DRIFT2488821; short-envelope152924099 checks/1851392 cases (silence306 ->0);
STUTTER42069041/all16384 settings;24 frozen PCM/state cases and repeats;
historical persistence/NOR5804; physical bank ownership26299; user presets;
editor2909, WIN110, C2259, project bridge2189, browser89, WIN provenance11,
C persistence109038 and downgrade oracle362. No new failure in these gates.
Broad historical editor/motion/backup/robustness failures were not rerun or
claimed fixed. No unrelated goldens changed.

## Remaining gates

Production SPREAD still needs matching side TONE/SVF ownership and once-only
coefficient/fade advancement; an approved nonlinear headroom policy; coherent
amplitude/mute/slicer behavior; PAN/V-SPRD reconstruction and mono effect-send
policy; live-C/tail handling; CPU2 concurrency qualification and actual compact
FM1 timing/link measurements. M2/S2 must never simply be fed into the existing
mono mixer. No target toolchain was installed; target RAM/flash fit remains
unverified. No production C mapping, UI, protocol, persistence, push, merge,
release or flashing is part of this task.

Changed files: firmware/src/eng_grain.c; tests/grain_stereo_prototype_test.c;
tests/grain_stereo_prototype_bench.c; tests/run_grain_stereo_prototype.py; this report.
