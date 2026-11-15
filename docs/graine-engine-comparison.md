# GRAINE versus COFFEE GRAINS: granular architecture audit

## Scope and evidence

Documentation only. COFFEE GRAINS base: `feat/coffee-grains-seq-theme`, **d9e352f9380da85dedd0b37434a513fed5b6b636**. Audit branch: `audit/coffee-grains-graine-comparison`. No implementation recommendations in this report have been applied.

Reference: the supplied **Granular-FM1-main.zip**, 1,798,722 bytes, SHA-256 **76b5f034393832b06dd9e6fab8cc279cc858195c4eedbc44a7b72d4ba0b970c1**. Extracted outside the repository to `/tmp/graine-reference/Granular-FM1-main`. This archive has no Git metadata identifying an exact upstream commit. Findings describe these supplied bytes, not a moving upstream branch. Reference documents were treated as evidence, not instructions.

Below, **C:** paths refer to this COFFEE GRAINS revision; **G:** paths refer to the supplied GRAINE tree. Line numbers are anchors, with function names authoritative. Source-derived calculations and proposals are separated from executed host tests. Neither instrument was measured on hardware during this audit.

## Architecture side by side

| Area | COFFEE GRAINS | GRAINE |
|---|---|---|
| Musical organization | Four independent tracks, personalities and parameter sets; common allocator shares an eight-sounding-voice budget; GRAIN permits three voices per track | One patch, up to six voices; not four independently sequenced tracks |
| Grain storage | `gr_part_t.g[12]` per track: 48 slots reserved overall | `gr[128]`, one shared pool across six voices; admission cap 64 on one core, 128 on two |
| Allocation | First free slot, per-voice share and per-track decode-work admission | First free slot below `maxg`; drop spawn if none; saturating drop counter |
| Stealing | Grain admission refuses excess work; inactive owners cleaned by block hook; existing common voice allocator still steals voices | No grain stealing in `grain_spawn`; `note_on` reuses same key, free voice, quietest released voice, then oldest voice |
| Lifetime | Output-sample countdown, Q32 window phase; zone-fitted span; existing owner cleared at end | Q32 window-phase overflow; grain owns voice index/source pointers and scheduled delay; current voice envelope/filter applied after summing |
| Source | Original factory/user IMA ADPCM; no full PCM copy, seek index and reverse decode caches | Byte mu-law or RAM int16 source; direct random access and 256-float decode table |
| Sample interpolation | Linear between decoded int samples, Q16 rate/fraction | Linear float samples with integer index + Q32 fraction; mu-law is nonlinear storage, interpolation occurs after decoding |
| Window | Fixed 257-entry int16 Hann; lookup/interpolation at block endpoints, amplitude ramp inside block | Four 513-entry float base windows plus one morphed/skewed table; table interpolation every output sample |
| Scheduling | Per-voice countdown; at most one scheduling opportunity per render block; SHIMMER can request a pair | 64-frame control chunks; periodic, bounded exponential-gap random, or clock-division scheduling with sample offsets and catch-up loops |
| Numeric strategy | Mostly fixed point; wide intermediates for source spans, detune and resonant output safety | Float control/mix/filter; integer source index and Q32 fractional/window phase; fastmath functions |
| Scan | Bounded uint32 sample + uint16 fraction + admitted target; delta-retargeted live POS/SHP | Normalized scan and slewed position, forward/back, once, drunk and jump modes; optional shared scan |
| Reverse | Already supported: RAND also sets reversal probability up to about 50%; ADPCM cache cost counted twice | Independent reverse probability 0–100%, reverse pad and CHAOS; signed playback rate |
| Stereo | Grains accumulate mono per voice; common voice/track pan and V-SPRD, mixer and sends | Equal-power per-grain L/R gains plus voice detune pan offsets; stereo per-voice filter/buffers |
| Overload | 2× source-rate cap, 8 decode-equivalent source samples/output sample/track, voice sharing and fixed slots | Admission ceiling reduced by four down to 16; slowly restored; second-core failure falls back to 64 admission slots |
| Parallelism | Whole independent tracks assigned between cores, integer sums combined, shared events before split | Even/odd grains split between cores, separate per-voice float buffers combined before voice processing |

Source anchors: C: `firmware/src/eng_grain.c:41–128` (limits, structures), `:315` `gr_spawn_one`, `:397` `gr_run`, `:506` `grain_block`, `:532` `grain_render`; `core.h:11`, `voice.c:345`, `fx.c:988–1060`. G: `app/graine.h:11–20`, `app/dsp/engine.c:153–201`, `:334–355`, `:408` `note_on`, `:779` `grain_spawn`, `:887` `grain_run`, `:1000` `voice_chunk`, `:1650` `eng_render`.

### What the pools actually guarantee

COFFEE GRAINS' 48 slots do **not** mean 48 maximum-rate reverse grains are affordable. At rate 2, one reverse grain counts as four units of the track's eight-unit budget; only two such grains can coexist on that track. At rate 1, eight forward or four reverse grains fit the decode admission budget, even though twelve slots exist. Per-voice quotas are additional gates, not reserved guaranteed occupancy; SHIMMER's extra spawn independently competes for space/work. Allocation failures can therefore change texture before memory is exhausted.

GRAINE scans first-free slots, not a free-list. Spawns are ordered by voice iteration, so a busy early voice can consume shared slots before later voices. `eng_overload` lowers admission, but existing grains above the new ceiling are still rendered: recovery is not an immediate load shed. `voice_start` does not clear all old grains when reassigning a voice; they remain associated with that voice slot/filter/envelope. This differs from an independent grain-level release/steal policy. Do not copy it as a four-track fairness solution.

### Dual core and real-time safety

C: `fx.c:c1_parts`, `mix_split`, `mix_block` already implement a track cost-based split. Each worker owns its track's engine state, spread buffers and modulation state; events/voice allocation run before dispatch. Keeping this ownership avoids locks in the audio path. `tests/dualcore_test.c` is the relevant existing deterministic/threaded regression fixture.

G: `eng_render:1683–1712` clears worker buffers, publishes `job_n`, calls `plat_cpu2_sync/run`, renders even grains locally and waits before adding the worker's odd grains. On failed wait it disables dual core and runs odd grains locally. This depends on the **unavailable platform contract** guaranteeing that a timed-out worker cannot still mutate the same grain state. The archive alone cannot prove mailbox ordering, cache coherency, quiescence, fallback correctness or stack guards. Floating-point buffer grouping also does not guarantee one-/two-core bit identity. Scheduling/RNG/window preparation precede dispatch, so workers do not concurrently allocate grains in this code.

Both render paths use static/stack storage rather than heap allocation or user-facing blocking I/O. GRAINE's worker wait is nevertheless a synchronization deadline. `win_make` can perform a 513-entry rebuild on a control chunk; that work belongs in the callback budget. Moving our grains into one concurrent shared pool would remove an existing race-avoidance boundary. Retain track-local pools first.

## Grain windows: useful ideas and hazards

G: `engine.c:100–151`, `shape_base`, `win_init`, `base_at`, `win_make`; `grain_run:913–916`. C: `eng_grain.c:54–77` `GR_HANN`, `gr_win:390`, `gr_run:397`.

| Shape in the actual GRAINE code | Actual implementation | Musical use / recommendation |
|---|---|---|
| Gaussian | `exp(-9*(2t-1)^2)`; final constructed table endpoints forced to zero | Soft localized cloud; useful, but explicit endpoint fix introduces a small edge departure from the raw Gaussian |
| Hann | `0.5 - 0.5*cos(2*pi*t)` | Smooth neutral overlap; keep our existing Hann as exact default |
| Tukey | Flat central half, cosine-tapered outer quarters | More body/transient retention than Hann; good next contrasting option |
| “Rectangular” | Flat where `abs(2t-1)<0.94`, cosine edge taper outside | **Not a hard rectangle**: short fades intentionally reduce edge discontinuity; suitable for STUTTER only after click tests |
| Morph | Linear blend of adjacent Gaussian→Hann→Tukey→tapered-rectangle tables | Useful continuous texture, but consumes more control work/state than discrete windows |
| Skew | Piecewise time warp with peak `0.5+0.47*skew`, bounded near 0.03–0.97 | Percussive/slow-attack grains without changing their total lifetime; potentially useful across all personalities |

GRAINE caches shape/skew until either changes by at least 0.002, then rebuilds one **global** table. `eng_render` uses global normalized `gn[P_WIN]`/`gn[P_SKEW]`, not a separate table per grain. Active grains immediately read the rebuilt table at their current phase. This can alter an in-flight envelope abruptly; the threshold is not a crossfade. No claim that live morphing is click-free is justified by this source alone. Per-voice modulation of these parameters also needs checking against this global-table consumer.

Our Hann costs **514 flash bytes**, plus a 26-byte overlap table. GRAINE's five float tables cost **10,260 RAM bytes**, independent of pointers (assuming four-byte float). Four private copies would cost 41,040 bytes before grains. There is no reason to import that layout. A new 257-entry Q15 window is 514 flash bytes; two additional shapes are 1,028 bytes. Integer blend at the existing block endpoints could be inexpensive, but its sound differs from per-sample window evaluation, especially for tiny grains and skewed/tapered edges. These are storage calculations, not linked-size or timing measurements.

**Safe strategy:** begin with one additional discrete Q15 Tukey table, existing Hann exact default, window identity latched at grain spawn, explicit zero endpoints and bounded phase interpolation. Retain block-ramp rendering initially and measure short-grain error. Never mutate the table being used by active grains. Later, latch a bounded morph/skew value per grain and blend table values at the same evaluation points; do not regenerate four track tables in the sample loop. Check envelope energy/normalization because the current `1/sqrt(overlap)` correction assumes neither constant RMS nor equal gain across window shapes. All four personalities can use a window without changing their pitch/scheduling semantics; FREEZE long HOLD needs full-length phase tests, and STUTTER needs special edge tests.

## Reverse playback and boundary safety

C already supports forward/reverse in **every personality** through `gr_spawn_one` and `gr_run`. `rev = ((r1>>16)&255) < RAND`: RAND=127 gives 127/256 reversal probability, while the same parameter also sets pitch randomization. Adding a dedicated direction choice would be a new control, not a missing decoder implementation. No reassignment of RAND is recommended.

C fits the complete grain span before starting: wide `len*step`, ceiling for reverse, floor for forward; shortens length to the zone; forward start bounded by `n-3-span`, reverse bounded by `[span,n-2]`. `gr_fill`/`gr_seek` reconstruct ADPCM reverse windows. Source head is `gr_head_t {sample,frac,target}`, avoiding the old 65,535-sample Q16 integer limit. Live POS/SHP applies target deltas only to an admitted candidate head, preserves its fraction and leaves old grains alive. Source-generation reset invalidates the head even if the zone index is unchanged. Seek-index capacity remains **1,075 entries per track across all zones**, not unbounded storage for arbitrary user material; over-capacity zones remain silent by design.

GRAINE chooses sign at spawn and stores signed float `rate`; the render floors rate into integer/Q32 fractional increments. Initial source window controls spawn positions, but grains wrap through the **whole source**, not just the selected START/LEN interval. The runtime wrap uses `len1 = source.len-1` and subtracts/adds `len1` only once per sample (`grain_run:931–934`), while neighbor interpolation wraps at `>len1`. That endpoint convention and single-wrap assumption require tests before reuse, particularly with very short sources, high notes/glide and rates exceeding one source length. This is a source-observed risk, not a reproduced GRAINE crash. Unlike C, it does not fit a full variable-rate grain span within a non-wrapping zone.

**Recommendation:** optional independent FORWARD/REVERSE/MIX can reuse C's existing bounded direction paths, retaining legacy RAND behavior as the default. Keep reversal latched per grain; direction changes should affect future grains, not abruptly flip running ADPCM state. Preserve source head movement independent of direction. A reverse-probability control must not consume extra RNG in the legacy mode or break neutral audio identity. Do not import GRAINE's wrap code or replace the repaired head.

## Pitch, scan, scheduling and stereo

| Feature | Source finding | Safe relevance to COFFEE GRAINS |
|---|---|---|
| Pitch | G: `semis_ratio`, `grain_spawn:808–858`: note/±24 pitch/fine, quantized random spread, probabilistic shimmer, small grain detune, then source-rate ratio | Keep C's existing PTCH, RAND, SHIMMER interval and allocator DTUNE convention; do not substitute GRAINE's semitone detune scale |
| Intra-grain glide | G multiplies `g->rate` by `rmul` every 16 rendered samples, derived from ±24 semitones over grain duration | Useful new sound, but dynamic rate invalidates C's constant-span proof and admission estimate; highest expected instantaneous rate must fit 2× cap and decode budget |
| Scan | G `voice_scan:956`: signed SPEED, loop/ping-pong/once/drunk/jumps; POS slew and key position in `voice_chunk:1020` | Interesting future modes; do not reinterpret C's repaired POS or DRIFT. Shared scan is advanced from each active `voice_chunk`, so voice-count dependence requires verification |
| Stretch | G stretches grain **duration** by 1–8× (size up to 16 seconds); C STRETCH reduces source-head scan factor | Different semantics. Copying G's STRETCH would break patches; preserve C's current mapping |
| Position randomization | G uses P_SPRAY around effective normalized position; C uses G-SPRD for scheduling jitter, and personality-specific positional spread | Preserve CLOUD WIDTH, FREEZE MOTION and G-SPRD identities; do not merge these controls |
| Density/timing | G 0.5–150 grains/s normally, pad can quadruple it; periodic/random/clock divisions with within-chunk offsets | Sample-offset scheduling can reduce C's 32-frame quantization; seeded optional random/clock modes later, without changing the sequencer clock |
| Stereo | G random equal-power pan per grain, plus detune-related voice positions | Attractive width, but C grain accumulator is mono. Requires owned stereo accumulation and coordinated interaction with common pan/V-SPRD; not a small decoder tweak |
| Level compensation | Both estimate overlap and scale by reciprocal square root; C uses a bounded Q15 lookup | Changing window, density distribution or shared admission alters actual overlap; test perceived level rather than assuming normalization remains correct |

C: `gr_spawn_one:330–361` preserves allocator `voice.fine` via rate multiplier `(4096+fine)/4096` **before** the existing 2× cap; center stays unchanged. Scan excludes that voice detune and RAND. `grain_render:570–601` advances scan with STRETCH, CLOUD DRIFT/64 and a saturating live-target policy. C's TONE is independent, smoothed one-pole (127 exact bypass); CUTOFF=0 still applies TONE, resets resonant state and accumulates dry output with the existing amplitude ramp. These completed fixes must remain invariant.

C envelope/filter state lives in existing `voice.s[]` slots; they are not spare space for new window/glide state. G's parameter normalization, event queue (64 entries) and per-voice `vn[96]` enable broad modulation but would duplicate C's existing MIDI/motion/modulation infrastructure. Do not transplant them.

## Four personalities

| Personality | Current behavior verified in C | Best incremental ideas | Avoid |
|---|---|---|---|
| CLOUD | DRIFT scales source scan 0..127 with 64 neutral; WIDTH governs positional spray; G-SPRD still jitters timing | Gaussian/Tukey choice, optional timing distribution, later independent grain pan | Replacing DRIFT with G's SPEED or coupling grain pitch to scan |
| FREEZE | HOLD extends grain duration; MOTION gives quadratic positional spread; scan still responds to STRETCH | Smooth Gaussian/skewed envelope, explicitly controlled slow movement | Calling it a perfectly frozen spectral buffer; G's capture/GP.freeze model is different |
| STUTTER | RATE adjusts spawn interval and independent gate period; GATE masks rendered accumulator | Short-edge-taper window, later sequencer-synchronized scheduling as a separate mode | Changing existing gate phase/period to G's clock chopper; hard rectangular edges |
| SHIMMER | Optional second grain at 7..19-semitone interval scaled by HARMONICS; same 2× cap/admission | Alternate window and carefully bounded glide; fair paired-grain admission as a later explicit policy | Confusing grain shimmer with GRAINE's separate reverb shimmer delay; raising pitch ceiling |

No idea requires replacing these four personalities. Shared allocation and stereo grains are architectural projects, not personality fixes.

## Memory, CPU, flash and stack

### Measurements and source-derived costs

Host sizeof probes, compiled outside the repository from the actual type definitions:

| Item | Result / classification |
|---|---|
| C GRAIN state | **7,296 bytes/track**, measured; four states use 29,184 bytes logically |
| C engine reservation | **12,888 bytes/track**, source constant; 51,552 bytes reserved for all engines across four tracks, unchanged by grain-only UI |
| C remaining private capacity | 5,592 bytes/track in host layout; not additional free device RAM |
| C grain / common voice / track | **64 / 84 / 1,840 bytes host**, measured; the latter two are existing common infrastructure |
| C source head | 8 bytes/voice, measured by existing position test |
| G grain | **80 bytes host**; 10,240 bytes for 128 host grains |
| G voice | **496 bytes host**, including 96 normalized floats; 2,976 bytes for six |
| G 32-bit grain estimate | 68 bytes with ordinary four-byte pointer/float alignment; **8,704 bytes/128**; unverified target ABI |
| G main + worker voice buffers | `2 * 6 * 2 * 64 * 4` = **6,144 bytes**, source-derived, both statically allocated even in one-core fallback |
| G mix buffers | `2 * 64 * 4` = **512 bytes** |
| G window buffers | **10,260 bytes RAM**, source-derived |
| G capture | `65472 * 2` = **130,944 bytes**, PLAT_POOL |
| G echo / reverb lines | 105,840 / 30,916 bytes, PLAT_POOL; excludes indices and other state |
| G additional FX | allpasses 3,620 bytes float; shimmer 4,096 bytes; resonator lines 9,216 bytes; excludes state |

These are selected components, **not a linked RAM total**. G's PLAT_POOL placement, linker limits and stacks cannot be verified without its missing platform. C's full voice/track objects are shared infrastructure, not all granular state; retaining other engine code means their reservation cannot be reclaimed just by raising GR_NG.

C `firmware/app.ld:16–20` defines 581,564-byte XIP image, 24 KiB RAMTEXT, 72 KiB RAM and 360 KiB POOL, plus NOINIT/reserved boot region. Pool RAM and the 240 KiB user **flash** sample pool are distinct. Neither is available to borrow without memory/storage design work. Factory audio/SLICE and startup assets already compete for XIP; do not infer remaining firmware headroom by subtracting only sample bytes. No linker or memory-map changes are recommended.

C per-sample cost includes ADPCM decode, linear interpolation and gain; reverse refills seek and decode a 128-sample cache. One index entry (256 decodes) is built per track/block; initial seeks may skip 255 samples. G per-sample cost includes mu-law lookup/int16 load, float source/window interpolation, stereo multiplies/adds and index updates; expensive pitch/log/normalization functions generally run at spawn/control rate, glide updates rate every 16 samples. A direct “128 is faster than 48” comparison is invalid: formats, overlap, rates, stereo work and filters differ. C's top-of-file historical instruction counts are comments, not newly measured target performance.

Host compiler stack data retained from the exact base's previous build reports `grain_render` 328 bytes and `gr_spawn_one` 112 bytes; this audit does not turn these into target worst-case stack guarantees. GRAINE target frames and maximum callback cycles are unmeasured. Per-core worst-case cycles, cache contention, seek bursts, worker deadlines and stack high-water marks must be measured on FM1 before increasing budgets.

### Conservative, moderate and ambitious budgets

| Proposal | Slots | Resource/fairness implications | Assessment |
|---|---:|---|---|
| Conservative | Keep 12/track = 48 reserved | Keep 8-unit decode cap, quotas, track ownership; improve windows/tests first | Recommended; a new shape can improve sound without raising workload ceiling |
| Moderate | Experiment with 16/track = 64 reserved | Four extra records + reverse caches/track: target-layout estimate 4×(56+256)=1,248 bytes/track; normalization table grows; index fixed; actual occupancy still work-limited | Plausible within private reservation on paper, **not measured fit or speed**. Keep decode cap first |
| Ambitious | 96–128 globally shared | A 128-grain C-style pool roughly 39,936 bytes for 56-byte target records +256-byte reverse caches alone, before four seek indices (~12,900 bytes), heads/metadata | Major redesign; exceeds current 51,552 reservation before all state at 128. No recommendation to transplant |

The moderate record-size estimate follows the C field layout with four-byte pointers/alignment; host records are larger. These are layouts, not compiled target results. Separate CPU budget from slot budget: retaining the decode cap means more slots mainly benefit slow grains. Raising it multiplies worst-case decode work and invalidates existing cost bounds.

A safe *logical* shared budget could leave memory track-local and precompute bounded credits before dispatch, with a per-track guaranteed floor, deterministic rotating surplus, owner-local mutation, reverse-weighted work charges and a global cap. It would still change admission/sound and needs its own compatibility decision. Truly shared storage requires per-track partitions or ownership transfer outside worker rendering, generation-safe voice references and deterministic starvation/paired-SHIMMER policy. No audio locks, heap operations or unsynchronized free-list are acceptable. GRAINE's first-free single-patch scheme does not supply those guarantees.

## Licensing and reuse boundaries

G: `LICENSE` is MIT, copyright 2026 GRAINE contributors; engine/header/tools carry MIT SPDX headers. Its license explicitly identifies `base/` (fm1-base and felucca-bsp) as MPL-2.0. `.gitmodules` points to `https://github.com/charlesvestal/fm1-base.git`, but the archive's base directory is empty: no exact platform revision, full dependency source/license text or CPU2 contract was supplied. G `app/fm1.toml` credits synthesized built-ins generated by `tools/gen_sounds.py`; this is provenance evidence for G's materials, not a license grant for unrelated recordings.

C `LICENSE`, `LICENSING.md`, `CREDITS.md`: firmware including `eng_grain.c` is GPL-3.0-only, with separate MIT engines, font licenses, Apache/MSFA components and sample provenance obligations. MIT GRAINE code can be incorporated into GPL firmware with its copyright/permission notice retained for copied/substantial portions; GPL distribution/source obligations remain. Do not relabel MIT portions as solely original work. MPL platform reuse would need file-level compliance and inspection of actual dependencies; unavailable BSP code is not cleared by the app's MIT license. No GRAINE code, table or sample asset was copied into production here.

| Technique | Classification | Reason |
|---|---|---|
| MIT window/helper source | Directly reusable with retained notices, after correctness review | Licensing permits it; float/global-table design is still a poor architectural fit |
| Hann/Tukey/Gaussian formulas and Q15 lookup generation | Better independently reimplemented | Standard mathematics; preserve integer behavior and reproducible tables; any copied implementation retains attribution |
| Per-grain reverse choice | Better adapted | Existing C decoder, span proof and caches are the correct foundation |
| Glide | Better adapted | Must fit C's fixed-rate boundary/load invariants and allocator convention |
| Offset/random scheduling | Better adapted | Keep per-track RNG and sequencer clock; do not import single-patch state |
| Per-grain equal-power panning | Better adapted | Requires review of common spread/mixer ownership and mono buffer contract |
| Global floating window mutation, wrapping reader, shared pool/CPU2 fallback | Not recommended as direct reuse | Active-grain changes, boundary assumptions, fairness and platform synchronization differ |
| GRAINE FX/capture/scene subsystem | Not recommended for this phase | Unrelated feature/storage expansion; would replace retained four-track functionality |

## Prioritized roadmap (proposals only)

Ratings are qualitative estimates. **No new parameter ID is allocated in this audit.** Any new persistent control needs a separately approved schema/editor/backup compatibility design; never repurpose existing TONE, DTUNE, DRIFT, STRETCH, WIDTH, G-SPRD or reserved fields silently. Neutral modes must preserve old RNG consumption and exact audio where feasible.

| Priority | Stage | Audible benefit | Complexity / resource risk | Compatibility / testability |
|---|---|---|---|---|
| 1 | Discrete alternate window | High texture contrast across all personalities | Low–medium; ~514 flash bytes/table plus selector implementation | Hann default exact; easy deterministic envelope/audio tests |
| 2 | Independent reverse mode | High control over an existing capability | Low–medium; tiny control state, existing cache/work caps | Legacy RAND mode exact; boundary tests already available |
| 3 | Optional sample-offset scheduling | Medium–high rhythmic consistency | Medium; spawn bursts/CPU deadlines | Legacy countdown mode exact; compare spawn sample indices |
| 4 | Latched window morph/skew | Medium–high | Medium; LUT/control cost, grain fields and normalization | Default neutral; click/energy tests needed |
| 5 | Bounded intra-grain glide | High experimental sound | Medium–high; variable-rate boundary proofs and reverse decode load | Zero exact; robust numerical reference possible |
| 6 | Per-grain stereo movement | Medium–high | High; buffer/filter/pan architecture | Center exact; common V-SPRD interaction significant |
| 7 | Larger/shared grain budgets | Uncertain until profiling identifies audible drops | High–very high CPU/RAM/concurrency risk | Admission changes patches; hardest deterministic validation |

### Stage acceptance plans

1. **Selectable window — recommended first small sound improvement.** C `eng_grain.c:GR_HANN`, `gr_win`, `gr_run`, `gr_spawn_one`; descriptor/editor integration in `params.c` only after control storage is approved. Prototype a compile-time test variant first, then a per-grain-latched Hann/Tukey selector, default Hann. Numerical tests: table bounds, zero endpoints, symmetry, Q15 limits, phases including UINT32_MAX, output canaries, legal short/long lengths and FREEZE HOLD extremes. Audio: four personalities, five factories/user/sine, pitch/reverse extremes, spectral/energy and click metrics, exact Hann PCM baseline. Hardware risk: table/code flash and extra endpoint lookup, not a larger pool. Accept only with neutral identity, bounded energy, no sanitizer errors and measured linked/callback cost.

2. **Independent direction.** C `gr_spawn_one`, `gr_fill`, `gr_run`; later approved control adds LEGACY/FWD/REV/MIX or a probability with explicit legacy mode. Do not retask RAND; no change to source scan. Tests: lengths 65,535/65,536/65,539/275,098, short legal zones, POS endpoints/live changes, fractional rates, rate cap, canaries, fixed seeds and four tracks. Accept legacy PCM/RNG identity, deterministic future-spawn changes, no OOB and unchanged decode admission; link/CPU measurements before shipping.

3. **Scheduling.** C `grain_render` countdown/admission and `gr_spawn`; reference G `voice_chunk:1058–1090`. First add instrumentation/tests for admitted/dropped spawn time without a UI change; only then an optional sample-offset scheduler. New timing/sync/random controls require approval; keep old DENS/G-SPRD semantics in legacy mode. Tests: frame partition invariance, periodic/random statistics, maximum bounded catch-up, transport reset, swing/external MIDI clock, SHIMMER paired requests and fair four-track overload. Accept no unbounded loop, unchanged legacy PCM, declared clock semantics, worst-case callback below measured device deadline. Do not silently synchronize STUTTER to a different clock.

4. **Morph/skew.** C window helpers and new explicitly owned per-grain fields; G `win_make` for reference only. Latch controls, preserve old active grains; use static Q15 tables instead of global live table replacement. Tests: endpoints, peak mapping, monotonic blend, overflow bounds, live edits/retrigger, short-grain approximation error and equal-energy listening/audio metrics. Accept click limits, Hann identity, no callback table builds and compiled state still under engine reservation. Flash/RAM estimates must be remeasured.

5. **Glide.** C `gr_spawn_one` span/load proof and `gr_run`; G `rmul`/16-sample update. New bipolar glide control defaults zero. First derive wide-integer maximum-rate integrated span for each direction, enforce 2× instantaneous cap and charge peak decode cost; do not reuse constant `len*initial_step` as the safety proof. Tests: ±glide, pitch caps, long HOLD, forward/reverse caches, RAND/SHIMMER/DTUNE extremes, output/state canaries, reference integration and meaningful audio pitch trajectory. Accept zero PCM identity, proven bounded reads/cost, no modified scan/TONE/DRIFT and hardware deadline evidence. Stop if safe span fitting forces unacceptable behavior or cost.

6. **Stereo grains.** C `grain_render` mono `acc`, `voice.c` spread/mix contract and `fx.c:mix_part`; G `grain_spawn` pan and stereo buffers. A separate approved grain-pan amount must not replace V-SPRD. Prototype with existing buffers only after ownership audit; no arbitrary second framebuffer/audio array. Tests: centered identity, symmetric/equal-power energy, four-track isolation, pan/mute/sends, mono collapse and dual-core determinism. Accept no double-panning/gain jump, documented added stack/buffer cost and target fit. This is not a minimal first change.

7. **Budget experiments.** C `GR_NG`, `GR_NORM`, `gr_part_t`, `ENG_MEM_FITS`, `fx.c:mix_split`; no initial new user control. Measure drops/CPU with 12 local grains before raising to 16 or adding shared credits. Tests: sustained/released voices, all four tracks, deterministic fairness, SHIMMER saturation, source changes, CPU2 fallback, TSAN ownership checks and sanitizer canaries. Accept no starvation, no locks/allocations, bounded per-track decode cost, retained project formats and measured worst-case hardware cycles/stack/linked headroom. Do not pursue 128 shared grains merely to match GRAINE's headline capacity.

## Executed tests and reproducibility

All test binaries/output and sizeof probes were written outside the repository under `/tmp/graine-audit-results`. Existing COFFEE GRAINS fixtures were compiled from the exact clean base worktree, with existing generated headers/pinned engine dependencies; no source/test/asset changes.

- `tests/grain_only_audio_test.c`: **330 checks passed**, 32 sounding cases spanning four personalities, tracks and spread settings. Explicit master 4096 and peak/RMS/nonzero assertions; example final case peak **8,753**, RMS **1,716.817749**, **56,803/64,000** nonzero samples.
- `tests/grain_position_test.c`: **854,920 checks passed**, including full-range head/span/boundary checks, live changes and sounding simultaneous four-track validation. Measured state **7,296/12,888**, head **8 bytes**.
- `tests/grain_tone_audio_test.c`: **36 successful four-track renders**: four personalities × factory source 0/user/sine × TONE 0/64/127, CUTOFF=0, RESON=0, static mode. Each fixture asserts meaningful sounding output. This is an existing behavior smoke matrix, not a new proof of every spectral or live-edit case.
- No ASan/UBSan diagnostics in these runs. Flags: `-O1 -g -w -ffp-contract=off -fsanitize=address,undefined -fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow -fno-sanitize-recover=undefined -Ibuild/gen -Ifirmware/src`, then source, `-lm`. These are the existing host-suite exclusions, **not strict all-category numerical sanitization**; none was added by this documentation audit.
- GRAINE `sh build.sh --host` stopped with exact error: **`build.sh: no fm1-base in base/ (git submodule update --init --recursive)`**. Its `tests/run_tests.sh` needs that host build before running `app/tests/{features,drone_glitch,import}.txt` and importer tests. No dependency was fetched, substituted or executed to guess around the missing submodule. GRAINE audio, import, dual-core and fastmath runtime behavior remain untested here.
- COFFEE GRAINS UI/MIDI/motion/web matrices were not rerun for a report-only change. Prior SEQ checkpoint's 15 MIDI and 27 motion failures are historical, not passing results or newly classified by this audit. There is no new cross-engine PCM equivalence claim: these are different synthesis designs.

Example reproduce the tone matrix after compiling the existing fixture to `$OUT/tone` with the flags above:

```sh
# type=0..3, source=0/user/sine, tone=0/64/127
"$OUT/tone" 0 0 64 0 0 static 4 "$OUT/tone.pcm"
```

For architecture sizeof, the GRAINE `grain_t`/`voice_t` definitions were read verbatim into an external probe including its `graine.h`; no DSP implementation was imported into C. Four-byte target figures above are arithmetic layout estimates, not target-build results.

## Open questions and limits

1. Obtain the exact GRAINE platform/submodule revision, licenses, linker map and CPU2 wait/fallback contract before executing a trustworthy full comparison. Its missing fastmath implementation also prevents coefficient/cycle/approximation analysis from being complete.
2. Measure both instruments on the same FM1 at the same source rate, active voices, grain lengths/rates and effects. Host counts cannot establish target throughput, float cost or flash cache pressure.
3. Verify GRAINE shared-scan advancement per active voice, mutable global window click behavior, selected-window versus whole-source wrap, and high-rate single-wrap boundaries with actual host tests. These are review findings/open risks, not reproduced audible defects.
4. Profile COFFEE GRAINS admission failures and reverse-cache/index startup before considering shared storage. More slots cannot solve source-index capacity or weighted decode ceilings.
5. Decide persistent control allocation/version compatibility separately. This audit approves no new IDs, format changes, UI or preset rewrites. Existing sounds and four-track structure remain the reference behavior.
6. No successful target link, stack high-water capture, cycle measurement or hardware flashing occurred. **Hardware fit and release readiness remain unverified.** This report changes firmware text/data/BSS by **zero**; proposed feature costs are estimates until implemented and linked.

**Conclusion:** retain the fixed-point ADPCM reader, repaired source head, four track-local pools and current whole-track dual-core ownership. Start with a discrete, latched alternate grain envelope; expose existing reverse behavior more clearly only through an independently approved compatible control. Pursue glide, stereo grains and shared allocation as separate measured projects, not a wholesale GRAINE port.
