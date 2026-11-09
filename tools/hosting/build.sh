#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run in a fresh checkout with pinned Emscripten 3.1.74 and Python dependencies.
set -eu
HOSTING_TOOLS=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
[ "$#" = 2 ] || { echo "Usage: sh tools/hosting/build.sh APPROVED_SOURCE_CHECKOUT FULL_APPROVED_SHA" >&2; exit 1; }
python3 "$HOSTING_TOOLS/source_policy.py" "$2" --checkout "$1" --require-submodules
cd "$1"
emcc --version | head -n 1 | grep -F ' 3.1.74 ' >/dev/null
# Only generate headers; never invoke the FM-1 target build or SDK.
python3 - <<'PY'
import sys
sys.path.insert(0, 'tools')
import build
build.generate()
PY
sh web/emu/build.sh
