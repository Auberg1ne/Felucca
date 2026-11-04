# Sample transaction provenance (C2)

The 240 KiB pool remains at `0xA0000..0xDBFFF`. In the new format its first
4 KiB sector is an append-only ownership journal. It leaves 59 sectors for
sample extents. There are no writes outside the existing pool.

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
claimed extent and must match the actual sample descriptor and audio CRC.
An unusable latest sample is hidden; recovery never resurrects older samples.

Invalid, conflicting, skipped, out-of-sequence or torn journal records quarantine
the entire pool and hide its samples. Nothing is erased or recycled. An I/O
error latches unresolved state, retains all reservations and disables persistent
storage writes until restart; subsequent sample mutations return failure.
A busy chip at startup is not recovered or written. Background storage backend
checks also prevent an already-started transfer from bypassing this latch.

## Deliberate availability limits

There is no compaction or extent reclamation in this conservative repair. Every
CLAIM remains reserved even after abort, replacement, deletion and reboot.
Existing SAMPLE, GRAIN and SLICE readers therefore retain their flash bytes.
When audio space or journal space is exhausted, mutations fail without erasing
user data. DELETE needs journal space, but no free audio sector. A separate,
reviewed compaction/migration design is needed to regain capacity.

Existing valid FSMP legacy and relocated SMA1 images remain readable through
compatibility recovery, with **all 60 sectors reserved and mutations disabled**.
Experimental unanchored SMD1 records are never deletion authority. Old SMA1 has
no provenance root and can itself be impersonated by crafted audio; compatibility
reads retain the old validation/conflict checks, but never grant deletion or
sector-reuse authority. No writable adoption of that history is attempted.

## C3 interaction

The old header-first upload ordering and legacy migration format are unchanged.
C3 remains an outstanding compatibility/migration issue. In the new anchored
format a header without journal PUBLISH cannot become visible, incidentally
containing that failure mode without implementing a legacy C3 migration repair.

Deletion is logical, not secure erasure. Downgrading to a firmware that scans raw
SMA1/SMD1 and does not honor this journal is unsupported and can resurrect data.
The provenance boundary covers the firmware's sample API, not an actor able to
replace firmware or directly program raw flash.
