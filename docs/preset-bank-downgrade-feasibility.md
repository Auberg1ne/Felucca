# Step 240B — preset-bank downgrade feasibility (option B)

Baseline: `39b2ee66a9c2b50daf6c61fd6233b2c1162d511d` on
`feat/coffee-grains-granular-quality` (production identical to0da978b).

**Blocked for protection against the already-shipped baseline.** No firmware
bridge, marker, parameter, format, audio or reset change is implemented. A new
writer cannot make an unchanged older reader honor an ownership rule it lacks.
The safe outcome is a documented downgrade policy and a separately scoped,
forward-only protection checkpoint. This report and its host-only probe are left
uncommitted; there is no safety-bridge commit.

## Source trace

| Source / symbol | Verified baseline behavior |
|---|---|
|`upreset.c`, UP_BANK_MAGIC/up_rec_t/up_bank_t|UPB1 header;16 records x192 bytes plus8-byte bank header =3080 bytes. Preset versions1..7 accepted. Unknown used records are preserved only after a bank passes shape/loading checks.|
|`upreset.c:150`, up_bank_check|Length, bank magic, record size or slot-count mismatch zeroes the RAM cache. Invalid used records inside a valid bank are retained. This does NOT erase NOR at boot.|
|`upreset.c:307`, up_boot|Loads each bank through st_load, then normalizes the cache. Missing/corrupt/unreadable load and an erased bank are not independently classified for ownership here. Unsupported records also appear unused to normal browsing; favorites handling can clear their in-memory favorite bits.|
|`upreset.c:325`, up_put|Refuses replacing an unsupported USED target record. It is NOT a physical whole-bank ownership guard. Writing another slot is allowed and carries retained unknown records along when the cache is intact. It recreates a normal header and saves the complete bank.|
|`upreset.c:423/464`, up_store_cat/up_rename_cat|Normal STORE, rename/category changes and erasure/replacement converge on up_put. Failed st_save rolls back the selected RAM record/header, not an already-erased physical sector.|
|`storage.c:79/91`, st_head/st_body|Validates commit header and payload CRC. Bad CRC/read/shape does not confer ownership. No preset-schema marker is consulted.|
|`storage.c:137/160`, st_current/st_load|Selects newest CRC-valid copy, falls back to the older CRC-valid copy. Returns -1 if neither can be used or length exceeds destination capacity. Physical nonemptiness is lost from this load result.|
|`storage.c:172`, st_save_to|Chooses the copy other than the current valid one, or copy0 if neither is valid. Erases the complete4KiB sector BEFORE programming payload; commit header is written last. It does not check preset-bank ownership.|
|`storage.c:113`, st_copy_state|Already distinguishes erased, valid, nonempty-corrupt and I/O-failed physical copies for project safety. Preset-bank boot/write paths do not use it as their ownership gate.|
|`editor_backup.c:54`, ed_bk_object|Bank IDs6/7 expose the current RAM bank image, not both physical sector copies. This is not a forensic raw-NOR backup.|
|`editor_backup.c:141/189`, ed_bk_commit|Checks incoming bank shape and cached/input grain_bank_readonly. After a successful save it replaces/normalizes the cache. A zero-length restore is an existing deliberate bank-clear path, subject to those cache-based guards.|
|`project.c:1367`, persist_boot|Checks flash readiness and loads settings/projects/preset banks. A latched flash fault can block hardware writes, but CRC/shape failures do not universally establish such a latch. This is not persistent preset ownership.|
|`storage_hw.c:29/42`; `hal/fm1_flash.h:53/171`|Address bounds/readiness protect the flash driver. Permitted preset sectors can still be erased; the driver does not inspect their stored schema.|
|`loader/loader.c`; `loader/ldr_core.c`|The included updater's bounds deliberately exclude the user-store range while installing an app. Package CRC/app checks are not a preset-schema anti-rollback policy. Once an old app runs, its own data writer can erase those banks.|

Physical bank0: copyA0xDC000, copyB0xDD000. Bank1: copyA0xDE000,
copyB0xDF000. Each sector is4096 bytes; payload starts at256. A3080-byte bank fits
the existing3840-byte storage payload limit. No bank geometry change is needed
for a future new-firmware guard.

### Defaults, initialization and reset

INIT/CLEAN in `ui.c` reset the selected sound, not the preset-bank sectors.
Invalid bank initialization currently clears RAM; the physical reformat happens
on a later STORE/rename/erase/restore through st_save. An intentionally empty
bank restore writes a zero-length storage object. No behavior is changed here.

No separate firmware-wide factory erase of preset banks was identified in these
application paths. Stock/vendor factory recovery and external reflash behavior
cannot be established from these application sources; they are not declared safe.

## Reproduced failure and power-loss behavior

The existing Step240A oracle still demonstrates all three inherited unsafe cases:
truncated bank, unknown bank header, and damaged storage payload CRC. Cache
normalization drops the protection and a later STORE can replace the data.

`tests/preset_bank_downgrade_feasibility_test.c` adds a strict simulated-NOR
counterexample using the existing candidate-version model, NOT a new marker:

1. Commit a supported bank, then a newer valid candidate8/C55 bank to the other
   sector. Corrupt the newer copy's payload CRC while leaving its C data present.
2. Boot the baseline: it selects the older supported copy and no longer sees the
   candidate record in its cache.
3. A successful STORE erases/replaces the newer sector containing future data.
4. With failure injected before the first payload program, STORE reports rc2,
   but that future-data sector has ALREADY been erased. RAM rollback and keeping
   the older valid copy do not restore the lost future bytes.

Both A/B directions are tested. Another case proves the nuance of the valid-bank
protection: STORE to a neighboring slot rewrites the bank but preserves the
unknown record bytes. Thus “read-only bank” in earlier summaries must not be
interpreted as “the baseline performs no bank writes.” The record and backup
checks provide narrower guarantees than a durable physical ownership bridge.

The probe passes8229 strict checks because these are EXPECTED unsafe baseline
outcomes. It is not a passing safety implementation. Existing two-copy atomicity
protects the selected CRC-valid object; it does not promise to preserve suspect
or unrecognized physical data in the other sector.

## Why a new marker cannot retrofit safety

- A marker in an unknown bank header makes the baseline clear its cache.
- A marker inside a future record is consulted only after bank loading/shape/CRC
  succeeds; the demonstrated corruption and fallback paths bypass it.
- A marker in reserved commit fields/padding is not an ownership condition in
  st_head/st_current/up_put. st_save_to erases the sector and writes its own
  header/reserved fields, so those bytes are not durable against old writes.
- A marker elsewhere can survive but is ineffective if the old app never reads
  or honors it. Duplicating markers does not change that fact.

Mirroring complete future banks can mitigate a single-copy failure when another
valid, sufficiently recent future copy survives. It cannot guarantee preservation
through mixed-version/interrupted states, both unusable copies, or an old writer
advancing a stale snapshot over the suspect sector. It is not a full ownership
bridge.

No marker is proposed or installed. In particular, changing bank magic or hiding
a format to make new firmware reject it would not make old firmware safe.

## Protection classes

| Class | Feasible guarantee |
|---|---|
|Preventable in NEW firmware|Inspect both physical copies before cache normalization and before every bank mutation. Quarantine unsupported/nonempty-corrupt/unreadable banks, even when an older supported copy exists. Preserve raw bytes and fail atomically. Known-valid banks and genuinely erased banks remain usable.|
|Requires backup/recovery|Recover original future presets after an old writer erases them; preserve suspect raw data before recovery; restore format-aware data through compatible firmware. CRC detects damage but does not reconstruct absent C values.|
|Requires additional trusted enforcement|An enforced bootloader minimum-version/anti-rollback policy, or hardware write protection an old app cannot remove, could prevent unsafe old execution/writes. Neither is established as available by this report. A passive bootloader or persistent marker alone is insufficient.|
|Fundamentally unpreventable by a NEW app alone|Once an unchanged old app or external flasher has permission to erase these sectors, the new app cannot intercept that erase. Already-erased values cannot be recovered without a surviving copy or external backup.|

A future fail-closed bridge may quarantine a legitimate old interrupted save as
well: after reset there may be no durable evidence distinguishing its orphan
payload from future unknown data. Recovery must be explicit; silently cleaning
that sector would reintroduce the downgrade hole. Such policy needs separate
review/testing, not an assumed marker-based solution.

## Safest deployment and backup policy

1. Keep ID118/formats unactivated until a preset-bank physical-ownership bridge
   and the version/provenance changes are validated. Publish that new bridge as
   the minimum supported downgrade checkpoint; do not label39b2ee6/0da978b fully
   safe for damaged future banks.
2. Before any downgrade, while compatible tools still operate, retain a verified
   format-aware full backup including projects, settings/favorites, both preset
   banks and associated FM6 user patches. Keep the original immutable with its
   checksum and producing firmware/schema identification.
3. Also obtain a read-only physical dump of BOTH copies of both banks (or full
   NOR) where a trusted recovery method supports it. Preserve sector headers,
   payloads and even invalid copies, with SHA-256. Do not rely on editor cache
   exports after validation has already emptied that cache. No hardware dump was
   performed in this task.
4. Verify the backup off-device and qualify restoration on compatible firmware
   before risking the originals. Reading an archive successfully is not proof
   that the old firmware can restore a new format.
5. Do not boot an unpatched baseline with future banks still on-device. If that
   downgrade is unavoidable, first preserve/verify the originals and prepare an
   explicitly approved legacy-compatible bank using a qualified converter.
   Dropping personality-C is lossy and must never happen silently. No such
   converter or destructive preparation is implemented here.
6. On any corruption/unknown-format indication, stop normal preset writes and
   preserve raw data before recovery. Intentional clearing/factory recovery is a
   separate destructive operation, not proof that downgrade preservation works.

## Validation and scope

Commands executed:

```
python3 tests/run_grc_migration.py --regressions --out /tmp/preset-bridge-audit-validation
cc -O1 -g -w -Wno-implicit-function-declaration -ffp-contract=off \
  -fsanitize=address,undefined -fno-sanitize-recover=all \
  -Ibuild/gen -Ifirmware/src tests/preset_bank_downgrade_feasibility_test.c \
  -lm -o /tmp/preset-bank-feasibility-final
/tmp/preset-bank-feasibility-final
```

- Step240A:132084 strict model/oracle checks, standalone model pass.
- New feasibility probe:8229 strict checks; four unsafe fallback/write outcomes
  across both copy directions, plus neighboring-slot preservation.
- Persistence/NOR:5804 checks pass; binary fixtures unchanged.
- All24 granular PCM/state fixtures and independent process repeat pass.
- Strict short-envelope152924099, STUTTER42069041, filter-transition3553312
  checks pass. The mixer fixture retains its documented legacy arithmetic
  sanitizer exclusions; model/NOR/probe and strict kernels have no exclusions.
- No new regression in these requested gates. The unsafe counterexamples are
  inherited production behavior, not sanitizer failures or a new bridge.
- Production source, formats, IDs, DSP, UI, factory-reset behavior and firmware
  text/RAM/pool: no change. No target build, toolchain installation, push, merge,
  release or hardware flashing. No bridge commit is made.

**Next safe implementation step:** separately implement a NEW-firmware-only
preset-bank ownership guard using the existing st_copy_state physical probe,
covering boot/refetch, STORE/PUT/erase/rename, backup replacement/clear, read faults,
CRC fallback and interrupted writes. Keep it independent of C and preserve the
bank layout. Establish its limited downgrade policy and explicit recovery tests
before introducing future formats. It cannot retroactively secure old binaries.
