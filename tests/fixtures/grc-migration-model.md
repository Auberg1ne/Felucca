# Step 240A — version-aware personality-C compatibility proof

Production baseline: `0da978b6b356c7c2891151217337b7bdf42e920e`.
Everything added is test-only. ID118, format numbers and wire identifiers are
**not allocated in production**. P_COUNT remains118, P_E0 remains110, DSP/UI
and existing format readers remain unchanged.

## Reproduce

```
python3 tests/run_grc_migration.py --out /tmp/grc-model
python3 tests/run_grc_migration.py --regressions --out /tmp/grc-all
```

The runner requires production sources to match the baseline, checks the actual
version registry, uses strict ASan/UBSan with no exclusions for this model/NOR
oracle, and verifies every existing binary fixture remains byte-identical.
`grc_model_standalone.c` uses only standard C headers and the model header: it
needs neither production parameter definitions nor generated audio assets.
The separate oracle harness includes the unmodified physical NOR/backup tests.

## Mapping contract and fixtures

There are TWO explicitly selected decoders. Count alone NEVER selects a schema.

| Schema | Stored input | Canonical IDs0–109 | IDs110–117 (E0–E7) | ID118 |
|---|---|---|---|---|
|Historical tail-eight|np values|Existing common values/defaults|input[np-8..np-1]|0|
|Historical v4/count119|119 values|input[0..109]|input[111..118]|0|
|Proposed marked stable layout|Exactly119 values|input[0..109]|input[110..117]|input[118], 0..100|

Thus input118 is historical TONE/E7 in the v4/count119 example, never C. The
extra historical common position110 is ignored exactly as the old decoder does.
A test uses the actual production up_valid/up_params functions to demonstrate
that this record is accepted and mapped that way.

The historical decoder is checked against production params_by_count for every
count8..144, including truncations and defaulted common fields. Project fixtures
cover all supported versions1..12, legacy retained4840-byte objects and existing
reserved-WIN provenance cases. Preset fixtures cover versions1..7; generated
mapping vectors cover their accepted count boundaries. Inputs are immutable.
The existing5,804-check persistence suite adds the full historical WIN vectors.

Proposed C is one shared value per track, default0, not four remembered values:
TYPE0 SPREAD, TYPE2 CHAOS, TYPE1 DRIFT, TYPE3 SPARKLE. The model changes TYPE while
retaining that same value. Non-GRAIN C must remain0. All C values0..100 are checked
at vector/scalar level; project and runtime round trips
include0,1,55,100 independently across the four tracks; normal/grid preset
records test each value. IDs0..117 retain their
positions and values. The real fourth personality UI cell is still255/empty.

## Version availability — candidates only

At this baseline, project versions1..12 are registered; current magic is
FUNC/0x46554e43. Preset versions1..7 are accepted. Tests enumerate all256 preset
version bytes and unknown magic suffixes; correctly shaped fixtures prove the
accepted historical project versions.

Candidate FUND/13 (0x46554e44), normal preset8 and grid preset9 have no accepted
production allocation. FUND and version8 already appear in intentional negative
oracle tests; that is NOT an accepted-format collision. Those old expectations
and fixtures are preserved. Future allocation must be explicit and must not turn
this frozen downgrade oracle into a new-format reader.

## Frozen motion and MIDI contracts

Legacy motion uses frozen old count118 and old engine-tail mapping, never
`new_P_COUNT - old_count`. Every old ID/lock-bit combination is checked against
the existing proj_motion_ids implementation. For example, a count89 lock on85
maps to ID114, not115. Proposed marked-layout IDs0..117 stay fixed. New C motion/
locks are refused; no new modulation destination is defined.

The actual MIDI guard requires P_COUNT-P_E0==8. Existing ml_code(118) yields128,
which cannot fit its seven-bit code field and loses that code on16-bit packing.
No alternate encoding is invented. New C must remain quarantined from persistent
learn until a separately designed provenance/encoding migration exists.

## Exact storage model

Packed projects contain64 stored steps/track, not just the16 visible steps.
Each stored step occupies9 bytes. Chain and motion occupy84+388 bytes.

| Item | Historical118 | Proposed119 |
|---|---:|---:|
|Project allocation / retained slot|3840|3840|
|Track payload: params + engine/preset + steps|696|697|
|Four tracks end at byte|2852|2856|
|Chain + motion end at byte|3324|3328|
|Gap before MIDI|49|45|
|MIDI / FM6 / name / checksum offsets|3373 /3376 /3824 /3836|unchanged|
|Preset record|192|192|
|Packed preset values|118|119|
|Preset category index in packed[144]|143|143|

New C occupies one biased byte per project track or preset. The model preserves
all old parameter, step, patch, pattern and category fields. A preset has24
remaining non-parameter bytes before its category. No bank geometry grows:
two banks,16 records each,3080 bytes per bank including its8-byte header.

Raw runtime imports are separate from packed storage. Frozen runtime118 has
4840 bytes, tracks at66, checksum at4836. Proposed runtime119 has4848 bytes,
tracks still at66, checksum at4844. Explicit typed structures and offset checks
prevent a new sizeof(project_t) from deciding how to read old data. Historical
native imports are first normalized through the unchanged production reader,
then upgraded with C=0. Proposed runtime validates its own magic, size, checksum,
C and unchanged fields via a frozen legacy projection.

Packed project/proposed runtime checksums, unsupported versions, invalid C,
invalid counts, all truncation lengths, source immutability and staged-output
atomicity are checked. A low-bit flip at every byte of the modeled project object
is rejected. Preset records have NO per-record checksum: a structurally legal
parameter-byte change is deliberately shown to parse; bank/transport CRC and
ownership guards must provide that protection.

The legacy projection normalizes scratch spare bytes; it never rewrites the raw
source. This is not a promise to preserve undocumented spare fields through an
explicit future re-save. Raw downgrade preservation is tested independently.

## Editor provenance model — not a protocol implementation

Explicit scalar C requires discovered support, valid track, engine and0..100.
Unmarked legacy bulk always uses historical mapping and forces GRAIN WIN64/C0,
even for count119 or reserved109 values0/64/127. It cannot replace a nonzero-C
sound or an existing non-Hann sound. Marked appended-layout bulk requires a
matching capability and exactly119 values; unknown markers/capabilities fail.
Existing non-Hann bulk restrictions remain: a C marker does not confer WIN-aware
bulk provenance. No command numbers, capability tags or library versions are
allocated. The actual old UP_PUT parser rejects an unmarked119-value frame and
leaves its output record untouched.

All refusals precede applying values in the model. A production implementation
must validate BEFORE engine reset, audition SETs, undo capture or storage writes;
putting a guard after a reset would not provide the same guarantee.

## Exact-base downgrade results and inherited gaps

Valid modeled FUND objects are tested in all four real simulated NOR project
slots. Existing SAVE/SAVE AS/rename/erase/load, backup, cache invalidation and
refetch guards reject them without NOR erase/program calls, retained/song/track
changes or raw-byte changes. Backup restore of a candidate into an empty slot
also fails atomically.

Valid candidate preset8/9 records make the bank read-only. The oracle checks
normal/grid variants, both banks and first/last records (slots0,15,16,31).
STORE, replacement/erase, rename and backup restore refuse writes, preserving
bank bytes and NOR counters. Existing bank capacity and other-bank isolation
remain intact.

**Three inherited unsafe counterexamples are also retained and labelled:**

1. A committed future bank truncated by one byte.
2. A nonempty bank with an unknown bank header magic.
3. A future bank whose storage payload CRC is damaged.

In each case baseline up_boot/up_bank_check clears the unusable cache, no longer
marks it read-only, and permits up_store to replace it. The tests EXPECT and
print this unsafe baseline behavior; they do not count it as a safety guarantee.
They run only on simulated NOR, never hardware or actual user data.

Consequently0da is a useful valid-object downgrade oracle, but **not a complete
fail-closed preset-bank downgrade baseline**. A preset-bank physical-ownership
bridge is required before promising preservation of damaged/unreadable future
banks. New formats cannot retroactively repair unpatched older firmware. Project
B1 protection must remain intact; releases older than that bridge are not made
safe by this proof.

## Results and production blockers

- Latest model/oracle:132,084 strict checks passed; standalone model passed.
- Existing persistence/NOR:5,804 checks passed under strict sanitizers.
- All24 granular PCM/state fixtures unchanged, including RNG/admission/decoder
  observations; no new parameter is read by audio.
- Strict short-envelope152,924,099, STUTTER42,069,041 and filter-transition
  3,553,312 checks pass. Full mixer tests retain their previously documented
  arithmetic sanitizer exclusions; model and kernel tests have no exclusions.
- No unexpected test failures. The three downgrade counterexamples are existing
  production limitations, not new regressions. Broad historical Editor/Motion/
  robustness/MIDI/STRETCH failures were not rerun by this focused runner.
- Production text/data/BSS, pool, formats and IDs: zero change. Hypothetical
  decoded runtime grows8 bytes; actual firmware RAM/flash must be measured only
  after production implementation. No target fit or hardware readiness claim.

Before production: establish the damaged-bank protection baseline; approve the
actual schema-selected codecs and wire provenance contract; bound engine
lookups to110..117; preserve118-count FUNC and preset6/7 readers; freeze old
native imports; quarantine new automation/MIDI eligibility; audit initializers,
undo and browser tail mappings. Re-run this OLD oracle against actual new-format
outputs from a frozen checkout rather than weakening its rejection expectations.
