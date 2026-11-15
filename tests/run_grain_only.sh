#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Shipping-policy tests. The full multi-engine backend suite can separately run
# with CC='cc -DFELUCCA_GRAIN_ONLY=0' tests/run_tests.sh; never use that switch here.
set -eu
cd "$(dirname "$0")/.."
out=build/host/grain-only
mkdir -p "$out"
cc=${CC:-cc}
flags='-O1 -g -w -ffp-contract=off -Ibuild/gen -Ifirmware/src'
if [ "${SANITIZE:-1}" != 0 ]; then
    # Same intentional DSP/XIP exclusions as the existing host sanitizer suite.
    flags="$flags -fsanitize=address,undefined -fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow -fno-sanitize-recover=undefined"
fi
for test in grain_only_test grain_only_editor_test grain_only_persistence_test grain_only_audio_test grain_cutoff_zero_test grain_position_test grain_tone_test grain_drift_test grain_dtune_test long_edit_test env_grid_test lfo_mod_ui_test glo_theme_test; do
    $cc $flags -UFELUCCA_GRAIN_ONLY -DFELUCCA_GRAIN_ONLY=1 "tests/$test.c" -lm -o "$out/$test"
    "$out/$test"
done
$cc $flags -UFELUCCA_GRAIN_ONLY -DFELUCCA_GRAIN_ONLY=1 tests/grain_cutoff_audio_test.c -lm -o "$out/grain_cutoff_audio_test"
for type in 0 1 2 3; do
    for cut in -25 0 25 switch; do
        "$out/grain_cutoff_audio_test" "$type" "$cut" "$out/cutoff.pcm" 4
    done
done
$cc $flags -UFELUCCA_GRAIN_ONLY -DFELUCCA_GRAIN_ONLY=1 tests/grain_position_audio_test.c -lm -o "$out/grain_position_audio_test"
for type in 0 1 2 3; do
    for source in 0 2 3 13 14; do
        "$out/grain_position_audio_test" "$type" "$source" 64 0 static 4 "$out/position.pcm"
    done
    "$out/grain_position_audio_test" "$type" medium 0 127 pos 4 "$out/position.pcm"
    "$out/grain_position_audio_test" "$type" medium 64 127 shape 4 "$out/position.pcm"
done
node web/test_grain_only.mjs
# Optional exact-base rendering comparison (base must already have generated tables).
if [ -n "${GRAIN_ONLY_BASE_TREE:-}" ]; then
    for kind in audio navigation; do
        case "$kind" in audio) macro=GRAIN_ONLY_UI_SOURCE;; navigation) macro=GRID_UI_SOURCE;; esac
        for tree in base current; do
            source=$PWD
            if [ "$tree" = base ]; then source=$GRAIN_ONLY_BASE_TREE; fi
            $cc $flags "-D$macro=\"$source/tests/ui_test.c\"" "tests/grain_only_${kind}_test.c" -lm -o "$out/$kind-$tree"
            "$out/$kind-$tree" "$out/$kind-$tree.bin"
        done
    done
    cmp "$out/audio-base.bin" "$out/audio-current.bin"
    cmp "$out/navigation-base.bin" "$out/navigation-current.bin"
fi

# F2/F3/F6 intentionally change long-source audio. Keep historical comparators
# intact; this separate exact-base matrix also requires unaffected short-source identity.
if [ -n "${GRAIN_POSITION_BASE_TREE:-}" ]; then
    python3 tests/grain_position_compare.py --base-tree "$GRAIN_POSITION_BASE_TREE" --out "$out/position-comparison"
fi

# TONE final-output checks: include the fallback and a real user-zone descriptor.
$cc $flags -UFELUCCA_GRAIN_ONLY -DFELUCCA_GRAIN_ONLY=1 tests/grain_tone_audio_test.c -lm -o "$out/grain_tone_audio_test"
for type in 0 1 2 3; do
    for source in 0 user sine; do
        for tone in 0 1 32 64 96 126 127; do
            "$out/grain_tone_audio_test" "$type" "$source" "$tone" 0 0 static 4 "$out/tone.pcm"
        done
        "$out/grain_tone_audio_test" "$type" "$source" 127 0 0 live 4 "$out/tone.pcm"
    done
done
if [ -n "${GRAIN_TONE_BASE_TREE:-}" ]; then
    python3 tests/grain_tone_compare.py --base-tree "$GRAIN_TONE_BASE_TREE" --out "$out/tone-comparison"
fi

# CLOUD DRIFT: actual four-track audio; personality A remains isolated in other types.
$cc $flags -UFELUCCA_GRAIN_ONLY -DFELUCCA_GRAIN_ONLY=1 tests/grain_drift_audio_test.c -lm -o "$out/grain_drift_audio_test"
for type in 0 1 2 3; do
    for source in 0 2 3 13 14; do
        for drift in 0 1 32 63 64 65 96 127; do
            "$out/grain_drift_audio_test" "$type" "$source" "$drift" 0 static 4 "$out/drift.pcm"
        done
    done
done
for source in 0 2 3 13 14; do
    for mode in pos shape drift type; do
        "$out/grain_drift_audio_test" 0 "$source" 0 0 "$mode" 4 "$out/drift.pcm"
    done
done
if [ -n "${GRAIN_DRIFT_BASE_TREE:-}" ]; then
    python3 tests/grain_drift_compare.py --base-tree "$GRAIN_DRIFT_BASE_TREE" --out "$out/drift-comparison"
fi

# Allocator-driven DTUNE audio: three voices, shared four-track budget and held edits.
$cc $flags -UFELUCCA_GRAIN_ONLY -DFELUCCA_GRAIN_ONLY=1 tests/grain_dtune_audio_test.c -lm -o "$out/grain_dtune_audio_test"
for type in 0 1 2 3; do
    for source in 0 2 3 13 14; do
        for dtune in 0 1 40 64 127; do
            "$out/grain_dtune_audio_test" "$type" "$source" "$dtune" 3 0 0 0 127 static 1 "$out/dtune.pcm"
        done
        "$out/grain_dtune_audio_test" "$type" "$source" 127 3 24 127 1 127 static 4 "$out/dtune.pcm"
        for mode in held retrigger; do
            "$out/grain_dtune_audio_test" "$type" "$source" 0 3 0 0 0 127 "$mode" 1 "$out/dtune.pcm"
        done
    done
done
if [ -n "${GRAIN_DTUNE_BASE_TREE:-}" ]; then
    python3 tests/grain_dtune_compare.py --base-tree "$GRAIN_DTUNE_BASE_TREE" --out "$out/dtune-comparison"
fi

# Headroom tests do not inherit historical signed-overflow exclusions.
safe_flags='-O1 -g -w -ffp-contract=off -Ibuild/gen -Ifirmware/src'
if [ "${SANITIZE:-1}" != 0 ]; then
    safe_flags="$safe_flags -fsanitize=address,undefined -fno-sanitize-recover=undefined"
fi
for test in grain_headroom_repro grain_headroom_test; do
    $cc $safe_flags -UFELUCCA_GRAIN_ONLY -DFELUCCA_GRAIN_ONLY=1 "tests/$test.c" -lm -o "$out/$test"
    "$out/$test"
done
# Other historical mixer bit shifts remain excluded, but signed overflow stays on.
audio_safe_flags=$safe_flags
if [ "${SANITIZE:-1}" != 0 ]; then audio_safe_flags="$audio_safe_flags -fno-sanitize=shift"; fi
$cc $audio_safe_flags -UFELUCCA_GRAIN_ONLY -DFELUCCA_GRAIN_ONLY=1 tests/grain_headroom_audio_test.c -lm -o "$out/grain_headroom_audio_test"
for type in 0 1 2 3; do
    for source in 0 2 3 13 14 user sine; do
        for cut in -50 -25 0 25 50; do
            for res in 0 127; do
                "$out/grain_headroom_audio_test" "$type" "$source" 127 "$cut" "$res" static 4 "$out/headroom.pcm"
            done
        done
        "$out/grain_headroom_audio_test" "$type" "$source" 127 0 0 sweep 4 "$out/headroom.pcm"
    done
    for source in 0 2 3 13 14; do
        "$out/grain_headroom_audio_test" "$type" "$source" 127 25 127 unison 4 "$out/headroom.pcm"
        "$out/grain_headroom_audio_test" "$type" "$source" 127 25 127 hot 4 "$out/headroom.pcm"
    done
done
if [ -n "${GRAIN_HEADROOM_BASE_TREE:-}" ]; then
    python3 tests/grain_headroom_compare.py --base-tree "$GRAIN_HEADROOM_BASE_TREE" --out "$out/headroom-comparison"
fi

# Gesture timing is unsigned and requires no historical UI/DSP UBSan exclusions.
$cc $safe_flags -UFELUCCA_GRAIN_ONLY -DFELUCCA_GRAIN_ONLY=1 tests/long_edit_timing_test.c -lm -o "$out/long_edit_timing_test"
"$out/long_edit_timing_test"
if [ -n "${LONG_EDIT_BASE_TREE:-}" ]; then
    python3 tests/long_edit_compare.py --base-tree "$LONG_EDIT_BASE_TREE" --out "$out/long-edit-comparison"
fi

if [ -n "${ENV_GRID_BASE_TREE:-}" ]; then
    python3 tests/env_grid_compare.py --base-tree "$ENV_GRID_BASE_TREE" --out "$out/env-grid-comparison"
fi

if [ -n "${GRID_HINTS_BASE_TREE:-}" ]; then
    python3 tests/grid_hints_compare.py --base-tree "$GRID_HINTS_BASE_TREE" --out "$out/grid-hints-comparison"
fi

if [ -n "${LFO_MOD_BASE_TREE:-}" ]; then
    python3 tests/lfo_mod_compare.py --base-tree "$LFO_MOD_BASE_TREE" --out "$out/lfo-mod-comparison"
fi

if [ -n "${MOD_LABEL_BASE_TREE:-}" ]; then
    python3 tests/mod_label_compare.py --base-tree "$MOD_LABEL_BASE_TREE" --out "$out/mod-label-comparison"
fi

if [ -n "${GLO_THEME_BASE_TREE:-}" ]; then
    python3 tests/glo_theme_compare.py --base-tree "$GLO_THEME_BASE_TREE" --out "$out/glo-theme-comparison"
fi
