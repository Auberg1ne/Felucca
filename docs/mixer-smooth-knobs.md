# Visual-only MIXER smoothing

Branch based on `2d20caf85ab2893aad7b77b24e902049e90361b1`.
LEVEL, PAN and REV upper pointers interpolate in normalized Q15 position space.
Text still formats the actual parameter immediately. MUTE is binary and instant.
The existing lower strip indicators and all input/audio paths are unchanged.

The existing monotonic `fm1_ms` clock supplies unsigned elapsed milliseconds,
including wraparound. An 80 ms exponential envelope uses a Q15 per-millisecond
retention factor of 32361/32768 (approximately exp(-1/80)); composing the elapsed
milliseconds keeps it independent of UI cadence within fixed-point rounding.
Elapsed time is capped at 50 ms per render to bound stalls and calculation cost.
At most 50 small integer multiplies compute one shared decay coefficient; each
of three controls then needs one multiply. No math library, timers, threads,
allocations or audio callback changes. Reversals follow the latest target without
overshoot. Within 32/32768 of the target, the position snaps exactly and stops
requesting redraws. Pointer endpoints interpolate along the existing nine
positions rather than jumping between nine discrete directions.

Page entry, display wake and channel changes snap to that channel's real values;
ordinary forced redraws do not reset interpolation. Only MIXER updates this state.
The cache grows from 20 to 36 bytes (+16 bytes), with no new images or canvas.
Actual target flash delta needs a successful pi32v2 link; the target toolchain is
not available here. Hardware capacity remains unverified.

Focused tests exercise immediate text, large jumps, reversal, exact settling,
10/20/30 ms cadence equivalence, stalled-time bounding, clock wrap, independent
channel targets, instant mute and hidden-page inactivity. Navigation traces compare
270 snapshots and all pixels outside the upper control cells; audio renders compare
all samples with smoothing disabled/enabled using `COFFEE_MIXER_SMOOTH_KNOBS`.

Validation: ASan/UBSan (using the existing DSP exclusions documented in
mixer-top-knobs.md) passed 32 smoothing checks, 9,265 knob checks, 7,583 MIXER
checks and all 850 UI assertions. Bounds and static-value clipping counts were
zero. All 270 old/new navigation snapshots and pixels outside the upper cells
matched; the disabled-smoothing trace also matched the prior checkpoint.

Smoothing enabled/disabled audio renders matched all 705,600 bytes, SHA-256
`19edec409a606fa3d4080259771717ced0c46bdd9b8104823b8d089849d97338`.
No audible sample, parameter, mixer input or navigation changes were detected.
`git diff --check` and shell syntax checks passed. Changes remain uncommitted.
