#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Existing generated headers / initialized pinned engine sources are prerequisites.
set -eu
cd "$(dirname "$0")/.."
out=${WIN_OUT:-/tmp/coffee-grains-win-i1r}
base=${WIN_B1_TREE:?Set WIN_B1_TREE to the clean B1 worktree}
[ "$(git -C "$base" rev-parse HEAD)" = 6055566f6d6f3a9735d822cf4d1f6283c1d23e57 ]
git -C "$base" diff --exit-code HEAD -- firmware tests
mkdir -p "$out"
(cd tests/fixtures/grain-win-i1r && sha256sum -c SHA256SUMS)
cc=${CC:-cc}
flags='-O1 -g -w -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=all -Ibuild/gen -Ifirmware/src'
for mode in 1 0; do
    $cc $flags -DFELUCCA_GRAIN_ONLY=$mode tests/grain_win_persistence_test.c -lm -o "$out/nor-$mode"
    "$out/nor-$mode" "$out"
done
$cc $flags tests/grain_win_editor_test.c -lm -o "$out/editor"
"$out/editor"
$cc $flags tests/upreset_test.c -lm -o "$out/presets"
"$out/presets"
$cc $flags "-DWIN_B1_PERSISTENCE_SOURCE=\"$base/tests/persistence_test.c\"" "-DWIN_B1_BACKUP_SOURCE=\"$base/firmware/src/editor_backup.c\"" tests/grain_win_downgrade_oracle.c -lm -o "$out/b1-oracle"
"$out/b1-oracle" "$out"
# Keep the original historical failing oracle unchanged. The old revision is
# optional locally; it is NOT a supported downgrade target.
if [ -n "${WIN_UNPATCHED_TREE:-}" ]; then
    [ "$(git -C "$WIN_UNPATCHED_TREE" rev-parse HEAD)" = 99325e7eef99e09b5565edf37ed9098ec5c2f5af ]
    $cc $flags "-DWIN_I1_BASE_PERSISTENCE_SOURCE=\"$WIN_UNPATCHED_TREE/tests/persistence_test.c\"" tests/grain_win_i1_downgrade_gate.c -lm -o "$out/historical-oracle"
    if "$out/historical-oracle" >"$out/historical-oracle.log" 2>&1; then exit 1; else [ "$?" = 1 ]; fi
    rg 'preserved=0; replacement magic=46554e42' "$out/historical-oracle.log"
fi
node web/test_win_persistence.mjs
# No new rendering path is enabled; compare sounding final mixer output.
for which in base current; do
    tree=$(pwd); [ "$which" != base ] || tree=$base
    $cc -Os -w -ffp-contract=off -Ibuild/gen -Ifirmware/src "-DGRAIN_ONLY_UI_SOURCE=\"$tree/tests/ui_test.c\"" tests/grain_only_audio_test.c -lm -o "$out/audio-$which"
    "$out/audio-$which" "$out/audio-$which.bin"
done
cmp "$out/audio-base.bin" "$out/audio-current.bin"
echo 'WIN I1-R strict gates and byte-identical final-output audio passed'
