# COFFEE GRAINS — GRAIN DSP audit

Audited repository: `Auberg1ne/Felucca`. Exact starting revision:
`44ec4606c0469cc79326a4887b78e321ec704739`, `feat/coffee-grains-grain-only`.
Documentation branch: `audit/coffee-grains-grain-dsp`.

**Read-only implementation audit. No DSP, UI, test fixture, asset, descriptor,
protocol, storage or MIDI implementation is changed by this report.** Source
references and measurements below describe that exact starting revision.

## 1. Executive summary

GRAIN renders useful, nonzero audio in all four personalities, but several
visible controls do not deliver their advertised function. The descriptor and
persistence repair for STRETCH is correct; it did not repair the renderer.

| Priority | Finding | Evidence classification |
| --- | --- | --- |
| P1 | CUTOFF **0 mutes**, rather than bypasses, every personality | Confirmed rendered silence and early return |
| P1 | Source head has only 16 integer bits; **all five factory recordings exceed its range** | Confirmed arithmetic and observed wrapping |
| P1 | Live POS/SHP modulation is ignored after head initialization | Confirmed identical live-change renders |
| P1 | TONE 117 does nothing in all personalities | Confirmed identical nonzero PCM at 0/64/127 |
| P1 | CLOUD DRIFT 104 does nothing | Confirmed identical nonzero PCM at 0/64/127 |
| P1 | UNISON DTUNE 44 does nothing to GRAIN pitch | Confirmed identical nonzero final mixer output, despite different voice fine values |
| P1 | Maximum FREEZE length/rate overflows `len * step` | Confirmed 32-bit/64-bit span discrepancy |
| P1 | Valid long user sources can exceed the fixed seek-index capacity and be silent | Confirmed DSP boundary test, not a storage failure |
| P1 | Resonant output can exceed the amplitude helper's signed-product range | Confirmed with bounded synthetic filter input; not triggered by the 48 stock-source numeric stress renders |
| P1 (tests) | The earlier 14,336,000-byte audio identity comparison compared silence | Confirmed zero-master fixture; replacement nonzero comparisons below |
| P2 | G-SPRD is temporal jitter only in CLOUD/FREEZE; WIDTH/MOTION replace its spatial effect | Confirmed spawn trace and distinct temporal renders |
| P2 | SHIMMER interval and PTCH hit the two-source-samples/output rate ceiling | Confirmed clipping and identical upper-interval renders at MIDI 60 |
| P2 | Short grains that fit within one control block have zero Hann endpoints and can render silence | Confirmed short-zone renders |
| P2 | FREEZE is extended grains plus position scatter, not a frozen capture/head | Confirmed moving head and implemented formulas |
| P2 | Common FLT modulation is ignored; SHP modulation only seeds the initial head | Confirmed source trace and nonzero mixer comparisons for FLT |
| P2 | Personality defaults, preset names and editor labels can mislead | Confirmed descriptor/load/metadata paths |

No crash or memory corruption was found in the exercised paths. This is **not**
a proof of memory safety for arbitrary data, target real-time performance, or
hardware flash fit. No P0 defect is currently confirmed.

“Functional” below means a traced mechanism plus a measurable rendered-signal
change under the stated conditions. Hash differences alone do not establish
perceptual usefulness. No listening panel, calibrated pitch analysis, target
recording or hardware timing measurement was performed.

## 2. Parameter-to-DSP matrix

### Common UI, persistence and normalization path

All listed parameters live in each track's `int16_t p[118]`; engine ID remains
8. The authoritative grid is [params.c:553–592](../firmware/src/params.c#L553).
SELECT chooses rows 1–3, wrapping; K1–K4 address that row. EDIT changes between
the two pages. Row entry and track changes reset to row 1. GLO+SELECT and
modifier priorities precede ordinary row navigation
([ui_input.c:1754–1777](../firmware/src/ui_input.c#L1754)). Empty cells resolve to
255 and are inert. Physical edits write `p[id]`, then capture motion and MIDI
learn ([ui_input.c:1044–1051](../firmware/src/ui_input.c#L1044)).

The grid labels, units and live values use `track_desc` and `param_format`
([grain_edit_grid.c:23–57](../firmware/src/grain_edit_grid.c#L23)). `F_PCT`
0–127 displays rounded 0–100%; `F_INT` is a raw integer; PTCH uses semitones.
SIZE/DENS percentages are **not** milliseconds/grains per second; STRETCH's raw
integer is **not** a time ratio. ID 115 displays **G-SPRD**, ID 95 **V-SPRD**.

For **every matrix row**, project capture saves motion-base values, FUN11 packs
raw `value + 64`, and runtime loads clamp with `param_desc_of(engine,id)` /
`param_fit`; presets similarly unpack then clamp. See
[project.c:659–676](../firmware/src/project.c#L659),
[project.c:884–889](../firmware/src/project.c#L884),
[project.c:923–930](../firmware/src/project.c#L923),
[project.c:1173–1176](../firmware/src/project.c#L1173), and
[upreset.c:286–296](../firmware/src/upreset.c#L286).
No DSP normalization rescales the stored value itself.

Editor GET/SET/DESC resolve the same stable IDs and canonical ranges
([editor.c:489–500](../firmware/src/editor.c#L489),
[editor.c:714–755](../firmware/src/editor.c#L714)); TRACK_PARAM supports all
tracks. MIDI learn/CC validation uses the canonical descriptor
([midi_control.c:230–240](../firmware/src/midi_control.c#L230)). Motion/locks accept
all listed IDs **except VCE**, whose voice-allocation changes are intentionally
excluded ([motion.c:34–44](../firmware/src/motion.c#L34),
[motion.c:167–174](../firmware/src/motion.c#L167)). A parameter being stored,
learnable or automated does **not** mean the renderer consumes it.

In this table, “default” means descriptor default, not the loaded factory preset.
`G` references `firmware/src/eng_grain.c`; each link points at the exact consumer
or descriptor. `I/B/G/N` mean immediate next render block / next source block /
next successfully spawned grain / next relevant note event.

| Page/row/knob | Control; stable ID | Type; raw range; default; display | Actual consumer and rendered effect | Timing; personality coverage; assessment |
| --- | --- | --- | --- | --- |
| 1/1/K1 | SRC; 110 | enum; 0–14; 0; source name | [G:108–156](../firmware/src/eng_grain.c#L108), [G:406–421](../firmware/src/eng_grain.c#L406): zone selection, cache rebuild, old grains cleared | B, then indexing/scheduling delay; all four; functional, abrupt source switch can drop audio |
| 1/1/K2 | POS; 111 | percent; 0–127; 32; 0–100% | [G:264–271](../firmware/src/eng_grain.c#L264): POS + `m.shape` computes a center, but only initializes a new voice/zone head | G after N or changed zone index; all four; initial position functional, sustained edits ineffective; long-source truncation |
| 1/1/K3 | SIZE; 112 | percent; 0–127; 80; 0–100% | [G:245–263](../firmware/src/eng_grain.c#L245): exponential grain duration, FREEZE extension, fit to source | G; all four; functional, with short-window and FREEZE multiplication defects |
| 1/1/K4 | DENS; 113 | percent; 0–127; 80; 0–100% | [G:477–492](../firmware/src/eng_grain.c#L477): interval approximately 1–98 grains/s, STUTTER multiplier, jitter and pool/load limits | New interval when countdown expires; existing countdown not retimed; all four; functional but requested density may not be achieved |
| 1/2/K1 | PTCH; 114 | semitone integer; −24…24; 0; signed st | [G:251–255](../firmware/src/eng_grain.c#L251), [G:465–470](../firmware/src/eng_grain.c#L465): per-grain decoder rate and source-head speed; also sine fallback | G for samples, I for fallback; all four; functional with rate ceiling, head/pitch coupling and ignored fine detune |
| 1/2/K2 | G-SPRD; 115 | percent; 0–127; 30; 0–100% | [G:272–277](../firmware/src/eng_grain.c#L272), [G:486–488](../firmware/src/eng_grain.c#L486): spatial spray in STUTTER/SHIMMER; interval jitter in all | G / next interval; CLOUD/FREEZE spatial calculation overwritten by B; functional but label overpromises in those modes |
| 1/2/K3 | RAND; 116 | percent; 0–127; 10; 0–100% | [G:238–256](../firmware/src/eng_grain.c#L238): quadratic random pitch depth up to about ±12 st, reverse probability up to 127/256 | G; all four; functional, affected by pitch/load limits; not an independent random-position control |
| 1/2/K4 | TONE; 117 | percent; 0–127; 127; 0–100% | [G:554](../firmware/src/eng_grain.c#L554) descriptor/preset storage only; no renderer read of `P_E7` | None; ineffective in all four |
| 1/3/K1 | STRETCH; 108 | integer; 0–127; 0; raw integer | [G:462–474](../firmware/src/eng_grain.c#L462): slows inter-grain source head by `pow2_q16(-v*10)`, not grain playback rate | I for head movement, G for audible position changes; all four; functional but limited, not robust pitch-independent full-source time stretching |
| 2/1/K1 | TYPE; 103 | enum; 0–3; 0; CLOUD/FREEZE/STUTTER/SHIMMER | [G:245–321](../firmware/src/eng_grain.c#L245), [G:477–505](../firmware/src/eng_grain.c#L477): selects A/B behavior | Gate behavior I, length/pitch/spray G, density next interval; all four selectable; old grains continue, no personality crossfade |
| 2/1/K2 | Personality A; 104 | 0–127; shared default 64; percent except SHIMMER integer | CLOUD: unused; FREEZE: length extension; STUTTER: scheduling + gate period; SHIMMER: 7–19 st boost interval; details below | None / G / I+next interval / G respectively; mixed effectiveness |
| 2/1/K3 | Personality B; 105 | percent; 0–127; shared default 64 | CLOUD: linear position scatter; FREEZE: quadratic scatter; STUTTER: pulse-gate duty; SHIMMER: extra-grain gain | G / G / I / G; all have rendered effects; SHIMMER also changes RNG/pool consumption |
| 2/2/K1 | CUTOFF; 106 | integer; −50…50; 50; signed raw number | [G:508–532](../firmware/src/eng_grain.c#L508): negative HP, positive LP, zero exits without output | I; all four; functional away from zero, zero is a confirmed mute defect |
| 2/2/K2 | RESON; 107 | percent; 0–127; 0; 0–100% | [G:516–525](../firmware/src/eng_grain.c#L516): SVF damping from 2 to 592/4096; Q approximately 0.5–6.92 | I; all four, no effect with CUTOFF 0; functional; resonant amplitude headroom needs repair |
| 2/3/K1 | VCE; 37 | enum; 0–3; 0; POLY/MONO/LEG/UNI | [voice.c:112–121](../firmware/src/voice.c#L112), [voice.c:337–365](../firmware/src/voice.c#L337): allocation, mono/legato retrigger rules, up to three unison voices | Primarily N; existing voices not instantly reallocated; all four; functional shared control; one held note cannot distinguish POLY/MONO/LEG |
| 2/3/K2 | DTUNE; 44 | integer; 0–127; 40; raw integer | [voice.c:339–345](../firmware/src/voice.c#L339) sets unison `v.fine`; [voice.c:743–747](../firmware/src/voice.c#L743) forwards `m.fine/m.inc`, neither used by grain-rate code | Intended N/UNISON; actually ineffective in GRAIN all four; not expected to affect an ordinary single POLY voice |
| 2/3/K3 | V-SPRD; 95 | percent; 0–127; 0; 0–100% | [voice.c:312–313](../firmware/src/voice.c#L312), [voice.c:754–758](../firmware/src/voice.c#L754), [fx.c:818–862](../firmware/src/fx.c#L818): voices alternate left/right around track PAN | I on existing assigned sides; all four in POLY/UNI; intentionally inactive MONO/LEG; confirmed stereo effect, not grain-position spray |

The raw descriptor definitions are [G:543–563](../firmware/src/eng_grain.c#L543)
and [params.c:129–180](../firmware/src/params.c#L129).
Personality-dependent descriptors are [params.c:23–29](../firmware/src/params.c#L23).
Their nominal FREEZE A/B defaults 96/8 and SHIMMER A default 80 are **display
metadata only**: TYPE edits do not install them. Normal preset initialization
uses shared defaults A=64/B=64. Editor canonical metadata still calls A/B
DRIFT/WIDTH even in other modes; hardware labels are dynamic
([params.c:251–264](../firmware/src/params.c#L251),
[editor.c:489](../firmware/src/editor.c#L489)). SHIMMER A displays a raw integer,
not its actual semitone interval.

Actual power-on preset is CLOUD PAD (TRK_DEF preset 4 modulo four presets):
SRC=2, POS=50, SIZE=92, DENS=88, PTCH=0, G-SPRD=40, RAND=14, TONE=100;
TYPE=0, A/B=64/64, CUTOFF=50, RESON=0, STRETCH=0, VCE=POLY,
DTUNE=40, V-SPRD=0. See [engines.c:197](../firmware/src/engines.c#L197),
[G:535–541](../firmware/src/eng_grain.c#L535), and
[ui.c:1389–1401](../firmware/src/ui.c#L1389).

SRC's visible order remains CELLO 0, FLUTE 2, GUITAR 3, SYNTH 13, VOX 14,
then USR1–USR8 at 5–12; IDs 1/4 remain legacy CELLO aliases. Mapping and alias
navigation are [eng_sample.c:64–80](../firmware/src/eng_sample.c#L64) and
[params.c:320–327](../firmware/src/params.c#L320). No IDs shift.

## 3. Findings by personality

### CLOUD

A/DRIFT is never read in CLOUD. B/WIDTH sets spatial spread
`gr_scale(n, B*516)`, replacing G-SPRD's previously calculated spread. G-SPRD
still jitters grain-start intervals by up to roughly ±50%, so it is **not dead**,
but it no longer means spatial spray here. The source head moves continuously
even with WIDTH=0; this is not the frozen-position behavior described in the
opening source comment. TONE and DTUNE are ineffective. Other common controls
have the limitations in the matrix.

### FREEZE

HOLD multiplies grain duration by `(128 + 3*A)/128`, approximately 1…3.98×.
MOTION chooses spatial scatter proportional to `B²*4`, not head velocity.
Neither freezes or captures audio: the same source head continues moving and
STRETCH still affects it. G-SPRD remains timing jitter only. Maximum SIZE/HOLD
at 2× source rate overflows span arithmetic; see §4. All three HOLD values
0/64/127 produced different nonzero PCM in the normal-size test.

### STUTTER

RATE shortens the DENS interval and sets an **independent audio-rate gate**:
`period = 128 + (127-A)*12` samples. This is approximately 26.7…344.5 Hz,
not sequencer/tempo synchronization and not a held-loop repeat length. GATE is
`period*B/127` on-samples; B=0 deliberately produces silence, B=127 is fully on.
The gate is applied before the resonant filter, so a live close can leave a
filter tail. Edges are abrupt; the period changes immediately without smoothing.
Pitch/head/grain spawning otherwise continue normally. G-SPRD affects both
spatial scatter and interval jitter.

### SHIMMER

Each scheduled event first requests a normal grain, then, if B>0, a second grain
at `(7 + A*12/127)` semitones and gain `B*258` in Q15. This is **one additional
pitch layer**, not a bank of harmonics or a pitch-shifting feedback network.
A=0/64/127 requests 7/13/19 st. At root MIDI 60, 44.1-kHz source, PTCH=0,
RAND=0, the 2× rate cap clips intervals above approximately 12 st: A=64 and 127
produce identical renders in the primary sweep. Lower notes move that threshold.
B changes output and available grain capacity; enabling the second spawn also
consumes more random numbers, so it changes subsequent dry-grain choices.

“FROZEN” and “SHIMMER” factory preset names do **not** select TYPE=FREEZE or
SHIMMER. All four preset loads reset TYPE to CLOUD, A/B to 64/64; their stored
engine parameters, envelopes and sends still differ. This was verified by actual
preset loads and [ui.c:1391–1401](../firmware/src/ui.c#L1391). Changing preset
content to select personalities would be a separate approved content change.

## 4. Confirmed defects and numerical evidence

### F1 — CUTOFF zero is silence, not bypass (P1)

[G:508–513](../firmware/src/eng_grain.c#L508) resets three filter-state slots and
returns after grains have been rendered into a local accumulator, without
adding that accumulator to `out`. A clear caller buffer therefore stays zero.
All four 131,072-frame renders at CUTOFF=0 had peak=RMS=nonzero-count=0.
CLOUD/GUITAR at CUTOFF=50 in the same setup had peak 34,286 and RMS 9,705.14
(internal integer units, before track/master attenuation).

Negative controls are HP: −50 maps to ~30 Hz, −1 approaches the top of the
frequency table. Positive controls are LP: +1 approaches ~30 Hz, +50 is ~16 kHz.
Zero is intended to be the dry center but currently acts as a mute notch. Near
zero, HP/LP outputs can legitimately be very quiet. This is distinct from F1.
The actual filter is a linear two-pole trapezoidal SVF with clamped states, not
an independently modeled nonlinear MS-20 circuit
([engines/src/dsp.c:82–110](https://github.com/hugelton/Felucca-Engines/blob/c46e0086f69bdc26df1cd144dca9e683cc4531dd/src/dsp.c#L82),
[gen_engine_tables.py:54–61](https://github.com/hugelton/Felucca-Engines/blob/c46e0086f69bdc26df1cd144dca9e683cc4531dd/tools/gen_engine_tables.py#L54)).

### F2 — Truncated source head and incorrect wrap (P1)

`head[3]` is uint32 Q16. `c << 16` discards source-position bits above 65,535;
`maxhead = (n-3) << 16` discards length bits. Adding `move` can also wrap before
the comparison/modulo, even for lengths below 65,536. See
[G:100–101](../firmware/src/eng_grain.c#L100),
[G:268](../firmware/src/eng_grain.c#L268),
[G:468–473](../firmware/src/eng_grain.c#L468).

| Factory source | Actual frames | Duration at 44.1 kHz | Truncated wrap length `(n-3)%65536` | Its duration |
| --- | ---: | ---: | ---: | ---: |
| CELLO | 176,924 | 4.011882 s | 45,849 | 1.039660 s |
| FLUTE | 125,317 | 2.841655 s | 59,778 | 1.355510 s |
| GUITAR | 73,071 | 1.656939 s | 7,532 | 0.170794 s |
| SYNTH | 275,098 | 6.238050 s | 12,951 | 0.293673 s |
| VOX | 66,150 | 1.500000 s | 611 | 0.013855 s |

These are arithmetic head-domain lengths, **not the length of the audible
recording or proof that every grain stays in that tiny region**. Spray can
place grains elsewhere, and grains read beyond their starting head. Actual
trace over 131,072 frames counted GUITAR 19 head wraps, SYNTH 10, VOX 215.
A synthetic legal-length zone of 65,539 frames makes `maxhead=0` and the head
never advances. Thus full-length assets are stored correctly but scanning is
incorrect. The ADPCM index fix for SYNTH did not fix this separate issue.

### F3 — Live POS and source-position modulation are discarded (P1)

The center is calculated each spawn but immediately replaced by `head >> 16`
unless `head_zl != zl`. `grain_note_on` invalidates that tag; ordinary POS/SHP
edits do not. Three initially identical renders changing POS from 64 to 0/64/127
after block 1,024 remained byte-identical, in all four personalities. Three
renders starting at 0/64/127 differed. Existing grains should reasonably keep
their own read positions; the defect is that **future** grains also ignore the
new center. `gr_reset` clears grains/index state but does not invalidate these
head tags, so a SRC change to a source with the same zone index need not reseed
POS either ([G:159–179](../firmware/src/eng_grain.c#L159)).

### F4 — TONE and CLOUD DRIFT are ineffective (P1)

TONE's only engine definition is the descriptor/preset slot; there is no read of
`P_E7` in GRAIN rendering. DRIFT/A is read only under other TYPE branches.
At values 0,64,127, all three complete PCM files were identical for each dead
control. The nonzero TONE hashes were:

| TYPE | SHA-256 of 131,072 mono int32 samples; equal at TONE 0/64/127 |
| --- | --- |
| CLOUD | `c806c1d6f027dbe68eab5b437e9260ed034fcb23a3703c1daeaa441138cdc4e5` |
| FREEZE | `630199a0bb9eb69b634118c57080ec1d98a3ace2aa8887b6b597023e2ce122fb` |
| STUTTER | `13e41f17e1c3b62289d97e7d6e8b738b690fedd557ae611571858789a49c42de` |
| SHIMMER | `9d84e4a694161ec1298ba76c19992e3dc5f75d6c5e7f930e4268ebb2324d2777` |

CLOUD DRIFT 0/64/127 had the same CLOUD hash. Stored values, UI editing and
MIDI/motion validation work; no rendered implementation follows them.

### F5 — DTUNE/fine pitch does not reach the grain rate (P1)

The common voice code calculates unison fine offsets and `m.inc`, but GRAIN
uses only `m.pitch16`, root and PTCH when making its grain rate. It ignores
`m.fine` and `m.inc`. Retriggered UNISON DTUNE 0/64/127 changed voice fine fields
from 0/0/0 to −28/0/+28 to −56/0/+56, but final stereo PCM remained identical in
all four personalities. CLOUD RMS was 2,282.1195 at every value, with 258,478
nonzero stereo samples. This test had unity master gain and no shared effects.

Fractional global tuning/bend residuals that take the same fine path are also
ignored; the coarse 1/16-semitone `m.pitch16` portion remains functional. This
is a shared-performance-path limitation, not a reason to alter MIDI mappings.

### F6 — FREEZE source-span multiplication wraps (P1)

At SIZE=127, HOLD=127, step=131,072 (2×), the actual duration formula yields
`len=68,703`. `span=(len*step)>>16` in uint32 produces **6,334**, while a wide
product produces **137,406** source frames. The missing 131,072 frames make
source-boundary fitting incorrect. Forward reads stop decoding at EOF and hold
the last sample; reverse reads stop at zero, so this can cause edge plateaus /
wrong grain duration without an observed out-of-bounds access. See
[G:246–260](../firmware/src/eng_grain.c#L246) and
[G:344–375](../firmware/src/eng_grain.c#L344).

### F7 — Seek-index capacity is smaller than valid user storage (P1)

The fixed part index holds 1,075 entries of 256 frames: at most **275,200 frames
in aggregate**, including zone rounding. `gr_reset` assigns zero capacity to
any zone that would exceed the remaining entries; `gr_spawn_one` silently
refuses it. A synthetic 275,200-frame zone rendered nonzero; 275,201 frames had
`zcnt=0`, peak=0 in every personality. This need not violate the 240-KiB sample
pool: 275,201 ADPCM frames need only 137,601 audio bytes. Multiple individually
small zones can also exhaust the shared per-part index. Header validation has
no GRAIN index-capacity constraint
([eng_sample.c:172–190](../firmware/src/eng_sample.c#L172)).

All five current factories individually fit the index, including SYNTH's 1,075
entries. This finding concerns accepted larger/multi-zone user sources; it is
not a request to alter the allocator/journal or shorten recordings.

### F8 — Short-grain window can be identically zero (P2)

Grain length is fitted to `n-3`, and lengths below 16 output frames are refused.
For an accepted length **16–32**, `gr_run` sees Hann=0 at start and forces the
end to zero within the same CTL=32 call. Its linear interpolation between those
two endpoints is zero for the entire grain. Synthetic zones with 19,34,35 frames
at 1× produced no output; 40,64,65 frames produced nonzero output. All four
personalities reproduced this boundary. Samples below the minimum length also
legitimately fail to spawn; that is a separate explicit limit.

### F9 — Resonant amplitude path can overflow (P1, bounded-input robustness)

The SVF input is clamped to ±65,535, but the resonant output is not clamped before
`voice_amp`. With resonance 127, a bounded sine at the coefficient's center
frequency, cutoff-table index 64, and unity envelope, a strict UBSan harness
reported:

```text
firmware/engines/src/dsp.c:8:64: runtime error: signed integer overflow:
69636 * 32767 cannot be represented in type 'int'
```

`mulq15(a,b)` uses a signed 32-bit product. A separate output-observation pass
without consuming the amplitude result reached filter peak 154,082. The probe
exercised the **exact shared filter + amplitude helpers with an admissible
input**, not an uploaded source or complete factory-grain render. Therefore the
arithmetic vulnerability is confirmed; its reachability/severity with normal
factory presets is not established. Forty-eight stock-source stress renders
with strict signed-overflow checking did **not** fail. Do not label this a
stock-preset crash or suppress it with the usual DSP overflow exclusion.

### F10 — Earlier final-audio identity evidence was vacuous (P1 test defect)

[tests/grain_only_audio_test.c:19–42](../tests/grain_only_audio_test.c#L19)
uses `ui_power_on`, which zeroes `song` and does not set master gain
([ui_test.c:183–219](../tests/ui_test.c#L183)). The mixer multiplies its output by
`song.master_q12` ([fx.c:1048](../firmware/src/fx.c#L1048),
[fx.c:1091–1096](../firmware/src/fx.c#L1091)). The earlier captured
14,336,000-byte file contained **zero nonzero bytes**. The independent per-part
level assertions were still meaningful, but output-byte equality did not prove
an audible regression check. This report corrects that earlier evidence.

A new external mixer probe sets `song.master_q12=4096`, requires/observes nonzero
output, and compares 44ec460 against the previously prepared exact 68c3229
implementation at identical state. All four personalities matched byte for
byte: four files × 1,048,576 bytes = **4,194,304 nonzero-containing PCM bytes**.
RMS values were CLOUD 2,282.1195, FREEZE 1,350.8553, STUTTER 1,416.4623,
SHIMMER 1,822.8140. This is a valid supplemental policy-change regression
comparison, not a claim that all engine states were exhaustively compared.

## 5. STRETCH, stereo and playback safety assessment

### STRETCH: descriptor correct, scanner limited

[params.c:240–258](../firmware/src/params.c#L240) supplies engine-specific ID 108,
0–127, default 0 to runtime loads, editor and validation. The re-run descriptor,
project, MIDI/motion and editor policy tests pass. No new compatibility defect
was found in that path. Older firmware can still clamp 108 on downgrade, as
already documented; do not rewrite old saves.

At root MIDI 60 with RAND=0, grain `step` stayed **65,536** for STRETCH
0/1/64/127, while the source-head increment per 32-frame block was:

| Raw STRETCH | `pow2_q16(-v*10)` | Head movement, Q16 / block | Source frames / output frame |
| ---: | ---: | ---: | ---: |
| 0 | 65,549 | 2,088,960 | 0.996094 |
| 1 | 63,225 | 2,007,360 | 0.957184 |
| 64 | 6,503 | 204,000 | 0.097275 |
| 127 | 668 | 16,320 | 0.007782 |

Thus STRETCH changes source traversal **without changing the individual grain
rate**, and all four values produce distinct nonzero audio. It neither changes
grain length nor performs phase-locked reconstruction of the original signal.
It does not provide a hard freeze at maximum. Truncating both rate factors to
8 fractional bits introduces about 24% additional speed error at the maximum
setting relative to using factor 668/65536 at unity source rate. Head rate is
also pitch-dependent below unity: at PTCH −12 the STRETCH=0 increment is
1,048,576, versus 2,088,960 at PTCH 0 **and +12** because `hs` is capped at
65,535. Therefore it is not fully pitch-independent time stretching. F2's wrap
error dominates long-source behavior. Raising the decoder-rate ceiling or
adding a new stretch algorithm is not part of the descriptor repair.

### G-SPRD and V-SPRD are independent, with different limitations

A deterministic first-spawn trace at G-SPRD 0/64/127 gave the same CLOUD read
position (35,193 after decoder setup) and the same FREEZE position (36,014).
STUTTER/SHIMMER positions differed: 36,823 / 35,193 / 33,588. This confirms the
spatial override, not merely an inferred code read. Timing-jitter renders were
distinct in all four modes.

V-SPRD 0/64/127 produced three different **final stereo** signals in each
personality. CLOUD summed absolute L−R difference was 0 / 136,607,992 /
271,801,258. It acts on whole voices after granular mono rendering, not on
individual grains. A single POLY voice is panned to its assigned side; it does
not become a stereo grain cloud by itself. MONO/LEG disable spread intentionally;
three-voice UNISON has a two-versus-one side distribution. Mono effects sends
and the level change from the existing linear pan law remain as before.

### Spawning, envelope, boundaries, isolation and cost

* A part has 12 grain slots, three allocated voices maximum; four parts share the
  eight-voice budget. At most 48 grain structs exist across parts. Pool shares
  depend on active voices. SHIMMER can request **two** grains per event, so the
  opening comment claiming at most one start/voice/block is stale. The second
  request can exceed the nominal per-voice share by one; the physical pool and
  decode-load checks still apply.
* DENS schedules after rendering the current block. Changing DENS does not
  immediately replace its countdown. Pool exhaustion, load cap, unbuilt seek
  entries or too-short grains skip starts; the countdown still advances. Output
  density can be lower than the displayed request. `GR_NORM` normalizes expected
  overlap, not the number that actually succeeded, so skipped starts can reduce
  level. No allocation, lock or blocking storage I/O occurs in the renderer.
* SIZE's base curve is about 10…392 ms at 44.1 kHz; FREEZE extends this by up to
  ~3.98× before fitting. The envelope is a Hann lookup interpolated **between
  control-block endpoints**, not a sample-exact Hann. Large grains are cheap;
  F8 shows why very short grains need a separate treatment.
* Forward grains decode on demand from an indexed ADPCM predictor/step state;
  reverse grains use 128-sample windows. Forward EOF holds the last decoded
  sample, reverse reads stop at sample zero. Ordinary spawn placement keeps a
  two/three-sample margin, but F6 invalidates the span proof. The ASan edge and
  reverse probes found no invalid access. No anti-alias resampler exists beyond
  linear interpolation; 2× source stepping can alias, and the hard cap flattens
  high-note/PTCH/SHIMMER changes.
* GRAIN loops its own head through the whole `n-3` domain; it does **not** use
  source `ls/le/looped` or loop predictor metadata. Grain windows are fitted,
  not loop-wrapped. This is distinct from SAMPLE's loop playback. CELLO's
  verified metadata is preserved but its loop semantics are not adopted by
  GRAIN; the other four sources retain documented unresolved musical root/loop
  metadata and MIDI-60 audition anchors ([factory-samples.md:35–41](factory-samples.md#L35)).
* A source with no data uses a sine fallback, not silence. A user source with no
  matching key zone ends that voice. Overlapping user zones use the **last**
  matching GRAIN zone, whereas SAMPLE chooses the narrowest; this is a documented
  implementation difference to test, not evidence of corrupted metadata.
* `gr_build` builds one 256-frame seek entry per part per render block, with
  sounding zones prioritized. At CTL=32, filling the remaining 1,074 SYNTH
  entries takes roughly 0.779 s of rendered time. First sound can occur earlier
  if its requested entry is ready; sparse DENS can add another countdown delay.
  The old “1 s zone in ~60 ms” comment is not the current CTL=32 estimate.
* Each part has separate grain pool, head/index arrays, reverse windows and RNG
  (`gr_part`, `gr_of`, `gr_rng`; G:91–123). Host measured `sizeof(gr_grain_t)=64`
  and `sizeof(gr_part_t)=7,288` bytes (64-bit pointer host). Target sizes may differ.
  Current reserved engine region is 12,888 bytes/part, 51,552 bytes total
  ([engines.c:7–22](../firmware/src/engines.c#L7)). No new memory was added here.
  RNG storage is separate per part but starts with the same seed, so identical
  parts can begin with correlated random sequences; this is not shared-state leakage.
* `GR_LOAD_MAX` counts forward stepping and weights reverse by two. It bounds
  the admitted weighted rate, **not an exact instruction or worst-case decode
  budget**: reverse refills also seek up to 255 samples, and SHIMMER doubles
  possible spawn/seek bursts. The legacy 1,050-instruction comment and current
  CPU2 heuristic of 189 cost units/voice are not new target measurements.
  Measure all-four-track cold-cache, reverse and SHIMMER cases on hardware.
* Input accumulator clamp and SVF integrator limits avoid unbounded filter-state
  growth, but do not prove amplitude arithmetic safe (F9). A coefficient-grid
  bound check produced intermediate maxima 1,597,388,025 and 1,822,617,210,
  within int32 for clamped states/input. That is a checked grid, not a formal
  proof over all coefficients/transients. Unsigned head/span wrap is defined C
  behavior and therefore invisible to ordinary UB checks.
* Potential clicks: abrupt SRC cache reset, unsmoothed filter sign/value changes,
  STUTTER hard gating/period retargeting, and source/end plateaus under F6.
  A large interpolated grain has smooth amplitude endpoints, but no listening
  or discontinuity spectrum test was done. These are quality risks, not confirmed
  buffer faults. Filter state is retained in voice slots across some retriggers;
  declick behavior should be tested before deciding to reset it.

## 6. Prioritized improvement plan — proposals only

Resource estimates below are engineering estimates, **not linked byte counts**.
No fix below is implemented. Keep all stable IDs/ranges/formats and original
WAVs. Any altered sound at the same saved parameter values needs release notes.

| Priority / item | Smallest proposed change and expected result | Risk and estimated CPU/RAM/flash | Specific regression strategy |
| --- | --- | --- | --- |
| P0 | None confirmed; retain memory/target stress as a release gate | No safety certification inferred from host success | ASan boundaries, malformed sources, real-time four-part target profiling |
| P1 F1, bug fix | `eng_grain.c` zero branch emits clamped dry `acc` through the existing amplitude ramp before returning; keep intentional filter reset | Previously silent saved CUTOFF=0 becomes audible. O(n) gain work only in that branch, no new state; small code addition | All types, factory/user/fallback, envelope ramps, nonzero output, existing output accumulation, CUTOFF −1/0/+1 and held-note switching; nonzero cut renders byte-identical |
| P1 F2/F3, bug fixes | Replace overflowing head representation with sample-index + fraction or wide Q16; wrap before addition can overflow; track/apply POS/SHP changes to future spawn centers without restarting existing grains. Invalidate tags on SRC/zone changes | Scanning/saved sounds change substantially. Split representation adds roughly 6–12 bytes/part before alignment; wide Q16 about 12–16 bytes/part. Both fit current engine union on the measured layout; wide divide cost requires target profiling | 65,535/65,536/65,539/275,098 frames, all factories, live POS both directions, SHP LFO/ENV, SRC same-zone changes, no out-of-range reads, correct complete traversal at stretch extremes |
| P1 F6, bug fix | Compute grain source span with a wide product or exact split multiply before fitting | One wider multiplication per spawn, no new state, small code cost; long FREEZE boundary sound changes | SIZE/HOLD min/mid/max, rates min/1×/2×, forward/reverse, short/end regions; compare span to 64-bit reference |
| P1 F5, bug fix | Apply the existing `m.fine` correction to grain playback rate before the existing cap; do not change global MIDI/bend math | A few integer operations/spawn, no new state; UNISON/global tuning sound changes. Decide explicitly whether held-note DTUNE is to retune immediately or on retrigger | Three unison voices show distinct rates at DTUNE 1/64/127; DTUNE 0 byte-identical; fractional tuning/bend; every personality; retain pitch limits |
| P1 F9, arithmetic fix | In the GRAIN output path, enforce amplitude-helper input headroom or use a safe wider multiply locally; do not silently rewrite shared `mulq15` for every engine | Local clamp is cheapest but clips resonance; wide gain preserves more signal at extra multiply cost. No new state; small code. Select sonic policy before implementation | Strict signed-overflow-on test with bounded resonant sine/impulses, high-amplitude sources, both HP/LP, maximum envelope, stock-preset comparison; verify no wrap distortion |
| P1 F7, playback/diagnostic fix | Determine a documented GRAIN source-capacity policy; preferably adapt seek spacing to source total while retaining a fixed-size index, rather than rejecting otherwise valid storage or truncating audio | Variable spacing raises worst-case seek cost. A larger fixed index alone needs 3 extra bytes/entry/part and still needs a proven worst-case user-zone bound. No allocator/journal change | Maximum single extent, 16 zones and rounding, exact capacity ±1, no silent accepted zone; target spawn cost and source-change startup |
| P1 F10, test-only fix | Set a nonzero master in final-output regression fixtures and require nonzero RMS/peak before comparing bytes | No firmware cost; may expose genuine formerly hidden failures | Fail deliberately if master is zero; channel solos and full mix, all personalities, known gain and effects state |
| P1 F4 TONE, missing implementation requiring sonic approval | Add the intended light one-pole LP before the personality SVF, using the existing SAMPLE-style coefficient/modulation convention; raw 127 explicitly bypasses it. Existing unused legacy `s[2]` state is a possible reuse after auditing filter-state ownership | Restores a promised but absent control, yet is a **new audible synthesis implementation**, not an already-working algorithm repair. About one multiply/add per sample, no new RAM if safely reusing state, otherwise 4 bytes/voice; small code, target size unmeasured | At 127 preserve current audio exactly; min/mid progressively reduce high-frequency energy; all types, fallback, smoothing/transitions, independent CUTOFF/RESON; verify preset tone values now change sound |
| P1 F4 DRIFT, missing behavior requiring specification | Define DRIFT before coding. A minimal candidate is CLOUD-only scan-rate scaling with raw 64 neutral, 0 stopped and 127 faster, multiplied by existing STRETCH factor | This is a **new behavior**, not recovery of a hidden existing algorithm. A few operations/block, no state necessary; non-default saved DRIFT values change sound. Could overlap STRETCH, so obtain interaction approval | Neutral 64 byte identity, zero hold, rate monotonicity, independence from WIDTH/G-SPRD, other types unchanged, source boundaries and all tracks |
| P2 F8, bug fix | Evaluate Hann within the single short block for accepted ≤32-frame grains, keeping the existing endpoint interpolation for longer ones | Up to 32 window evaluations per short grain; no persistent state, small code | Lengths 15/16/31/32/33/64, steps 0.5/1/2, both directions, actual nonzero decoded source, envelope energy and bounds |
| P2 scaling/caps | Expose/document real SIZE/DENS/INTERVAL/STRETCH meanings without changing stored raw IDs; either document the pitch ceiling or separately budget a higher-quality/rate decoder | UI formatter/code only if documenting; raising caps/resampling is a different CPU-heavy feature and cannot be smuggled into a bug fix | Raw/UI mapping monotonicity, exact semitone/rate reference, CPU admission limits, alias spectra; preserve MIDI and saved bytes |
| P2 metadata/presets | Display personality-aware browser aliases while keeping stable library keys; document conditional defaults and named presets before any content change | No DSP cost or bitmap; small frontend code. Renaming canonical keys or installing defaults on TYPE would threaten compatibility and is not recommended | Switch all personalities, metadata GET/DESC, old library keys/backups, no parameter writes merely from changing display labels |
| P2 common modulation/quality | Decide whether FLT should affect TONE or CUTOFF and specify head/SHP interaction; consider short smoothing for gate/filter/source transitions only after bug fixes | Modulation binding is an audible feature change; one-pole/ramp state typically a few words/voice. Measure CPU/code; preserve current neutral state | ENV/LFO/MOD destinations, controlled spectra, pitch/position independence, transitions/click metrics, no new audio-thread allocation |
| P3 optional refinements | Per-part RNG seed decorrelation, personality-aware factory patches, calibrated display units, clearer “extra interval layer”/“extended grains” terminology | Optional audible/preset/UI changes; no large assets required | Reproducibility, neutral-state comparison, explicit preset review and target size/timing checks |

## 7. Recommended smallest first implementation slice

Approve **F1 plus F10 only** first: repair the zero-cut dry path and strengthen
actual-audio assertions so they cannot pass on silence. This is a small,
well-defined bug fix with no parameter, source, UI or persistence changes.
Keep every nonzero-CUTOFF output byte-identical. Explicitly document that old
projects using zero as an accidental mute will now sound.

Next, repair F2/F3/F6 together as a bounded source-position/boundary slice, with
a deterministic reference model. Schedule the locally scoped F9 headroom repair
and DTUNE correction before claiming audio robustness. Do **not** bundle TONE,
DRIFT, true freeze, new stretch algorithms, higher decoder caps or new presets
into these repairs: their missing semantics/sonic changes need separate review.

## 8. Tests executed, reproduction and limitations

Only `docs/grain-dsp-audit.md` is intended for the audit commit. All disposable
harnesses, PCM and logs are outside the Git worktrees at
`/workspace/work/grain-dsp-audit-evidence`. The existing implementation worktree
remained clean. Prepared existing sanitizer binaries from the same validated
implementation were re-run; the new audit probes were independently compiled
against the exact 44ec460 sources. No baseline fixtures/goldens were weakened. Native probes used Debian GCC
14.2.0 with `-O1 -ffp-contract=off`; the engine submodule is pinned to
`c46e0086f69bdc26df1cd144dca9e683cc4531dd`.

### Existing tests re-run

| Suite | Configuration | Result |
| --- | --- | ---: |
| GRAIN-only selection/project policy | ASan/UBSan, shipping policy | 380 passed |
| GRAIN-only editor/backup policy | ASan/UBSan, shipping policy | 70 passed |
| Physical NOR persistence | ASan/UBSan, shipping policy | 24 passed |
| Four-channel level/pan/spread fixture | ASan/UBSan, shipping policy | 82 passed; **final-output silence caveat F10** |
| Theme/shared sequencer | ASan/UBSan, shipping policy | 8,229 passed |
| Grid rendering/value/bounds | ASan/UBSan, shipping policy | 1,935 passed |
| Grid routing, MIDI learn, motion/locks | ASan/UBSan, multi-engine diagnostic mode | 2,329 passed |
| STRETCH canonical descriptor/load/MIDI/motion | ASan/UBSan, multi-engine diagnostic mode | 1,764 passed |
| FUN11/FUN10 format/corruption | ASan/UBSan, multi-engine diagnostic mode | 41,146 passed |
| Factory source integrity/reproducible ADPCM | Existing Python test | Five checksummed originals, full-frame encoding verified; changed/missing originals rejected |

Diagnostic mode permits backend cross-engine assertions; it is not the shipping
selection policy. Existing broad Motion/MIDI/web failure counts from the prior
milestone were **not reclassified as passing or re-run as part of this DSP
audit**. Their previously documented baseline is not new DSP evidence.

### New disposable experiments

* **176 direct renders**: four personalities, min/mid/max of POS/SIZE/DENS/PTCH/
  G-SPRD/RAND/TONE/STRETCH/A/B/RESON, seven bipolar CUTOFF points, and four
  source choices. Each has 131,072 mono int32 frames (~2.972 s), deterministic
  RNG, full seek-index prebuild, fixed MIDI 60, unity envelope and SHP center.
  These isolate renderer controls; prebuilding deliberately excludes startup
  latency. SRC test choices were 0/3/13/14; FLUTE was additionally rendered in
  the separate all-five-source trace. All exited normally.
* **88 supplementary renders**: unity-master mixer tests of VCE/DTUNE/V-SPRD/
  common FLT modulation; live POS changes after block 1,024; all five source
  head domains; STRETCH 0/1/64/127 with RAND=0; maximum FREEZE cases. All exited
  normally. Single-note POLY/MONO/LEG equality is expected, not evidence those
  modes are broken; their allocator/retrigger code was inspected separately.
* **Fresh ASan/UBSan probes**: 24 direct-render scenarios, four unity-master
  unison mixer runs, and 52 short/index-boundary cases (four types × 13 source
  lengths). No address/standard configured UB sanitizer report. Established
  exclusions were `signed-integer-overflow,shift,bounds,object-size,pointer-overflow`
  for existing DSP wrap/host XIP semantics; these exclusions limit that claim.
* **48 strict signed-overflow-enabled renders**: all four types, twelve nonzero
  bipolar cutoffs, maximum RESON and SIZE, stock GUITAR. Only shift checks were
  disabled; signed arithmetic checks remained active. All exited normally.
* **Separate strict filter-range probe**: bounded sine ±65,535, resonance 127,
  table cutoff 64, unity amplitude; consumed gain result stored to a volatile
  sink to prevent optimization. It **failed as expected at signed `mulq15`
  overflow**, F9. This is not hidden under the ordinary DSP exclusions.
* Deterministic internal traces checked first spawn positions, head increments,
  exact duration/span arithmetic, conditional defaults/presets and coefficient
  ranges. Supplementary 44ec460-versus-68c3229 final audio comparison used four
  nonzero unity-master files, all identical, as detailed in F10.

The core direct-probe setup is reproducible without changing production code:

```c
#define UI_TEST_NO_MAIN 1
#include "/absolute/path/to/exact-44ec460/tests/ui_test.c"
/* In main, one process per case: */
ui_power_on();
track_t *t = &trk[0];
host_preset(t, ENGI_GRAIN, 0);
t->engine = t->eng_req = ENGI_GRAIN;
memset(t->v, 0, sizeof t->v);
/* Override E0..E7 = {source,64,80,90,0,40,30,127};
   TYPE=case, A=B=64, CUTOFF=50, RESON=0, STRETCH=0;
   then override the parameter under test. */
gr_part_t *p = gr_of(t);
memset(p, 0, sizeof *p);
gr_rng[0] = 0x2545F491;
grain_block(t);
for (unsigned i=0; i<GR_NIDX; i++) gr_build(p,t);
voice_t *v = &t->v[0];
v->note=60; v->active=1;
grain_note_on(t,v);
vmod_t m={0};
m.pitch16=960; m.shape=64<<8; m.amp0=m.amp1=32767;
/* 4096 iterations: clear int32_t out[CTL], grain_block(t),
   grain_render(t,v,out,CTL,&m), save samples + RMS/peak/hash.
   Live tests change POS only after iteration 1024.
   Mixer tests instead use trk_note_on + mix_block, master_q12=4096,
   effects off, maximum sustain, and assert nonzero final output. */
```

Compile external probes from the prepared implementation worktree:

```sh
cc -O1 -w -ffp-contract=off -Ibuild/gen -Ifirmware/src \
  /outside/repository/probe.c -lm -o /outside/repository/probe
# Existing tests are run without changing their source or expected values.
# Fresh standard probes add -g -fsanitize=address,undefined and the
# established exclusions above; the strict numeric probe excludes shift only.
```

Evidence manifest (SHA-256 of the temporary structured result files):

| File | SHA-256 |
| --- | --- |
| `sweep.json` | `f6ebe045f3b6c510a867a4eb75b62ca64616761e86337596b3fb87c1366a0c4c` |
| `extra.json` | `1353c429792b5c12ea2bdb9dc4bdbc4fa33f3b2ed902ae74f24ae1a6390d14b2` |
| `numeric.json` | `34840ed8f8845db69d74a4154e5bc40786b4423a9605172e1e1fdd42e84e971c` |

These external files are local evidence, not portable release artifacts or part
of the documentation commit. Result tables, exact formulas and key failing
inputs are preserved in this report so the findings do not depend on those
files remaining available.

### Remaining limits

No source WAV was trimmed, normalized, resampled or substituted. No DSP fix,
new feature, baseline update or storage migration was implemented. No new
bitmap, framebuffer, firmware RAM or firmware code bytes are added by Markdown.
Emscripten and JieLi target toolchains remain unavailable in this environment;
no dependencies were installed. No new WASM or target link, instruction-count
profile, hardware flash measurement or real-time deadline validation was
performed. The 581,564-byte image limit remains an **unverified release gate**.
