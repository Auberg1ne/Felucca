# Step240E — negotiated stable-ID editor foundation

Base: fd7f6f036487c53af3d31df3f209b63d6d2d9e8d.
Branch: feat/coffee-grains-granular-quality.

Implemented command82 / capability58(hex), version1, stable-ID layout1.
Current supported count118, flags7: scalar read/write and read-only snapshot.
No C, bulk replacement, new library format, native parameter or persistent value
is advertised. See web/EDITOR_PROTOCOL.md for exact byte layouts and lease rules.

## Changes and array-path audit

- `firmware/src/editor_stable.c`: bounded frame validation, explicit negotiation,
  USB-generation/5000ms lease, u14 IDs, strict scalar validation, snapshots and
  unsupported-operation refusal. Coverage policy is independent of layout/count
  inference and fails closed when the native schema outgrows the legacy view.
- `firmware/src/editor.c`: optional INFO tag; unchanged legacy INFO count118;
  fixed legacy DUMP, UP_GET, TRACK_DUMP and scalar-ID domain; no extended IDs on
  legacy WATCH. PRESET/G_ENGSEL, UP_PUT/LOAD/ERASE get future coverage guards.
  UP_STORE still captures native state through the existing storage routines.
- `firmware/src/upreset.c`: name the frozen118-value legacy UP_PUT framing
  separately from native P_COUNT. No record layout, version or migration change.
- `web/editor.html`: anchored capability parsing; explicit acknowledged session;
  actual WIN scalar write handler uses serialized negotiated requests on supported
  firmware, ordinary legacy scalar writes otherwise. No UI/grid control added.
  Unmarked119 imports map historically. Marked stable files/unknown versions are
  refused in library import, direct file load, audition and bank upload, before
  mutation. Non-Hann library transfer remains blocked. No JSON version bump.
- `tests/editor_stable_test.c`, `web/test_stable_protocol.mjs`,
  `tests/run_editor_stable.py`: strict native tests, real native SysEx/browser
  codec and actual stableSet handler tests, pinned old-firmware/client comparison.
- Documentation: this report and the protocol specification.

Other array paths audited: ED_PRESET factory data and UP_LOAD loaded arrays use
existing guarded native routines; ED_UP_STORE captures native parameters;
ED_PROJECT uses versioned project storage; ED_BACKUP_* uses explicit raw object
formats and existing validation/NOR guards, not naked parameter arrays. FM6 patch
arrays, sample transfers, motion records, preferences and palette commands have
separate fixed layouts and are unchanged. No raw backup path gains a parameter
reinterpretation. Browser IndexedDB and library keys remain the legacy layout;
recognized stable markers cannot enter them through the new import paths.

## Compatibility and evidence

| Client / firmware | Result |
|---|---|
|Old / old|Legacy118 remains operational.|
|New / old|No extension command without INFO capability; legacy fallback works.|
|Old / new|Legacy118 count/order and data replies identical; added INFO tag ignored.|
|New / new|Acknowledged stable118 lease; targeted existing-ID writes and reads work.|

Tests use the real native firmware gateway, not mocked firmware parameter state.
Legacy populated UP_GET, DUMP, TRACK_DUMP, GET109 and DESC109 replies compare
byte-for-byte against the exact base. The old browser's actual protocol parser
reads the new INFO successfully. Extended SET1090/1/64/127 works independently on
four tracks; no new MIDI learn or motion path is activated for WIN.

Strict native:2909 checks pass. Browser/native matrix:59 checks pass. WIN editor:
110 checks pass; WIN browser provenance:11 checks pass. Existing diagnostic
WIN DOM/native audio qualification:71 checks pass using its documented
multi-engine test profile and historical mixer signed-wrap/shift exclusions.
Protocol/storage/model checks have strict ASan/UBSan without those exclusions.

The current migration/NOR/audio runner passes: Step240A132084; preserved old
unsafe counterexamples8229; physical bank ownership26219; persistence/NOR5804;
short-envelope152924099; STUTTER42069041; filter3553312 checks. All24 frozen
PCM/state captures and independent repeat match; binary fixtures remain unchanged.

Future-C tests are explicitly models: a119-slot shadow with C0/1/55/100 is kept
unchanged when the production coverage predicate refuses a legacy118 replacement.
Unrelated scalar edits address only their requested slot. Actual persistent-C
preservation is deferred to240F. No claim that current firmware stores C is made.
Stable119 negotiation, ID118, unknown versions, expired/wrong nonces, reset,
disconnect, truncation, surplus bytes and out-of-range values are refused.

Historical broader suites remain non-green: both old and current editor_test
report22 FAIL lines then SIGSEGV with byte-identical line-buffered logs. The broad
web suite, with identical generated descriptor/tools inputs, reports the same six
FAIL lines (sample-ID/mock-table expectations) then crashes in mockTables at a
missing descriptor. These are inherited; no new failure observed in these
comparisons. The first WIN DOM attempt used shipping GRAIN-only firmware and
could not run its intentional non-GRAIN diagnostic engine switches; the documented
multi-engine profile passes. No fixture/golden was changed to hide failures.

Reproduce (existing generated headers and pinned engines required):

```
python3 tests/run_editor_stable.py \
  --baseline-tree /tmp/coffee-grc-fd7 \
  --oracle-tree /tmp/coffee-preset-evidence-606 --regressions \
  --out /tmp/stable-240e/final
```

Baseline tree must be a clean fd7f6f0 checkout with its existing pinned engine
submodule; evidence tree is the clean6062014 checkpoint required by the historical
oracle runner. Tests install/download nothing and generate outputs outside Git.

## Resources, limits and240F

Same native `editor_test.c -Os -ffp-contract=off -fstack-usage` build before/after:
text/rodata598624→599965 (+1341 bytes), data17792 unchanged,
BSS1386944 unchanged (host aggregate alignment). New session symbol is12 bytes;
no persistent storage, framebuffer, audio buffer or allocator was added.
Host ed_handle stack frame remains640 bytes; the request helper is inlined in
this build and the small main-loop expiry helper uses an8-byte host frame. These are host harness measurements, not FM1 target memory/CPU results.
No target link, hardware-fit claim, tool installation or flashing.

240F must separately add ID118, bounded descriptors, frozen historical count and
motion mappings, FUND/13 and preset8/9 provenance, native4840 import, retained/
undo/backup migration and physical-ownership qualification. It must replace the
conservative coverage refusal only with proven complete representation or an
explicit refusal; no lost C or WIN is allowed. Bulk stable-ID writes/library file
support remain unadvertised until their full transactional path is implemented.
