# GRAIN-only product policy

Base: `68c3229b66d86a067d5cacd3b20214cbf6bf2424`.

The default build sets `FELUCCA_GRAIN_ONLY=1`. Engine IDs, all 14 backend
engine descriptors/DSP implementations, all 118 parameters, codecs and storage
geometry remain present. This is a selection/activation policy, not an audio
engine rewrite. The exact base already initialized all four tracks as GRAIN
CLOUD PAD; that initialization is retained and tested.

## Selection and retained behavior

Hardware engine browsing and the EDIT engine layer offer only GRAIN. The
layer's INIT and CLEAN controls follow its sole engine entry (the existing
relative layout), and initialize GRAIN. Factory and user sound browsing skips
non-GRAIN sounds without renumbering their saved slots or favourites. MIDI
program changes remain within GRAIN. Undo, direct runtime DIGITAL conversion,
editor engine SET/PRESET and project activation cannot activate another engine.
The browser's exported heavy-test shortcut also uses four GRAIN tracks.

CLOUD, FREEZE, STUTTER and SHIMMER, factory source IDs and USR1–USR8 are
unchanged. The two-page GRAIN grid, row resolver, locks, motion, hidden settings,
MIDI routing and all retained screens keep their existing implementation.
PHRASES remains a pattern-only operation: it can reuse stored step data without
loading the source preset's engine or parameters, or altering that preset.

## Non-destructive legacy policy

* Projects with any non-GRAIN track are rejected before runtime mutation.
  The historical DRUM recovery preset marker is also rejected, even in a
  crafted record claiming GRAIN. No implicit engine conversion occurs.
* Recognized incompatible project slots are read-only: load, overwrite,
  rename, erase and backup replacement are refused. SONG preparation rejects
  incompatible source projects before arming the chain. Save new GRAIN work
  into a free or compatible slot.
* Non-GRAIN and unknown-version occupied user preset records remain read-only.
  Boot skips their normalization and historical SAMPLE/PERC migration. Updating
  a different GRAIN slot in a mixed bank preserves their raw record bytes.
* A rejected legacy/unparseable autosave is protected even with RESTORE LAST
  disabled or after an unclean boot. Automatic saves remain disabled for that
  session; manually save GRAIN work to a compatible/free project slot. An
  interrupted legacy FM6 transaction is likewise preserved in its staging
  sectors; this build does not finish or reuse that transaction's autosave space.
* FM6 patch/bank writes and automatic FM6-bank migrations are disabled. Existing
  banks/sidecars remain readable for backup. A backup containing incompatible
  projects or preset banks is rejected before committing that object. Identical
  FM6 sidecar restores are no-ops; replacing legacy FM6 data is refused.
* Backup export remains available. Backup formats/codecs are unchanged, including
  their existing historical normalization behavior; this does not promise raw
  byte export of every historical project format. Restore with compatible older
  firmware to resume a non-GRAIN project or finish legacy FM6 recovery. Keep the
  original backup. No automatic saved-data rewrite is introduced.

Backup restore retains the protocol's existing **per-object**, not whole-archive,
transaction model. A client can encounter an incompatible object after earlier
compatible objects have restored. The incompatible object is not changed.

## Editor compatibility

INFO keeps its engine count, names, IDs, parameter count and existing metadata
positions. A final optional `[0x47, 1, 8]` capability tag declares that only engine
8 is activatable. The editor filters engine/preset choices and refuses local
non-GRAIN audition/import before writing parameters. Legacy libraries are not
rewritten or deleted. Descriptor/parameter access and backup reads remain.

Old clients can still inspect the original descriptors. Unsupported engine
SET/PRESET/PROJECT and FM6 writes receive no success acknowledgement, following
the existing protocol convention for failed operations without an error field;
they will time out in older editors. The device also displays a GRAIN-only
warning. Successful compatible commands retain their original replies.

`FELUCCA_GRAIN_ONLY=0` is an explicit **compile-time diagnostic** configuration
for retained multi-engine backend tests. It is not a settings option or a
user-facing escape route in the default firmware.

## Validation

Run shipping-policy sanitizer tests with `sh tests/run_grain_only.sh`. Generated
sample/tables and the existing engine checkout must already be available. Set
`GRAIN_ONLY_BASE_TREE` to an already prepared exact-base worktree for audio and
retained-screen byte comparisons. The new script uses the existing host
sanitizer exclusions for deliberate DSP wrapping and host XIP rebasing; it adds
no exclusions. No assertion was removed from the existing suites.

| Test/configuration | Passing | Failing |
| --- | ---: | ---: |
| Shipping selection, initialization, sources, personalities, legacy project policy (ASan/UBSan) | 380 | 0 |
| Shipping editor/protocol/backup policy (ASan/UBSan) | 70 | 0 |
| Shipping physical NOR persistence, autosave and legacy staging (ASan/UBSan) | 24 | 0 |
| Shipping actual audio from CH1–CH4, pan/spread and silence (ASan/UBSan) | 82 | 0 |
| Shipping GRAIN theme/shared sequencer (ASan/UBSan) | 8,229 | 0 |
| Shipping grid values/labels/240×240 bounds (ASan/UBSan) | 1,935 | 0 |
| Editor capability/filter/no-write rejection (Node) | 20 | 0 |
| Retained multi-engine UI (diagnostic mode, ASan/UBSan) | 850 | 0 |
| Retained editor (diagnostic mode, ASan/UBSan) | 172 | 0 |
| Project format FUN11/FUN10 (diagnostic mode, ASan/UBSan) | 41,146 | 0 |
| Project runtime (diagnostic mode, ASan/UBSan) | 61 | 0 |
| Presets (diagnostic mode, ASan/UBSan) | 45 | 0 |
| Backups (diagnostic mode, ASan/UBSan) | 103 | 0 |
| GRAIN grid incl. other-engine isolation (diagnostic mode, ASan/UBSan) | 2,329 | 0 |
| STRETCH incl. non-GRAIN reserved slots (diagnostic mode, ASan/UBSan) | 1,764 | 0 |
| Motion (diagnostic mode, ASan/UBSan), **also exact base** | 97 | 27 |
| MIDI (diagnostic mode, ASan/UBSan), **also exact base** | 134 | 15 |
| Broad web/OTA (Node), **also exact base** | 282 | 5 |

The Motion/MIDI failing assertion lists match the exact-base logs line for line;
web failures also match. Historical reported MIDI counts differ from this exact
base's reproducible 15. These are not claimed fixed or new regressions.
Additional existing editor tests passed: STRETCH metadata refresh 27, source order
48; all five original factory WAV checksums and full-frame encoded data passed
`tests/factory_assets_test.py` (358,284 encoded bytes, original recordings intact).

The identical-state native audio comparison covers each of four GRAIN tracks,
with/without stereo spread, sounding and silent levels: **14,336,000 bytes
identical**. Retained navigation/screens: **270 snapshots, 31,381,560 bytes
identical**. The engine/preset picker and EDIT engine layer intentionally change
and are tested by the shipping-policy suite rather than this isolation trace.

## Resources and build limits

No bitmap, framebuffer, allocation, audio callback work, DSP or sample assets
were added/changed. One byte of static policy state protects legacy autosave/
staging. Existing project scratch buffers perform preflight checks in the main
loop; there is no new project-sized buffer or persistent field.

Actual comparable native `cc -Os -ffp-contract=off` object sizes for
`web/emu/felucca_web.c`:

| Section | Exact base | GRAIN-only | Delta |
| --- | ---: | ---: | ---: |
| text (includes read-only assets) | 880,258 | 878,292 | −1,966 |
| data | 31,060 | 31,060 | 0 |
| bss | 1,603,232 | 1,603,232 | 0 |

These are native host measurements including browser NOR/display simulation,
not target firmware flash/RAM measurements. The new byte fits existing native
alignment; target section placement is unverified. No memory layout changes.

`bash web/emu/build.sh` stops with `emu: no emcc (Emscripten)`.
`./build.sh` stops with `build.sh: no toolchain in /home/agent/.jieli/toolchain
(run tools/get_toolchain.sh)`. No tools were installed. Native browser-backend
rendering and bounds tests passed; WASM compilation and target linking remain
blocked. There is **no claim of fit within the 581,564-byte target image limit**.

## Changed files

* Firmware policy/activation: `firmware/src/core.h`, `engines.c`, `ui.c`,
  `project.c`, `upreset.c`, `up_fm6.c`, `fm6_vbank.c`, `editor.c`,
  `editor_backup.c` (all under `firmware/src/`).
* Browser: `web/editor.html`, `web/emu/felucca_web.c`.
* Tests: `tests/grain_only_test.c`, `grain_only_editor_test.c`,
  `grain_only_persistence_test.c`, `grain_only_audio_test.c`,
  `grain_only_navigation_test.c`, `run_grain_only.sh`, `persistence_test.c`
  (all under `tests/`); `web/test_grain_only.mjs`.
* Documentation: `docs/grain-only.md`, `web/EDITOR_PROTOCOL.md`.
