# GRAIN stereo transport foundation (Task 244B)

Base: `6905d018e0e469bc3226198c14c6f44d1c66127b`.
SPREAD is not activated. The mono engine, TONE, bipolar filter and its 256-frame
transition remain authoritative for every personality and every C value.

## Private contracts and ownership

`grain_render_transport(t,v,mono,n,m,group)` is the GRAIN-only adapter called by
`track_render`. It adds exactly the original mono result, calls the original
renderer once, and rejects n > CTL or group > 1 without mutation. Callers supply
at least n mono int32 frames; this is not interleaved stereo. The public
`engine_t.render` ABI and ENG_GRAIN.render remain unchanged.

Each GRAIN part owns `stereo_side[2][CTL]` inside its reserved engine region.
Group 0 represents voices routed to V-SPRD's left-voice group; group 1 represents
all remaining voices. These are future intrinsic-side lanes, not replacements
for SPREAD_BUF and not output L/R. No producer writes audio to either lane yet.
`track_grain_stereo(t,n)` returns a borrowed const view of these two lanes and n
valid frames only for a currently owned GRAIN region and n <= CTL; otherwise it
returns null lanes and zero frames. The view is intended for mix_part after
track_render, on that part's owning render core, and expires at the next render
or engine ownership change. Do not retain it across a switch or read it from the
other core concurrently. No global/shared scratch buffer was added.

Full-capacity clearing occurs before every GRAIN track render, even with no
active voices or a short/zero-length block. Source resets also clear both lanes;
engine-memory claiming clears their storage on ownership change. Views are
unavailable for non-GRAIN engines or unclaimed memory. Returning to GRAIN clears
at the next render even if an intervening memoryless engine preserved the region.

The mixer continues its exact original mono/V-SPRD paths, with no second mix
loop or side-filter call. The view establishes the handoff, not an active stereo
routing feature. Future nonzero lanes require a separate qualified consumer:
apply track PAN and V-SPRD group gains to intrinsic stereo without overwriting
the existing whole-voice side signal. Preserve mono DIST/INSERT and sends unless
a separately approved policy changes them; gate/mute side signals coherently.
No claim is made that nonzero side data could safely be enabled in this revision.

## Memory, work and host measurements

Two 32-frame int32 lanes occupy 256 additional bytes per part (1,024 bytes across
four parts), reused inside the existing 51,552-byte reservation. No newly
allocated BSS, heap, framebuffer, grain record or persistent storage is added.
Host gr_part_t: 7,320 -> 7,576 bytes. Target-layout assertion: 7,224 -> 7,480 bytes,
leaving 5,408 bytes of each 12,888-byte region. Existing DRIFT offsets are unchanged.
Grain record: host64/target56 unchanged. Track state unchanged.

Identical GCC -O2 host PCM-driver builds: text 549,053 -> 549,213 (+160),
data16,868 unchanged, BSS306,056 unchanged. These include host test machinery,
not FM1 link measurements. Stack-usage reports: grain_render304 and mix_block320
bytes unchanged; no new scratch audio array on the stack. Compiler inlines the
adapter. Added work is 64 zero stores per GRAIN part block plus engine dispatch
checks; no extra sample decode, RNG, grain scheduling, per-sample DSP or mixer
loop. At four parts and 44.1k/CTL32 this is 352,800 zero stores/second, including
silent parts. Source reset may clear again.

Five-run host CPU medians, CLOUD C100/SYNTH, identical sounding hashes:
1 track67.344 ->68.756 ns/frame (+2.1%); 4 tracks177.260 ->179.369 (+1.2%).
Host timing is noisy and is not an FM1 CPU budget measurement. No target build,
actual target flash fit, analog stereo measurement or CPU2 timing qualification.

## Validation and reproduction

New `grain_stereo_transport_test.c`: 9,506 strict ASan/UBSan ownership, bounds, full
clear, source reset, rejected adapter calls, unclaimed/non-GRAIN view rejection,
separate V-SPRD pointers and unchanged callback checks. Existing size assertions
were updated only for the intentional reserved-region occupancy increase.

`python3 tests/run_grain_stereo_transport.py --base-tree CLEAN_6905d01 --out /tmp/stereo`
compiles exact-base and current firmware with the same deterministic harness.
96 pairs (four personalities, six scenarios, C0/25/50/100) are PCM/state identical;
nine additional four-track PAN(-64/0/63) x V-SPRD(0/64/127) pairs are identical.
The current harness checks every intrinsic-side sample remains zero after every
block. Captures use nonzero master, meaningful output and identical state hashes.
The generated harness stays outside the repository; no frozen hashes changed.

Other completed gates:

- 24 frozen granular PCM/state cases and independent repeats pass.
- Short envelopes: 1,851,392 cases, 152,924,099 checks; known silence306 ->0.
- STUTTER: 42,069,041 checks, all16,384 rate/gate settings.
- Filter transitions: 3,553,312 strict checks; worst first transition edge0.
- SPARKLE1,411,308; CHAOS3,804,742; DRIFT2,488,821 strict checks.
- DRIFT lifecycle/ownership4,311,482 checks.
- WIN latching1,981,446 and device UI62,537 checks.
- Experimental fidelity12,379,206 and hybrid32,824,526 strict checks.
- Historical persistence/NOR5,804 checks; user presets pass.
- Physical preset-bank ownership26,299 checks, both copies and fault paths.
- Editor native2,909; WIN110; C2,259; project bridge2,189; browser89;
  WIN provenance11; C persistence109,038; downgrade oracle362 checks pass.

Strict numerical/storage tests enable signed-overflow and shift sanitizers.
Full-mixer PCM comparisons retain the existing historical signed-overflow/shift
exclusions; they are not evidence that all historical mixer arithmetic is clean.
No new failure occurred in these focused gates. Broad historical editor/motion/
backup/robustness failures were not rerun or claimed fixed in this task.

## Task 244C gates

Before audible stereo, define matching side TONE/SVF state with coefficient and
transition advancement once per voice; resolve nonlinear clipping/headroom of
M/S versus independent L/R filters. Define C0 return-tail semantics and pan-law
normalization. Prove simultaneous intrinsic stereo plus V-SPRD, PAN, gates,
mute, DIST/INSERT, sends, limiter/BASS+, engine fades and CPU2 ownership. Keep
zero-C legacy path exact; no extra grain reads, RNG or allocations. Profile and
link the actual compact FM1 target before claiming hardware readiness.

## Changed files

Production: firmware/src/eng_grain.c (private adapter, owned buffers/reset),
firmware/src/voice.c (GRAIN dispatch, clear and const handoff), firmware/src/fx.c
(handoff contract comment only; mixer arithmetic unchanged).
New tests: tests/grain_stereo_transport_test.c and tests/run_grain_stereo_transport.py.
Size-assertion-only updates: tests/grain_chaos_test.c,
tests/grain_drift_state_test.c, tests/grain_fidelity_kernel_test.c,
tests/grain_hybrid_kernel_test.c, tests/grain_short_envelope_test.c,
tests/grain_sparkle_test.c, tests/grain_stutter_gate_test.c,
tests/grain_win_i2_latch_test.c and tests/grain_win_i3a_ui_test.c.
This report is the only documentation addition.
