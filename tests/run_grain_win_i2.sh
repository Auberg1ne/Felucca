#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# No downloads/build dependency installation. Existing generated headers required.
set -eu
cd "$(dirname "$0")/.."
out=${WIN_I2_OUT:-/tmp/coffee-grains-win-i2}
i1=${WIN_I1_TREE:?Set WIN_I1_TREE to exact 68f3e7a worktree}
experiment=${WIN_MORPH_TREE:?Set WIN_MORPH_TREE to exact 4c34b6e worktree}
mkdir -p "$out"
cc=${CC:-cc}
flags='-O1 -g -w -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=all -Ibuild/gen -Ifirmware/src'
for test in grain_morph_numeric grain_morph_kernel grain_win_i2_latch; do
    $cc $flags "tests/${test}_test.c" -lm -o "$out/$test"
    "$out/$test"
done
python3 tests/grain_win_i2_compare.py --i1-tree "$i1" --experiment-tree "$experiment" --out "$out/audio"
python3 tests/grain_win_i2_stress.py --i1-tree "$i1" --experiment-tree "$experiment" --out "$out/stress"
# Only inherited common-DSP exclusions; strict new arithmetic is tested above.
$cc $flags -fno-sanitize=shift,signed-integer-overflow tests/grain_win_i2_roundtrip_test.c -lm -o "$out/roundtrip"
for win in 0 64 127; do
    for type in 0 1 2 3; do
        for route in 0 1 2; do
            "$out/roundtrip" "$win" "$type" "$route" "$out/roundtrip-$win-$type-$route.pcm"
        done
        cmp "$out/roundtrip-$win-$type-0.pcm" "$out/roundtrip-$win-$type-1.pcm"
        cmp "$out/roundtrip-$win-$type-0.pcm" "$out/roundtrip-$win-$type-2.pcm"
    done
done
WIN_OUT="$out/persistence" tests/run_grain_win_i1r.sh
# Optional: host ThreadSanitizer availability varies. Explicit opt-in, not a skipped mandatory gate.
if [ "${WIN_I2_TSAN:-0}" = 1 ]; then
    $cc -O1 -g -w -ffp-contract=off -fsanitize=thread -DC1_HOST=1 -DWIN_I2_PRODUCTION=1 -Ibuild/gen -Ifirmware/src tests/grain_win_i2_audio_test.c -lm -pthread -o "$out/tsan"
    for type in 0 1 2 3; do
        TSAN_OPTIONS=halt_on_error=1 "$out/tsan" live 16 "$type" 13 2 "$out/tsan-$type.pcm"
    done
fi
