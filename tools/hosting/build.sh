#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run in a fresh checkout with pinned Emscripten 3.1.74 and Python dependencies.
set -eu
cd "$(dirname "$0")/../.."
emcc --version | head -n 1 | grep -F ' 3.1.74 ' >/dev/null
# Only generate headers; never invoke the FM-1 target build or SDK.
python3 - <<'PY'
import sys
sys.path.insert(0, 'tools')
import build
build.generate()
PY
sh web/emu/build.sh
