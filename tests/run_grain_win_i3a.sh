#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
out=${WIN_I3A_OUT:-/tmp/coffee-grains-win-i3a}
base=${WIN_I2_TREE:?Set WIN_I2_TREE to exact clean 838540d worktree}
mkdir -p "$out"
cc=${CC:-cc}
flags='-O1 -g -w -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=all -Ibuild/gen -Ifirmware/src'
for test in grain_win_i3a_ui grain_morph_numeric grain_morph_kernel grain_win_i2_latch; do
    $cc $flags "tests/${test}_test.c" -lm -o "$out/$test"
    "$out/$test"
done
$cc $flags -DFELUCCA_GRAIN_ONLY=0 tests/grain_grid_test.c -lm -o "$out/grid"
"$out/grid"
# The native browser firmware adapter retains the existing simulator sanitizer policy.
$cc $flags -fno-sanitize=shift,signed-integer-overflow,bounds,object-size,pointer-overflow web/emu/grain_grid_visuals_test.c -lm -o "$out/visual"
"$out/visual" "$out"
$cc $flags -fno-sanitize=shift,signed-integer-overflow -DFELUCCA_GRAIN_ONLY=0 tests/grain_win_i3a_gateway.c -lm -o "$out/gateway"
node web/test_win_controls.mjs "$out/gateway"
node web/test_win_persistence.mjs
python3 tests/grain_win_i3a_captures.py --base-tree "$base" --out "$out/captures"
WIN_OUT="$out/persistence" tests/run_grain_win_i1r.sh
