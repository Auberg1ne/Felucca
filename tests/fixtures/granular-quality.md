# Step 237 deterministic granular baseline

Base: `49e7698c646b1b7395b4c7059fd1e8274ce67df4`.
Only tests/reference data are added. No DSP, UI, IDs, assets, storage or build
scripts change. This is a host oracle, not an FM1 timing or hardware-fit claim.

## Reproduce

From the repository root, with the existing generated headers and pinned engine
submodule available:

```
python3 tests/run_granular_quality.py
python3 tests/run_granular_quality.py --sanitize --out /tmp/grain-quality
```

The default command verifies; it never updates references. `--freeze` explicitly
creates the manifest and is restricted to the exact base HEAD with unchanged
firmware/web/build scripts. Do not re-freeze after a DSP change merely to pass.
Captures and executables are temporary unless `--out` is given.

Compiler flags are O1, ffp-contract=off, default shipping GRAIN-only configuration.
The existing ui_graph.c implicit declarations require
`-Wno-implicit-function-declaration` with this host compiler. No firmware source
is patched. Generated input hashes are pinned, including the 380334-byte factory
sample header; the compact target profile is NOT substituted into this oracle.
Golden portability is qualified on this Linux x86-64 host, not every compiler,
architecture, target asset profile or future generated-header revision.

## Coverage and frozen data

`granular-quality.json` stores SHA-256 PCM hashes, FNV-1a explicit state hashes,
nonzero/peak/RMS evidence, RNG draw counts, admissions and resource maxima.
It pins 47 generated/historical inputs by SHA-256, without duplicating binaries.
PCM is explicitly serialized signed int32 little-endian stereo at 44100 Hz,
65536 frames per case. Each case is a fresh process with seeds
`0x12345678 + track * 0x10203`.

Four types each run six cases: low/high density, minimum SIZE, high RAND/reverse,
LFO filter/position modulation plus manual bipolar sweeps, and four simultaneous
mixed-personality tracks. The latter rotates all four personalities among all
four tracks. Sources are representative CELLO(0), FLUTE(2), VOX(14), SYNTH(13).
These are controlled states, not assertions that the factory templates have
identical parameter defaults.

RNG consumption is recovered by advancing the existing xorshift from each
block's prior state to its observed final state (bounded to fewer than256 draws).
State hashes include scheduling countdowns, source heads, grain phase/lifetime,
playback steps, ADPCM predictor/index, reverse caches and filter-transition state.
Pointers and structure padding are excluded. New admissions are observed as
owned grains with zero window phase after rendering. Decode load is the engine's
`step << reverse` admission metric; this is not a measured CPU cycle count.
Every block asserts at most12 active grains and load at most `8 << 16`.

The personality row remains `{103,104,105,255}`. Labels/ranges are checked for
all four types. Full mapping/navigation tests are also covered by the existing
WIN I3A UI suite.

A separate held-source kernel probe covers durations16..128, all32 block start
offsets and both directions: 7232 cases per process. **306 are silent** with the
legacy endpoint ramp. This is deliberately frozen, not repaired or excluded.
The zero-rate held-source probe isolates envelopes; actual reverse ADPCM playback
is independently exercised by the sounding captures. Minimum admitted duration
and the unavoidable single-frame convention remain unchanged.

Storage snapshots pin current FUNC/3840-byte projects and version6/192-byte
normal presets for every type. They preserve an historical unused ID8 value
across project import, and test count-based common/engine mapping for counts
8..118 without modifying inputs. The runner additionally executes the existing
5804-check WIN persistence/NOR gate and user-preset suite against unchanged
historical fixtures: earlier projects, retained schemas, versions1..5 presets,
normal/grid formats, reserved109 provenance, future-format refusal and no-write
protections. Existing binaries are never regenerated. Current grid-version7
geometry is asserted; its behavioral coverage remains in those existing suites.

## Validation at creation

- New suite: all24 sounding captures plus an independent-process repeat pass.
- Ordinary and ASan/UBSan runs match every frozen PCM/state/storage value.
- Mixer-path sanitizers exclude signed overflow and shift, matching intentional
  legacy DSP arithmetic; bounds/object/pointer checks remain enabled. This is
  not a claim that all inherited mixer arithmetic passes strict UBSan.
- Separately, the existing filter-transition suite passes strict ASan/UBSan with
  no exclusions: 3553312 checks, worst first transition edge0.
- RNG draws per capture:15..2325; admitted grains:5..301; reverse admissions
  across24 captures:272; maximum observed decode-load metric:524288 (the cap).

Existing suites run in the shipping configuration:

- PASS: grain_win_i3a_ui_test (62025), grain_filter_transition_test (3553312),
  grain_filter_response_test (612), grain_only_test (380),
  grain_only_editor_test (70), grain_only_persistence_test (24),
  grain_position_test (854920), grain_tone_test (722655),
  grain_drift_test (2315750), grain_dtune_test (75537),
  grain_win_persistence_test (5804), project_downgrade_bridge_test (2189),
  project_format_test (41146), grain_cutoff_zero_test (2355), upreset_test.
- Existing failures: editor_test has11 failed assertions before SIGSEGV;
  motion_test has30 failed assertions; robust_test has3; grain_stretch_test
  stops at line39's non-GRAIN descriptor assertion. midi_control_test does not
  compile in shipping mode because ANALOG_PRESETS is absent.
- Diagnostic `FELUCCA_GRAIN_ONLY=0`: motion has27 failures, MIDI control has15.
  These diagnostic failures are reported separately, not hidden by changing
  shipping flags.

The existing failed harnesses and all firmware are identical to the exact base;
archived base sources were also used to reproduce the shipping failures with
the same generated headers/submodule. No existing test assertions or goldens
were edited. This was focused regression validation, not the entire host runner.

## Use for subsequent work

Suitable as the pre-change short-envelope baseline: compare legacy/default
PCM and state exactly, then explicitly review intentional envelope-output changes.
Retain RNG, admission, decoder/head and storage identity as separate invariants.

Gaps: no target timing/CPU2 race qualification, no exhaustive factory-source or
user-source matrix, no exact timestamp hook for failed spawn attempts, and no
complete per-sample scheduling trace. The silent-grain count is an envelope-only
probe, not an estimate of musical silence frequency. Additional GRAIN/WIN stress
suites remain necessary before promoting a renderer or new personality control.
