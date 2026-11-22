# Step 240C — new-firmware preset-bank ownership guard

Base: `39b2ee66a9c2b50daf6c61fd6233b2c1162d511d`.
Evidence checkpoint: `6062014872f4f3e2bb9a247cf610066e46406524`.
Branch: `feat/coffee-grains-granular-quality`.

This is a NEW-firmware guard, not protection against an unchanged older binary.
ID118 remains unallocated. No parameter, record version, geometry, DSP, UI
navigation, sample or audio-path change is made.

## Preserved evidence

The separate evidence commit contains exactly the previously uncommitted
`preset-bank-downgrade-feasibility.md` and
`preset_bank_downgrade_feasibility_test.c`. Their text/conclusions were preserved;
references to “uncommitted” in that historical report describe its original state.
On that checkpoint the strict probe still passes **8229 checks documenting unsafe
behavior**, including erasure of a future-format copy during a successful save
and during a failed save before its first payload write, in both A/B directions.
Step 240A still demonstrates its three unsafe baseline cases: truncated bank,
unknown bank header and damaged storage CRC. These expectations are not changed.

`run_grc_migration.py --oracle-tree` compiles those historical tests against a
clean, exact evidence checkout, while compiling the persistence and ownership
suites against the current sources. It verifies the oracle revision and tracked
source cleanliness. This separates reproduction of the old defect from testing
the new guard. No binary fixture or historical unsafe assertion was regenerated.

## Mutation-path audit

Preset bank0 occupies `0xDC000/0xDD000`; bank1 occupies
`0xDE000/0xDF000`. Each object has two 4096-byte sectors. Payload remains 3080
bytes: an 8-byte UPB1 header plus sixteen 192-byte records.

| Entry point / source | Mutation and protection |
|---|---|
| `upreset.c:up_store_cat`, `up_store` | Native SAVE/overwrite builds a record, then enters `up_put`. Physical ownership is checked before changing its cached record/header. |
| `upreset.c:up_rename_cat`, `up_rename` | Rename/category edits converge on the same `up_put` guard. |
| `upreset.c:up_put(k,NULL)` | Existing record erase (including confirmed `CF_ERASE_USER` in `ui_input.c`) rewrites a bank through the same guard. Favorites/loaded labels/generation change only after success. |
| Editor UP_PUT / WIN-aware bulk PUT | Both converge on `up_put`; incoming invalid records are rejected before mutation. Existing provenance checks remain. |
| `editor_backup.c:ed_bk_commit`, bank IDs6/7 | Full-bank replacement and zero-length backup-clear validate incoming data and use `up_bank_save`. Cache replacement happens after success only. Both physical copies are inspected even if the cache looks empty. |
| `upreset.c:up_bank_save` | Chooses the alternate copy from a complete physical inspection, calls the guarded writer, and performs narrowly owned synchronous failure cleanup as described below. |
| `storage.c:st_save`, `st_save_to` | Common erase/program/recycling path. In the firmware application, every call targeting either preset-bank object inspects BOTH physical copies before the first erase, including explicit A/B target requests. Selection/sequence come from that inspection, not a later fallback-capable read. |
| `upreset.c:up_boot`, `up_bank_check`; `project.c:persist_boot` | Boot/refetch initializes/normalizes RAM only, never reformats bank sectors. Ownership is recomputed from NOR on every write. Favorites cleanup skips protected banks. Losing the cache or rebooting cannot grant ownership. |
| INIT/CLEAN / factory initialization | `ui_input.c` confirmed sound INIT/CLEAN call the existing sound initialization functions; `main.c` initializes live tracks. These do not erase preset banks. There is no application-wide preset-bank factory-reset bypass to preserve or newly authorize. Existing backup-clear remains available on owned banks only. |
| `up_fm6.c:upf_boot`, `upf_migrate`, `upf_save` | Retired FM6 migration affects the separate patch sidecar (`OBJ_UPFM6`), not preset-bank sectors. `up_store_cat` writes an FM6 sidecar only after its guarded record save succeeds. Shipping GRAIN-only restrictions remain. |
| `fm6_vbank.c` / autosave / settings / projects | Other storage objects/addresses; none erase `0xDC000..0xDFFFF`. Existing project ownership protection is unchanged. |
| Sample journal/allocator and update loader | Sample pool ends at `0xDC000` (exclusive). Loader `FL_RANGE_OK` excludes preset-bank sectors. No changes to these components. |

The actual bank-sector low-level mutations are `st_save_to`'s initial erase,
payload page programs and final commit-header program, plus the new transaction
cleanup erase. `storage_hw.c:st_erase/st_prog` remain address/readiness drivers;
they are not a persistent ownership mechanism. No additional caller writes these
bank addresses. External programmers, vendor recovery and different/older
firmware are outside this application guard's authority.

## Ownership and recovery policy

`up_bank_inspect` uses existing `st_copy_state` independently for each copy:

| Physical state | New-firmware mutation policy |
|---|---|
| Entire sector erased | Owned empty space; permitted. |
| Valid storage header/CRC, correct bank shape, supported compatible records | Owned; ordinary SAVE/recycling permitted. Versions1..7 remain supported. |
| Valid zero-length commit with erased gap/tail | Existing deliberate clear tombstone; permitted. |
| Unknown bank/header/version or incompatible record | Protect whole bank, including a suspect copy older than the selected valid copy. |
| Truncation, orphan payload, invalid header/CRC, non-erased padding or malformed nonempty sector | Protect whole bank; metadata failure never means empty. |
| Read/transport failure or unavailable flash | Refuse mutation. |

Non-USED records must be canonically all zero, as existing empty-record writers
produce. Nonzero unused records cannot silently be discarded. Existing used
records retain their accepted historical field/pattern migration behavior; no
new interpretation of reserved parameter bytes is introduced.

The two-copy scan is bounded; it does not use `st_load`/RAM-cache absence as
permission. It checks exact payload length, UPB1 shape, record versions and the
existing shipping compatibility policy, with erased padding and CRC validation
provided by `st_copy_state`. Both copies must be owned/erased. The guard does not
change which readable fallback `up_boot` may offer.

`ST_OWNER_DENIED=-20` is private and explicitly means no erase was attempted.
Existing caller results remain: native save returns failure with a read-only
message; editor backup returns its existing failure status. No new protocol or
confirmation UI is introduced.

### Interrupted writes

A synchronous `up_bank_save` transaction first establishes that its target is
owned. If its write fails, it may erase **only that transaction's incomplete
invalid target**, keeping the previous valid copy. It never cleans up following
`ST_OWNER_DENIED`, a valid commit, an erased target or an uninspectable target.
Cleanup is attempted once, without retries. If cleanup fails, later writes are
refused. This uses immediate transaction ownership, not invalid metadata alone.

After an abrupt power loss that transaction witness no longer exists. A partial,
nonempty target is therefore protected after reboot even when the other copy is
readable. This deliberately restricts older automatic “recover then overwrite”
behavior. A cut immediately after erase leaves an erased target and is writable;
a fully committed supported target is also writable. No destructive recovery
command is added. Preserve a raw dump/verified backup before separately approved
recovery of a quarantined bank.

Flash writes are serialized in the existing main loop. No ISR/CPU2 writes these
bank objects. No allocations, locks, audio callback work or persistent ownership
state is added. The existing scratch buffer is reused. A concurrent external
writer or spontaneous storage corruption cannot be made transactional by this
application check; failing reads/CRC checks are conservatively refused.

## Validation

Strict tests use `-fsanitize=address,undefined -fno-sanitize-recover=all`, without
signed-overflow/shift exclusions in ownership, migration or NOR tests.

| Gate | Result |
|---|---|
| New physical NOR ownership suite | **26219 checks pass**. Both banks/copies, suspect older/newer, unsupported8/9, unknown shape, truncation, CRC/header corruption, orphan/gap/tail data, read faults, stale cache/refetch, all save/erase/rename/backup-clear routes, cross-bank isolation, favorites preservation, sequence wrap and versions1..7. |
| Synchronous errors / power loss | Fail every payload/header program, failed initial erase, failed cleanup, backup replacement/clear errors; abrupt reset after erase, each payload page and commit header in both bank/copy directions. Protected operations assert no erase/program, raw byte identity and unchanged banks/tracks/projects/song/favorites/generation. |
| Step240A frozen model/oracle | Standalone model passes; **132084 checks pass** against the clean evidence revision; all three historical unsafe counterexamples retained. |
| Original feasibility probe | **8229 checks**, expected unsafe outcomes on the evidence revision, unchanged. |
| Current WIN persistence/NOR | **5804 checks pass**, both shipping GRAIN-only and diagnostic multi-engine builds. |
| Project ownership bridge | **2189 checks pass**. |
| Canonical editor parameter gate | **110 checks pass**. |
| Generic storage and user-preset suites | Both pass under strict sanitizers. |
| Granular regression | All **24** frozen PCM/state captures and independent process repeat pass. RNG, scheduling, admission, decoder-load/state and parameter/UI mappings unchanged. |
| Strict short-envelope kernel | **152924099 checks pass**. |
| Strict STUTTER gate kernel | **42069041 checks pass**. |
| Strict filter-transition kernel | **3553312 checks pass**. |
| Broad historical `persistence_test.c` | Both evidence and current builds report the same **19 FAIL lines**, then SIGSEGV; line-buffered logs are byte-identical. Obsolete multi-engine/legacy fixtures remain unfixed. This suite is not claimed green; the focused physical-NOR gates above complete successfully. |

The 24-case full mixer fixture retains its already documented signed-wrap/shift
sanitizer exclusions. The focused strict numerical kernels and ownership/NOR
suites have no such exclusions. No new failure was observed in the exercised
gates. No assertion was weakened and no DSP golden was changed.

Reproduce after existing generated headers and pinned engines are available:

```
git worktree add --detach /tmp/coffee-preset-evidence-606 6062014872f4f3e2bb9a247cf610066e46406524
# Initialize that worktree's pinned firmware/engines using the existing local
# checkout (or the repository's usual submodule procedure); no new tooling needed.
python3 tests/run_grc_migration.py \
  --oracle-tree /tmp/coffee-preset-evidence-606 --regressions \
  --out /tmp/preset-guard-240c
```

The runner also verifies all binary fixture hashes remain unchanged.

## Resources and limitations

Same host `cc -Os -ffp-contract=off -fstack-usage` build of
`grain_win_persistence_test.c` against evidence/current sources:

| Host ELF measurement | Before | After | Delta |
|---|---:|---:|---:|
| text/rodata (`size` text column) | 544786 | 545814 | +1028 bytes |
| data | 16688 | 16688 | 0 |
| BSS | 2470696 | 2470696 | 0 |

These are linked **host test harness** measurements, not FM1 image sizes.
No new production buffer, persistent field or engine state is allocated.
Host compiler stack reports: `up_put`272→288, `st_save_to`144→160;
new `up_bank_save`96 and `up_bank_inspect`160 bytes per call frame, with
`up_bank_known`16 and `st_copy_state`32. These are individual host frames,
not a measured target high-water mark. They do not execute on the audio path.
Repeated bounded physical scans add main-loop flash I/O at boot/save; target
latency and stack need hardware qualification. No target link/flash-fit claim is
made and no toolchain was installed.

The bridge is complete for the audited application's preset-bank mutations.
It does not preserve data against already-shipped firmware, external flashers or
vendor reset tools, and cannot reconstruct data already erased. Keep verified
format-aware backups plus raw BOTH-copy dumps before downgrade/recovery. No
persistent marker is claimed to influence binaries that never inspect it.

**Next safe ID118 step:** use this new-firmware checkpoint as a minimum supported
preset-bank downgrade baseline, then separately implement the version-aware
parameter/count and project/preset provenance design proved in Step240A. Legacy
imports must initialize C=0; new formats must preserve C explicitly. Do not
activate C DSP/UI/MIDI routing in that storage slice. Old unsupported binaries
still require external backup and an explicitly controlled downgrade policy.
