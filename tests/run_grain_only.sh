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
for test in grain_only_test grain_only_editor_test grain_only_persistence_test grain_only_audio_test grain_cutoff_zero_test grain_position_test; do
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
