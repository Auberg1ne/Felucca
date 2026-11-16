# WIN I2: production grain-window morph DSP

Base: `68f3e7a848b2410670945c07b6a66f3689ee88e3` (I1).
Branch: `feat/coffee-grains-win-i2-dsp`.
Reference renderer: `4c34b6e38612ca13d71aee5a5cc3cd7d9da3b07d`.

## Result

Production GRAIN now reads WIN109 independently on all four tracks:

| WIN | New grains |
|---:|---|
|0|Endpoint-corrected Gaussian9|
|1..63|Linear-amplitude Gaussian→Hann, fraction WIN/64|
|64|Original Hann lookup directly, exact legacy PCM|
|65..126|Linear-amplitude Hann→Tukey, fraction (WIN−64)/63|
|127|Tukey alpha0.5|
|Invalid runtime value|Hann|

I2's required Hann identity, spawn latching, four-track independence and
persistence gates pass. Only `firmware/src/eng_grain.c` changes in production.
No UI/control, protocol, preset/serialization, sample, scheduler, modulation or
voice-allocation changes. I1's FUNC projects, user records6/7 and B1 protections
remain authoritative. Existing explicit non-Hann I1 values now intentionally
become audible; pre-WIN imported data still becomes Hann. Nothing is rewritten
automatically.

No new build flag is needed: this is the production path. Existing test-only
window/morph selectors retain their explicit experimental overrides when their
flags are enabled. Fidelity/adaptive/per-sample/hybrid flags and controls remain
separate, opt-in and default-off. I2 never promotes those renderers.

## Exact algorithm and preserved kernel

The existing 257-entry Q15 tables and `grain_morph.h` are reused byte-for-byte
from the validated experiment. No table is generated in the callback or copied
into RAM. `gr_morph_win` is source-identical to the experiment, including direct
anchor dispatch. Weighted interpolation uses exactly:

- WIN<64: `(a*(64-WIN)+b*WIN+32)/64`.
- WIN>64: n=WIN−64; `(a*(63-n)+b*n+31)/63`.

The maximum rounded numerator is2097120, within signed32. Coefficients total64
or63; the result is convex and bounded0..32767. Divisions are by constants,
not variables. There is no equal-energy compensation or new normalization.
Different windows have naturally different energy/loudness.

`gr_win`, the original Hann table and the complete `gr_run` function body remain
source-identical to I1. `GR_RENDER_WIN` now uses the shared evaluator (historical
helper name `gr_experiment_win` retained for existing probes). Tag192 calls the
original Hann lookup without blending or additional rounding. Low tags0/1/2
continue supporting the old explicit experiments. All production spawns use
128+WIN, including192 for Hann. Invalid low tags defensively use Hann.

The original two-endpoint ramp, at up to32 frames, is unchanged. The strict
kernel still observes **20480 single-block silent cases** when both sampled
endpoints are zero. I2 deliberately does not repair that approximation or claim
short-grain fidelity improvements.

## Control ownership, publication and latching

`voice.c::track_render` calls the engine's per-track block hook after
`fx.c::mix_part` has called `mod_begin`, before rendering any voices. The
existing `eng_grain.c::grain_block` now captures one aligned `int16_t` WIN into
an audio-owned byte, `gr_part_t.win_snapshot`, after source/index maintenance.
Out-of-range signed values and non-GRAIN requested engines resolve to64; the
latter avoids treating another engine's reserved0 as Gaussian during a
diagnostic engine transition.

Every successful `gr_spawn_one` takes **only that snapshot**, encodes128+WIN in
`gr_grain_t.window` and never changes it again. A second SHIMMER spawn sees the
same snapshot, even if a raw control write is injected between the block hook
and spawning. Existing grains read only their captured byte. No renderer reads
the current control value while evaluating an active envelope. Latency is the
next owning track/block boundary plus the next normal grain spawn.

The snapshot fits the one unused byte after `head_zl[3]`. The grain latch uses
its existing trailing byte of padding (already used by the experiment). There
is no global publication array, new per-grain allocation or record/track growth.
Production compile-time assertions guard both known32/64-bit ABI sizes.

Concurrency assumptions are the existing firmware ownership contract, not
`volatile` used as a lock:

- Hardware main-loop UI/editor writers cannot execute while the audio ISR is
  running. Ordinary scalar parameter accesses use the existing aligned16-bit
  storage; transaction-style project/preset loads use the existing IRQ guards.
- `main.c::fm1_timer5_irq` scans inputs while nested but defers USB/MIDI polling
  when `felucca_dbg.in_audio` is set. Its nested UAC service touches the audio
  ring, not WIN. Historical learn109 stays quarantined, no fixed CC or motion/
  matrix destination writes WIN.
- `fx.c::mix_block` processes events before splitting parts. CPU1 owns only its
  assigned tracks until the caller waits. `fm1_cpu1_run/wait` use existing
  `FM1_C1_SYNC` job-publication/completion barriers. Both the track snapshot and
  grain pool are written only by the owning part renderer. No new locks or
  blocking operations are added.
- The browser AudioWorklet runs message handlers and synchronous `web_render`
  on the same worklet owner. Browser main-thread messages do not directly write
  shared WASM parameters during rendering. No new shared-memory API is added.

ThreadSanitizer passes actual pthread-C1 rendering with live changes on all four
personalities. Forced job refusal and accepted-but-unexecuted timeout paths
also match one-core PCM. These do not simulate a partially executed hardware
job being replayed; existing C1 fault/replay semantics are unchanged. Real FM1
cache, interrupt and timeout qualification remains a target test gate.

## Validation and exact comparisons

GCC14 host, `-ffp-contract=off`; comparisons use the exact clean I1 and exact
validated experiment worktrees. Each audio state runs in a fresh process,
because UI reset alone does not reset every mixer static. Master is explicitly
4096. Every sounding capture checks nonzero output, peak/RMS, canaries,
per-channel audio activity and sample-backed forward/reverse observations.
Sine fallback correctly has no grains and is unaffected by WIN.

| Gate | Result |
|---|---:|
|Strict morph numerical tests|85344961 assertions passed|
|Strict actual grain kernel|3315208 assertions passed|
|Production block/spawn/active-grain latching|1981446 assertions passed|
|**Strict numerical/kernel/latch total**|**90641615**,0 failures, no sanitizer exclusions|
|I1 default Hann comparisons|84 states exact|
|Gaussian/Hann/Tukey vs experiment|252 anchor states exact|
|All128 WIN × four personalities vs experiment|512 sounding comparisons exact|
|Live/independent tracks vs experiment and pthread C1|56 cases,168 renders exact|
|Additional extreme-control comparisons|80 cases,160 renders exact|
|C1 refusal and unexecuted timeout fallback|28 live cases,84 renders exact|
|Audible project/preset save-load|36 renders; direct/project/user PCM exact|
|Live pthread-C1 ThreadSanitizer|4 personalities, no race reports|
|Retained UI/navigation stream|225 full240×240 frames and all captured state identical|

The main matrix comprises **1780 sanitized audio executions**:588 anchor/legacy
renders,1024 all-value renders,168 independent/live/C1 renders. Its820 production
measurements and SHA256 PCM hashes are in [audio.csv](data/grain-win-i2/audio.csv).
Additional stress/fallback, save-load and TSan tests bring structured new audio
executions to2064, excluding the existing default-audio regression runner.
No unrelated golden or snapshot was regenerated.

Source matrix: CELLO0, FLUTE2, GUITAR3, SYNTH13, VOX14, a synthetic legal user
zone and sine fallback, all four personalities and SIZE/personality-A settings
0/0,80/127,127/127. Thus FREEZE HOLD extremes are included. Every WIN value is
swept on broadband GUITAR in all personalities; the seven-source matrix covers
the exact three anchors, not a claim of a complete128×7 Cartesian sweep.

Strict kernels cover every value, signed source extremes, both directions,
Q16 rates256/32768/65536/131072, durations16..86432, full long SYNTH lifetimes,
reverse-cache refill boundaries, phaseUINT32_MAX, table bounds and canaries.
Numerical tests exhaust effective anchor phase classes and wide-integer blend
references. Latching tests cover all tracks/personalities, full signed16 control
range, invalid snapshot bytes, both SHIMMER grains and complete active-grain PCM,
decoder/cache state equality against frozen clones while controls change.

Extra stress uses actual PTCH−24/+24, RAND0/127, DENS0/127, min/max SIZE and HOLD,
SYNTH/user zones, five anchor/intermediate values and every personality. An initial
reference-fixture failure checked only the final channel meter during low-density
silence between grains. The fixture now requires positive per-channel activity
somewhere in the capture and retains its final-meter assertion for normal-density
runs. Nonzero final audio assertions were retained; no production change was
made to address that fixture issue.

The36 save-load renders use the real physical NOR fixture, all four sounding
tracks, WIN0/64/127 and all personalities. Direct state, FUNC project reload and
native user-preset reload produce identical meaningful PCM, including explicit
Gaussian0. Legacy-to-Hann interpretation is separately covered by the frozen
I1 persistence fixtures. Factory/init/CLEAN/undo, all previous formats, retained/
restore-last, malformed/future objects and B1 raw-NOR ownership gates pass again.

### Persistence, editor and inherited regressions

Rerunning `tests/run_grain_win_i1r.sh` on I2:

- Strict physical persistence:5818 checks in each shipping/diagnostic build.
- Exact B1 downgrade oracle:1654 checks, no destructive writes.
- Editor canonical GET/SET/DESC and learn quarantine:110 checks.
- User-record parser/storage:45 passing assertions,0 failures.
- Browser provenance/headless/bulk-import checks:11 passed.
- Existing default final-mixer regression:330 checks per revision;
  **57344000 PCM bytes identical** to B1 (and the exact-I1 matrix above).

FUNC/6/7 versions, PER4 raw learn bytes, legacy unmarked-array Hann policy and
ambiguous replacement refusal are unchanged. Historical learn109 remains inert
through settings NOR and backup restore. No unversioned bundled browser path
begins sending Gaussian0; no new UI control or capability tag is introduced.
I1's documented third-party scalar-SET/library provenance limitations remain.
B1 remains the minimum supported shipping downgrade checkpoint; arbitrary
unpatched older firmware is not made safe by I2.

Fresh exact-I1/current MIDI suites both report122 passing/27 failing assertions;
motion both94/30, with identical failure lists. These are inherited GRAIN-only/
legacy-fixture failures, not new I2 failures. Broader UI/editor baseline crashes
are documented in the I1 report; those crash-prone full suites were not rerun
or claimed fixed here. The strict editor gate and complete225-frame retained
navigation stream were rerun.

Whole-mixer/reference and stress tests retain the pre-existing integration
exclusions for signed-integer-overflow, shift, bounds, object-size and
pointer-overflow. Save-load audio needs only the common-DSP signed-overflow/shift
exclusions. **No exclusion applies to the90641615 strict new-path assertions or
the strict persistence/editor gates.** ThreadSanitizer is separate from ASan;
its four actual threaded live renders completed without reports.

## Resources and workload

Same existing `grain_only_audio_test.c` harness, identical flags
`cc -Os -ffp-contract=off -fstack-usage` and only the exact source-tree override:

|Native ELF measurement|I1|I2|Delta|
|---|---:|---:|---:|
|size text (read-only allocated sections)|565435|567036|+1601 bytes|
|`.text` instructions|115730|116147|+417|
|`.rodata`|408432|409520|+1088|
|data|20492|20492|0|
|BSS|310600|310600|0|

The two newly linked constant windows contain1028 bytes of Q15 entries; section
alignment accounts for the larger rodata delta. Remaining read-only total
includes host unwind metadata. These are host measurements, not FM1 flash size.
The fair comparison has no production-only test assertions compiled into one
side. Earlier instrumented-fixture sizes are not substituted for these totals.

Host grain64, part7296 and common track1840 bytes are unchanged. Known32-bit ABI
assertions require grain56/part7200; actual pi32v2 compilation is still pending.
The engine reservation remains12888 bytes/part, with5592 host bytes free as
before. The latch/snapshot give meaning to52 previously unused bytes across
four pools but add no allocation. No audio buffer, framebuffer, bitmap, heap or
runtime RAM window table is added.

Host stack frames in the same harness: `grain_render`328→328,
`gr_spawn_one`112→112, `grain_block`64→72 bytes. `gr_run` is inlined there; its
body is unchanged. Another harness produced40→40 for the block hook, so do not
confuse compiler choices with a target stack guarantee. Full target call-chain/
interrupt/CPU1 stacks remain unmeasured.

### Work and host benchmarks

At48 occupied slots in a nonterminal32-frame block:

- 96 envelope evaluations, unchanged from the endpoint kernel.
- Anchor:96 table interpolations; intermediate morph:192 plus96 constant blends.
- Thus at most96 additional table interpolations and96 blends versus Hann for
  that steady block, plus four snapshot loads/sanitizations and spawn-byte stores.
- Source decoding, admission caps, grain count, scheduling and RNG work are unchanged.

[Kernel I1](data/grain-win-i2/kernel-i1.csv) /
[kernel I2](data/grain-win-i2/kernel-i2.csv):80 timing rows per revision, five
repeats, forward/reverse and Gaussian/Hann/Tukey/morph96. Capacity proxies use
48 slow grains or decode-budget-constrained16 forward/8 reverse grains at2×.
These manually initialized pools are a capacity bound, not a scheduler claim.

[Full mixer I1](data/grain-win-i2/mixer-i1.csv) /
[I2](data/grain-win-i2/mixer-i2.csv):120 rows each, fresh child process per state,
all personalities, five WIN values, slow/fast profiles and three repeats. All
captures sound. Maximum actual occupancy is48 slow,12 fast CLOUD/STUTTER and16
fast FREEZE/SHIMMER. Normalized grain/head/decoder/RNG hashes and occupancy match
I1 exactly in all120 rows after excluding only window/pointer representation.

Representative median host CPU ns/output frame (three repeats, effects off):

|Personality/profile|I1 Hann|I2 Hann|I2 morph96|max grains|
|---|---:|---:|---:|---:|
|CLOUD slow|238.18|241.79|262.21|48|
|FREEZE slow|282.96|252.77|289.48|48|
|STUTTER slow|264.85|237.46|339.20|48|
|SHIMMER slow|276.80|270.59|298.45|48|
|SHIMMER fast/reverse pressure|335.70|322.24|532.16|16|

Host rankings and outliers are noisy; an initial benchmark concurrent with
compilation was repeated serially before retaining these CSVs. Even the quieter
host results are not FM1 cycle counts or deadline evidence. The unchanged
scheduler cost estimate and real four-track/effects/CPU2 timing require target
profiling; this task does not alter admission to mask added work.

The unchanged target build attempt failed before compilation/linking:

```
build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)
```

No toolchain was downloaded/installed. **Fit within581564 bytes and actual target
RAM/stack/deadline margins remain unverified.** No deployment or flashing.

## Reproduction, files and next stage

With existing generated headers and pinned engine sources:

```sh
WIN_I1_TREE=/absolute/clean/68f3e7a-worktree \
WIN_MORPH_TREE=/absolute/clean/4c34b6e-worktree \
WIN_B1_TREE=/absolute/clean/6055566-worktree \
WIN_I2_TSAN=1 WIN_I2_OUT=/tmp/grain-win-i2 \
  tests/run_grain_win_i2.sh
```

`grain_win_i2_compare.py` checks exact reference commits, unchanged Hann/kernel/
validated helper bodies and tables, then renders comparisons. `grain_win_i2_stress.py`
adds pitch/density/HOLD stress, C1 failure paths and full UI-state captures.
The roundtrip fixture tests physical project/user saves before auditioning.
No audio/golden files need to be regenerated in Git.

Reproduce native sizes with the existing `grain_only_audio_test.c`, overriding
`GRAIN_ONLY_UI_SOURCE` to each exact tree and using the same flags above.
`grain_win_i2_bench.c` and `grain_win_i2_mix_bench.c` accept
`WIN_I2_UI_SOURCE` for base sources; compile `-O2 -ffp-contract=off` and run each
serially. The kernel probe additionally needs `WIN_I2_PRODUCTION=1` only on I2
to initialize test grains' latch bytes. This is a test macro, not a production
feature switch. Its first output line contains sizes before the CSV header.

Changed files: one production engine file; five new C audio/latch/roundtrip/
benchmark fixtures, two comparison/stress Python runners, one shell runner,
this report and five measurement CSVs. Existing I1 production/storage/browser/
MIDI files and frozen fixtures are untouched.

I2 is ready as the reviewed DSP baseline for I3 work. I3 must separately approve
device/browser WIN controls, versioned library/protocol provenance and any
persistent-learn/motion eligibility changes. Keep hybrid/fidelity default-off
until actual target profiling. No merge, deployment or flashing was performed.
