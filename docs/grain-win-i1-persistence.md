# WIN I1 persistence: stopped at the downgrade gate

Base: `ca62bb15047ffc42d586b1be0edebecced36d49c`.
Branch: `feat/coffee-grains-win-i1-persistence`.

## Status

**I1 is not implemented.** The complete integration design was read, and the
mandatory previous-revision downgrade oracle was exercised before editing
conversion code. It disproves the required preservation guarantee for a distinct
new project magic. Following the instruction to stop a part whose required
downgrade invariant cannot be safely implemented, project-format activation is
blocked. Descriptor/preset activation is also withheld rather than creating a
partially migrated parameter with ambiguous persistence.

Production firmware, descriptors, storage formats, defaults, UI, DSP, browser,
MIDI, sample assets and experimental flags are unchanged. This checkpoint
contains only this report, frozen test inputs and a reproducible failing gate.
It must not be described as completed WIN integration.

## Exact blocker and oracle evidence

The oracle compiles the real physical NOR persistence fixture from the **exact
base revision**, not a hypothetical new reader. It starts with a valid frozen
FUNB project and a copy with only the prospective FUNC magic and corresponding
FNV checksum changed. Both are3840 bytes with identical field offsets. The new
magic is deliberately unknown to the old reader, as the design requires.

Observed in both unsanitized and strict ASan/UBSan runs:

```
pre-I1 setup/read checks: 12 passed
unknown slot readonly=0; load rc=1; SAVE rc=0; erase delta=1; preserved=0; replacement magic=46554e42
FAIL: previous revision overwrites unsupported future project through normal SAVE
```

The gate exits **1**, intentionally reporting failure of the required invariant.
Exit2 is reserved for a broken probe/fixture. No expectation was changed to
classify the overwrite as a success.

1. The base accepts its frozen FUNB project.
2. It correctly rejects the prospective FUNC project on import/load.
3. Fetch/load preserve physical NOR bytes. **No automatic flash erasure was
   observed during load**; the failure is a subsequent normal user SAVE.
4. Its fetch path clears the retained slot's header when the future format
   cannot be imported. The slot is consequently presented as empty/unusable.
5. Normal `project_save(0)` then succeeds and performs one erase. Reading the
   storage object returns a new FUNB project, not the original future bytes.
   There is no unknown-format read-only protection on this path.

Source causes in `firmware/src/project.c` at the base:

- `proj_import_any` (around526): only known project magics are accepted.
- `proj_fetch` (around894): failed import clears the retained slot's first word.
- `grain_slot_readonly` (:945–949): **failed import returns0, writable**;
  protection only applies to successfully decoded incompatible engines.
- `project_save_as` (:953 onward): checks that predicate and then writes the
  current project through `st_save`, replacing the unsupported stored object.
- `project_used` (:1282): failed import is not considered a used slot.

This is pre-existing behavior, not an I1 regression. New code on the I1 branch
cannot retroactively fix that writer in `ca62bb1`. A distinct new magic supplies
safe *load rejection*, but not the stronger supported-downgrade preservation
invariant once the old firmware is used to save.

The design's user-record protections are different: unsupported used record
versions are protected on direct replacement in the base, and raw backup bank
restore checks `grain_bank_readonly`. That does not remedy the project-slot
failure. No claim is made that every bank save path is globally read-only; the
base's `up_put` protects its target, while backup restore checks whole banks.

## Format decisions and withheld work

The approved intended contracts remain:

| Item | Approved proposal | Implemented here |
|---|---|---|
| GRAIN WIN | ID109,0..127,default64; other engines reserved0 | No |
| IDs/layout | P_COUNT118,E0=110; no renumbering | Existing layout untouched |
| Serialized project | Distinct semantic version,3840 bytes, existing offsets | No new reader/writer/magic introduced |
| Retained schema | Explicit upgrade, no legacy109 reinterpretation | No activation |
| User records | WIN-aware normal/grid versions6/7,192 bytes | No activation |
| Legacy import | Every pre-WIN GRAIN record maps109 to64 | Withheld with storage activation |
| Bulk editor input | Existing command layouts; conservative legacy-Hann/rejection | No protocol or command changes |
| Learn/motion/modulation | WIN disabled pending approved provenance/eligibility | Existing reserved behavior unchanged |

`0x46554e43` was checked as an unused prospective magic at this base and used
only in the **test input**. It is not a released/implemented format decision.
No SysEx tag, extension, status, settings version or storage byte was allocated.
No descriptor-only change was made: it would leave legacy118-value records
containing raw109=0 and prematurely establish unsafe semantics for later DSP.

The old-client bulk-array ambiguity is independently real: no provenance exists
in its unchanged118-value UP_PUT payload. The proposed I1 policy remains legacy
Hann for unmarked input plus rejection of ambiguous replacement of a new
non-Hann record, using existing error framing. It has not been implemented or
validated here because dependent versioned storage is blocked. Existing
persistent reserved109 MIDI-learn bytes have not been activated or rewritten.

## Tests and limits

Changed test files:

- `tests/grain_win_i1_downgrade_gate.c`: exact-base release gate using the
  existing physical NOR fixture; deliberately fails on the preservation defect.
- `tests/fixtures/grain-win-i1/fun11-base.bin`: frozen actual base writer output.
- `tests/fixtures/grain-win-i1/proposed-func.bin`: prospective-format input.
- Fixture README documents derivation, scope and SHA-256 checksums.

Two probe executions: **12 setup/read checks pass in each; one downgrade gate
fails in each**. Strict build uses `-fsanitize=address,undefined
-fno-sanitize-recover=all`, with **no sanitizer exclusions**; it reports no
sanitizer errors on this executed path. An early compilation attempt failed
because this new worktree lacked generated headers/the initialized engine
submodule; using the existing identical generated headers and pinned engine
interface resolved compilation without installing or modifying dependencies.

The complete all-version project/preset migration matrix, factory/default/undo,
backup/editor/MIDI/UI regressions and PCM comparisons were **not run**, because
production conversion work stopped at the mandatory prerequisite. No claim is
made that those I1 requirements passed. The new gate is not inserted into the
normal regression runner as if it were a passing test. Existing assertions and
goldens are untouched. This is a documented pre-existing safety failure, not a
new production regression or a sanitizer failure.

Production code/RAM delta is zero by source scope; no native size, target link,
FM1 flash-fit or hardware timing measurement was performed. No audio code or
experimental build flags changed, but that is not presented as a new PCM test.

## Reproduction

Run from this branch's worktree, with `BASE` a clean checkout of the exact base,
`GEN` existing generated headers for it, and `ENGINE_INCLUDE` a compatible
initialized firmware/src include root for its pinned engine interface:

```sh
BASE=/path/to/clean-ca62bb1
GEN=/path/to/existing/build/gen
ENGINE_INCLUDE=/path/to/initialized/firmware/src
cc -O1 -g -w -ffp-contract=off \
  -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$GEN" -I"$ENGINE_INCLUDE" \
  "-DWIN_I1_BASE_PERSISTENCE_SOURCE=\"$BASE/tests/persistence_test.c\"" \
  tests/grain_win_i1_downgrade_gate.c -lm -o /tmp/grain-win-i1-gate
/tmp/grain-win-i1-gate
```

Expected at ca62bb1: the logged overwrite and exit1. Fixture hashes are in
`tests/fixtures/grain-win-i1/README.md`. Do not regenerate fixtures or substitute
a patched reader/writer while claiming this is the exact-base downgrade oracle.

## Smallest safe next decision

Approve a **separate downgrade-bridge checkpoint** that preserves unsupported
project objects and blocks normal save/erase/rename/backup replacement of them,
including after flash fetch and reset. That checkpoint would become the supported
downgrade floor. Test unknown magics, boot/load/save and physical NOR no-write
behavior before resuming I1. Arbitrary earlier firmware cannot be made safe by a
change to the new image alone.

Alternatively, explicitly narrow the downgrade policy to load rejection only,
with exported backups and no writes under old firmware. That is a weaker contract
than the preservation gate exercised here and is **not assumed approved**.

After the downgrade policy is resolved, resume frozen fixtures for every legacy
version and full I1 migration/default/protocol tests. I2 remains production DSP
spawn-time WIN capture using original A; I3 remains device/browser UI. Neither
renderer promotion nor hybrid activation is part of this checkpoint.

No merge, deployment, flashing or toolchain installation.
