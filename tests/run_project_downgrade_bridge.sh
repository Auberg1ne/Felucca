#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Requires the existing host generated headers and initialized engine interface.
set -eu
cd "$(dirname "$0")/.."
out=${BRIDGE_OUT:-/tmp/coffee-grains-project-bridge}
mkdir -p "$out"
cc=${CC:-cc}
flags='-O1 -g -w -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=all -Ibuild/gen -Ifirmware/src'
for mode in 1 0; do
    $cc $flags -DFELUCCA_GRAIN_ONLY=$mode tests/project_downgrade_bridge_test.c -lm -o "$out/nor-$mode"
    "$out/nor-$mode"
done
$cc $flags tests/project_downgrade_retained_test.c -lm -o "$out/retained"
"$out/retained"
$cc $flags '-DWIN_I1_BASE_PERSISTENCE_SOURCE="persistence_test.c"' tests/grain_win_i1_downgrade_gate.c -lm -o "$out/bridge-oracle"
"$out/bridge-oracle"
if [ -n "${BRIDGE_BASE_TREE:-}" ]; then
    base=$BRIDGE_BASE_TREE
    [ "$(git -C "$base" rev-parse HEAD)" = 99325e7eef99e09b5565edf37ed9098ec5c2f5af ]
    git -C "$base" diff --exit-code HEAD -- firmware tests
    $cc $flags "-DWIN_I1_BASE_PERSISTENCE_SOURCE=\"$base/tests/persistence_test.c\"" tests/grain_win_i1_downgrade_gate.c -lm -o "$out/base-oracle"
    if "$out/base-oracle" >"$out/base-oracle.log" 2>&1; then
        echo 'Historical oracle unexpectedly passed' >&2
        exit 1
    else
        status=$?
        [ "$status" = 1 ]
        rg 'pre-I1 setup/read checks: 12 passed' "$out/base-oracle.log"
        rg 'preserved=0; replacement magic=46554e42' "$out/base-oracle.log"
    fi
else
    echo 'Historical oracle not rerun: set BRIDGE_BASE_TREE to the exact clean I1 base.'
fi
