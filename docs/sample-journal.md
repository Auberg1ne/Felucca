# Sample transaction provenance and header integrity (C2/H2)

The 240 KiB pool remains at `0xA0000..0xDBFFF`. In the new format its first
4 KiB sector is an append-only ownership journal. It leaves 59 sectors for
sample extents shared by eight public logical slots (USR1..8). The original
three-slot geometry defines the pool boundary only, never eight fixed partitions.
There are no writes outside the existing pool. Public enumeration does not change
any descriptor, journal, quarantine, deletion or reservation rule.

## Trust boundary

Recovery reads the SMJ1 anchor only at `SMP_USER_BASE`, never by searching audio.
Every old or new payload WRITE starts at least 512 bytes after an extent base,
and no extent can start before the pool. Thus payload bytes cannot reach the
anchor's first bytes. Old END accepts only FSMP headers. FSMP and SMJ1 magic
patterns are bitwise incomparable (checked by a static assertion), so a torn
NOR header program cannot turn either one into the other.

This fixed physical root is the provenance mechanism. CRCs and final commit
markers check integrity and completion; they do not establish provenance.
After anchor installation, allocation permanently excludes its entire sector.
No client offset, slot header, descriptor or audio bytes select journal addresses.

The journal is installed only after verifying that the **entire pool is erased**.
There is no automatic migration that overwrites or adopts existing samples.

## Journal and recovery

The anchor is a 32-byte record with fixed version and geometry. The rest of the
sector holds 127 records of 32 bytes, using the existing descriptor-sized layout:

- CLAIM reserves a new, nonoverlapping extent for one logical slot and its next
  generation. `reserved` is its physical sector index. The claim is committed
  before any sample-sector erase or payload write.
- PUBLISH must match that slot's latest pending CLAIM (extent, length and next
  generation). It is committed only after the FSMP header, SMA1 descriptor and
  audio have been verified. This journal entry, rather than an SMA1 found by
  scanning, authorizes the sample.
- DELETE advances the slot's committed generation, clears its pending claim and
  publishes an empty logical slot. It does not erase audio or change old zones.

All journal records are written body first, verified, then committed with the
last word. Journal order and exact next-generation checks resolve ordering;
there is no serial-number guess across unauthenticated historical descriptors.
The bounded journal cannot accumulate a half-range of generations.

Recovery never scans payload sectors for SMD1, SMJD, SMJP or SMA1 authority. An
unfinished upload can contain an entire counterfeit sample or journal and it
still has no publication/deletion authority. A valid PUBLISH selects only its
claimed extent and must match the actual sample descriptor, complete header
CRC and audio CRC.
An unusable latest sample is hidden; recovery never resurrects older samples.

Invalid, conflicting, skipped, out-of-sequence or torn journal records quarantine
the entire pool and hide its samples. Nothing is erased or recycled. An I/O
error latches unresolved state, retains all reservations and disables persistent
storage writes until restart; subsequent sample mutations return failure.
A busy chip at startup is not recovered or written. Background storage backend
checks also prevent an already-started transfer from bypassing this latch.

## Version-2 publication integrity (H2)

The FSMP header remains version 1, exactly 480 bytes; audio begins at +512.
SMA1 descriptors and SMJP PUBLISH records now use version 2, still 32 bytes.
The SMJ1 anchor and CLAIM/DELETE remain version 1 with unchanged geometry.
Versions are checked per operation; unknown versions lock recovery.

| Record | Offset 16 (`data_crc`) | Offset 24 (`reserved`) |
| --- | --- | --- |
| SMA1 descriptor v2 | audio CRC-32 | complete FSMP header CRC-32 |
| SMJP PUBLISH v2 | complete FSMP header CRC-32 | physical sector index |

CRC-32 is the existing zlib-compatible implementation, computed over **all 480
exact stored bytes**: name, reserved bytes, active and inactive zones included.
No header bytes are normalized or excluded. CRC zero and FFFFFFFF are valid;
version and an explicit running-map validity flag distinguish protection.
The header itself still contains the audio CRC, so journal -> header -> audio
forms the integrity binding. The descriptor must independently match both the
journal's header CRC and the header's audio CRC, length, slot and generation.
Descriptor and journal record CRCs and final commit markers remain unchanged.

END applies H1 semantic checks and the audio CRC before writing the header or
descriptor. It verifies exact readback, commits the descriptor last, verifies
header/descriptor/audio integrity, then commits PUBLISH last. No header or SMA1
can authorize itself without that fixed-root journal publication. A cut before
PUBLISH retains the previous protected publication; a torn journal tail locks
the pool. An uncertain write retains reservations and the existing I/O latch.

Recovery verifies the latest protected publication before exposing a map. Bad
sample integrity makes that slot unresolved, with no historical fallback and no
sector release; structural journal errors lock the entire pool. DELETE barriers
remain independent of corrupt replacements. Metadata consumers use a checked
header snapshot, including INFO, backup and playback scans. An exposure failure
latches the slot unresolved and hides further metadata, preserving validated RAM
reader state and every flash reservation. The C2 uncertain-I/O latch blocks new
metadata XIP reads without discarding the running CRC or RAM zones. Audio
renderers do not hash per frame.

The expected CRC belongs to the **running** physical map. END publishing B must
not overwrite A's running CRC or zones while B awaits reboot. Reboot establishes
B's binding only after validation. Running CRC values plus explicit flags cost
40 bytes across eight slots before linker alignment. Header snapshots replace playback's zone scratch
copy and add stack usage to verification/export paths; target linking and stack
measurements remain necessary for the physical FM-1.

Any historical **version-1 PUBLISH**, even deleted or superseded, quarantines
**the entire pool**: all 60 sectors reserved, no published samples, no sample
mutations or export. This also applies to mixed v1/v2 histories. Recovery does
not calculate a CRC over old bytes and silently endorse them. Every old byte is
preserved; no automatic migration, formatting, compaction or reclamation occurs.
Version-1 anchor/CLAIM/DELETE histories without a v1 PUBLISH remain usable.

Older firmware that only accepts v1 journal records locks on v2 PUBLISH. Other
downgrades are unsupported. CRC-32 detects corruption; it is **not cryptographic
authentication** and cannot protect against intentional collisions or an actor
rewriting header, descriptor and journal consistently. It cannot reconstruct
previously corrupted unprotected metadata. Verification is at metadata ingestion
and recovery, not continuous detection of faults during rendering or transfer.

The editor upload wire layout is unchanged: clients send the original 480-byte
FSMP header and firmware creates the integrity records. Restores regenerate the
descriptor/publication instead of trusting an uploaded descriptor. The browser's
JavaScript protocol mock has no physical journal/power-loss model and cannot
prove H2 durability. The separate WASM fixed-address sample override remains
unchanged; passing protocol tests is not proof of relocated sample playback.
FM-1 hardware support is retained; flashing remains deferred until target build,
memory and hardware validation are complete.

## Deliberate availability limits

There is no compaction or extent reclamation in this conservative repair. Every
CLAIM remains reserved even after abort, replacement, deletion and reboot.
Existing SAMPLE, GRAIN and SLICE readers therefore retain their flash bytes.
When audio space or journal space is exhausted, mutations fail without erasing
user data. DELETE needs journal space, but no free audio sector. A separate,
reviewed compaction/migration design is needed to regain capacity.

Unanchored FSMP legacy and relocated SMA1 media are quarantined: **all 60
sectors are reserved, no samples are published, and sample-pool mutations fail**.
Every flash byte is preserved, including apparently erased gaps. Recovery never
infers ownership from raw headers, CRCs, generations or commit markers. INFO and
sample backup/export expose empty slots; project selections use the existing
missing-sample behavior. There is no automatic migration or formatting.

## C3 containment

A completed FSMP header with an erased SMA1 descriptor can be an interrupted
transaction in the wrong logical slot, byte-identical to a genuine legacy sample.
Likewise, unfinished audio can contain a complete counterfeit SMA1 sample.
Neither format proves ownership. Recovery therefore never scans unanchored
media for publication, including genuine legacy images. Legacy availability is
intentionally sacrificed to preserve data safely; migration is outside this repair.

Only an exact SMJ1 anchor at the fixed pool root authorizes journal recovery.
A nonblank pool with FSMP or erased root magic is quarantined; other invalid
anchors and invalid journal records are locked. Both hide samples and reserve
the entire pool. Only a completely erased pool can initialize a new journal.
Detected I/O failures retain the existing write latch. XIP reads have no error
status, so undetected hardware faults returning erased bytes remain outside
this software guarantee. Journal transaction layout and ordering are unchanged.

Deletion is logical, not secure erasure. Downgrading to a firmware that scans raw
SMA1/SMD1 and does not honor this journal is unsupported and can resurrect data.
The provenance boundary covers the firmware's sample API, not an actor able to
replace firmware or directly program raw flash.
