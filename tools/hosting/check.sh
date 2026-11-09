#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Focused required deployment gates; never mask failures with `|| true`.
set -eu
HOSTING_TOOLS=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "${1:-$HOSTING_TOOLS/../..}"
mkdir -p build/hosting-tests
# Use the reviewed gate from the workflow checkout, even when checking another source SHA.
sed 's|"../../web/emu/sample_xip_test.c"|"web/emu/sample_xip_test.c"|' "$HOSTING_TOOLS/factory_gate.c" > build/hosting-tests/factory_gate.c
cc -O1 -w -Ibuild/gen -Ifirmware/src -I. build/hosting-tests/factory_gate.c -lm -o build/hosting-tests/factory_gate
build/hosting-tests/factory_gate
for name in sample_delete_test storage_test storage_fault_gate_test slice_store_test backup_test project_codec_test eight_slot_compat_test; do
  cc -O1 -w -Ibuild/gen -Ifirmware/src "tests/$name.c" -lm -o "build/hosting-tests/$name"
  "build/hosting-tests/$name"
done
cc -O1 -w -Ibuild/gen -Ifirmware/src web/emu/editor_bridge_test.c -lm -o build/hosting-tests/editor_bridge_test
build/hosting-tests/editor_bridge_test
node web/test_web.mjs --s3-only
node web/test_backup.mjs
node web/emu/emu_test.mjs build/emu/felucca.wasm
node web/emu/startup_test.mjs build/emu/felucca.wasm
node web/emu/editor_transport_test.mjs build/hosting-tests/editor_bridge_test --wasm build/emu/felucca.wasm
