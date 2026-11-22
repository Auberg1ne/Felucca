# Personality effect in hardware slot D

Base: cc40a6d24beb4b45d07e4494808a1d5b99e2fa57.

The personality row is GRAIN EDIT page2, row1. Audit confirmed A/K1=TYPE103,
B/K2=personality A104, C/K3=personality B105, D/K4=empty255. No function was
removed. The authoritative GRAIN_GRID resolver now maps D to existing ID118.
A/B/C, all other rows, page/row navigation and knob modifiers are unchanged.

D labels in hardware only: TYPE0 CLOUD→SPREAD, TYPE1 FREEZE→DRIFT,
TYPE2 STUTTER→CHAOS, TYPE3 SHIMMER→SPARKLE. Labels fit the existing53-pixel
text area. Values remain integers0..100. TYPE changes and editor/external value
changes invalidate the existing cell signature; no new UI state or buffers.
Canonical descriptor name C, parameter IDs, range/default, persistence, editor
negotiation and formats are unchanged. Internal personality-C/C0 terminology
continues to refer to ID118, not physical knob C. Existing TYPE-dependent A/B
labels and behavior are untouched.

Hardware K4 uses the existing page-relative parameter editor and canonical
track parameter. The audio path reads that same value on its existing control
cadence; no new staging, DSP algorithm or latency is introduced. Existing live
smoothing/latching policies still apply. All four DSP implementations remain
unchanged, including lightweight CLOUD allpasses; expensive stereo stays off.
ID118 remains quarantined from MIDI learn, motion, locks and modulation. No new
protocol command, fixed CC or automation destination is introduced.

## Validation

- Strict ASan/UBSan D UI test:1,068,427 checks. All101 values, four types, four
  tracks; A/B/C values preserved; K4 bounds; project round trips; label width;
  external value refresh; unrelated pixels outside D unchanged; automation
  quarantine. Optional output argument captures four native240x240 PPM frames.
- GRAIN grid diagnostic multi-engine test:2584 checks; WIN UI62537 checks.
- Editor test2663 checks, including negotiated writes resolving to hardware D.
- All24 frozen C0 PCM/state captures and repeat pass, as do short-envelope
 152924099 and STUTTER gate42069041 checks and historical persistence/NOR5804.
- Width runner:46,783,560 strict checks;69 exact base/default/other-personality
  pairs, PAN/V-SPRD/effects, all factories/amounts, rapid changes and four actual
  threaded CPU2 GRAIN/mixed scenes. Baseline driver explicitly expects the old
  empty D; current driver expects118. No PCM hashes/goldens were regenerated.
- Editor/browser foundation, C persistence109038, downgrade362 and WIN
  provenance gates pass. Existing full-mixer signed-overflow/shift exclusions
  remain documented in the width report; local UI/storage tests are strict.

Commands:

```
cc -O1 -g -w -Wno-implicit-function-declaration -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=all -Ibuild/gen -Ifirmware/src tests/grain_effect_d_ui_test.c -lm -o /tmp/effect-D-ui
/tmp/effect-D-ui
python3 tests/run_cloud_width.py --base-tree /tmp/coffee-cloud-c421 --out /tmp/effect-D-verify/width
python3 tests/run_granular_quality.py --sanitize --oracle-tree /tmp/coffee-grc-fd7 --out /tmp/effect-D-verify/quality
python3 tests/run_editor_stable.py --baseline-tree /tmp/coffee-grc-fd7 --e-tree /tmp/coffee-e-371 --out /tmp/effect-D-verify/editor
```

The old default grain_grid_test fails line63 on both base and current because
it tries to select DRUM in a GRAIN-only build. Its documented diagnostic
`-DFELUCCA_GRAIN_ONLY=0` profile passes. Standalone ui_test compilation also has
an inherited missing fop_op error under shipping flags on both revisions; no
unrelated test or production code was changed to hide these failures.

## Resources / target status

No DSP/grain/track state, audio buffers, engine reservation or storage growth.
A DSP-only host executable is byte-size unchanged: text548059/data16540/BSS306088.
This does not measure new UI code. The -O2 diagnostic multi-engine full-UI
test executable measured text1384250→1389050(+4800), data34592→34624(+32),
BSS2856448 unchanged; optimizer/inlining and host harness costs are included.
These are not target firmware sizes. New runtime state is zero; UI adds only label constants and
selection code. Target flash size and stack/timing remain unmeasured.

Authenticated GitHub Codespaces listing is empty and the local supported JieLi
compiler is absent. No toolchain download, host substitute, merge, release or
flashing is attempted. A real FM1 build remains required using
`FELUCCA_SIZE=1 ./build_coffee_grains.sh` at this correction's exact commit.
Captures and logs are under /tmp/effect-D-verify, not persistent firmware assets.
