# Short fitted-grain envelope correction (Step 238)

Base: d615ab70730d97f52237f541989a0ff02561326f, branch
`feat/coffee-grains-granular-quality`. No parameter, format, sample, UI, filter,
pool, scheduler or admission changes.

## Cause and correction

The old `gr_run` samples the window only at a block's endpoints. An admitted
16..32-frame grain can begin and end in the same block. Both endpoints are
zero, so the entire gain ramp is zero even for a constant nonzero source.
The frozen Step237 probe reproduces 306 silent cases out of7232.

The original kernel is retained as `gr_run_block`. `gr_run` dispatches to it
unchanged for grains whose original duration exceeds32 frames. Only original
durations <=32 use at-most-four-frame fragments. The immutable phase increment
(`winc >= UINT32_MAX / 32`) selects this policy; remaining lifetime does not.
Thus long grains' short tails do not unexpectedly change renderer.

No duration divide, floating point, heap, persistent state or second decode is
added. Each source sample is decoded/interpolated once; the kernel's predictor,
index, phase, source position and reverse cache continue between fragments.
At CTL32 there are at most8 fragment calls per render. A four-frame ramp uses
`(w1-w0)*256`, exactly the old `(w1-w0)*1024/4`, also for negative slopes.
This avoids a runtime divide for complete fragments. Partial fragments retain
the old rounding. All products remain within the existing legal Q15 bounds.

This is a narrow short-grain fix, not promotion of the experimental hybrid
renderer. Windows, WIN latching, normalization and all personalities are intact.
Minimum admitted duration remains16; no claim is made for unadmitted1-frame
windows or intentionally silent samples, gates or envelopes.

## Tests and intentional PCM changes

`tests/grain_short_envelope_test.c` covers1851392 held-source cases:
durations16..128, all32 first-block offsets, both directions, all128 production
WIN values and positive/negative full-scale source samples. It verifies:

- Hann silent cases306 -> **0/7232**; no silent corrected held-source case.
- 278528 short renders intentionally change; every >32-frame render remains
  byte-identical to the original block kernel, including its tail.
- Full grain/part decoder state matches after every block, including real
  forward/reverse ADPCM tests over all5 factory sources,3 rates, variable block
  lengths and reverse-cache refills (1350 additional cases).
- Actual admission/span fitting produces sounding16..32-frame user grains for
  all4 personalities and all4 track identities (272 additional cases).
- Output/state canaries hold. Constant-source output stays within signed16-bit
  bounds. Short-window maximum sample jumps are bounded by the per-sample
  window reference's maximum jump plus8 integer units.
- Aggregate envelope squared error versus per-sample evaluation decreases
  from5600216864267332 to4037394206439142 (27.9065%). This is a numerical
  fidelity measure, not a subjective sound-quality claim.

The final gate passes **152924099 checks**, normally and under strict
ASan/UBSan with no exclusions on this kernel/fitting path.

All24 Step237 final-mixer PCM hashes remain identical. RNG draw counts,
scheduling/admission/load observations and decoder/head state hashes also
remain identical. These representative mixer captures do not contain the
problematic fitted short grains; the exhaustive and fitted-source gates above
exercise the intentional change directly.

The JSON golden is NOT regenerated. Its embedded short-window hash and306
count deliberately remain a legacy oracle: the runner selects `gr_run_block`
for that historical probe only. Actual mixer rendering still uses corrected
`gr_run`, and the runner now mandatorily executes the new non-silence/state
gate. The correction is not disabled to obtain fixture identity.

Filter-transition strict ASan/UBSan:3553312 checks, worst first edge0.
Bipolar direction/frequency-response suite:612 checks, pass. No filter code
changed. The complete mixer fixture sanitizer run retains its documented
historical signed-overflow/shift exclusions; do not confuse that with the
new kernel's strict, exclusion-free run.

Twenty relevant existing shipping host suites were rerun:15 pass. The same
5 baseline issues remain: Editor11 assertions then SIGSEGV, Motion30 failed
assertions, robustness3, STRETCH's line39 descriptor assertion, and MIDI's
shipping-mode compile error (`ANALOG_PRESETS` absent). These match Step237;
no existing assertion or unrelated golden was modified. This is focused
regression coverage, not a claim that the entire historical host runner is green.

## Reproduce

With existing generated headers and the pinned engine submodule:

```
python3 tests/run_granular_quality.py --out /tmp/short-normal
python3 tests/run_granular_quality.py --sanitize --out /tmp/short-sanitized
cc -O2 -w -Wno-implicit-function-declaration -ffp-contract=off \
  -Ibuild/gen -Ifirmware/src tests/grain_short_envelope_test.c -lm \
  -o /tmp/short-bench
/tmp/short-bench --bench > /tmp/short-bench.csv
```

The runner checks unchanged input hashes, current storage snapshots, the5804-
check persistence/NOR gate, user presets and an independent-process repeat.

## Host resource/performance evidence

Same compiler/flags and Step237 harness, `-Os -fstack-usage`:

- `.text` +153 bytes; `.rodata`, `.data`, `.bss`:0 change.
- GNU `size` text aggregate507461 ->507698 (+237, including unwind metadata).
- Grain/part host structures remain64/7296 bytes; reserved engine pool unchanged.
- Original kernel frame120 bytes; short dispatch frame56; grain_render frame248
  unchanged. Worst short call chain adds56 host stack bytes. Target stack differs.

O2 held-source kernel benchmark, median of5 repetitions over windows/directions:

| Frames | Old ns/frame | Corrected ns/frame | Ratio |
|---:|---:|---:|---:|
|16|2.379|3.370|1.417|
|32|1.542|2.792|1.810|
|33|2.354|2.377|1.010|
|441|1.648|1.634|0.992|
|4410|1.580|1.593|1.008|

These include harness consumption overhead and use a zero-rate held source to
isolate envelope work. They are comparable host CPU-time measurements, not
instruction counts or FM1 timings. A full short32-frame call evaluates at most
16 window endpoints versus2 previously (intermediate morphs need2 tables per
evaluation); ADPCM work does not multiply. The conservative48-grain four-track
bound is384 fragment calls/768 evaluations per CTL32 callback, without relaxing
GR_NG12, GR_POLY3 or GR_LOAD_MAX8Q16.

The existing four-track mixer benchmark was also run against the pre-fix
firmware and corrected firmware:120 captures, up to48 active grains, all
scheduling hashes and peak/RMS results equal. Median host time157.927 ->157.081
ns/frame, within host timing noise. That workload uses normal-length grains;
it does NOT qualify worst-case short-grain FM1 callback headroom.

No usable JieLi target compiler was found in this environment. No installation,
download, target link or hardware timing measurement was performed. Actual FM1
flash/stack fit and short-grain callback headroom remain unverified. Saved sounds
that previously lost short fitted grains can intentionally become more audible;
this is not a stored-data migration. Remaining block scheduling, normalization,
long-grain window approximation and aliasing limitations are unchanged.
