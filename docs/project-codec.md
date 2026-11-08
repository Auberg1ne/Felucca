# Coffee Grains FUNB project codec

FUNB is project format 11 (numeric magic `0x46554E42`). Firmware writes FUNB and reads valid FUN1–FUNA records. Older firmware may reject FUNB; no downgrade conversion or automatic reinterpretation is provided. Storage remains a 3,840-byte payload in the existing 4 KiB A/B object sectors. The storage transaction and sample pool are unchanged.

## Losslessness gate

The omitted bit is bit 7 of the old packed step metadata byte, not bit 7 of runtime `step_t.flags` (which contains nudge). Packed FUN7–FUNA validation rejects that metadata bit for every step, including inactive steps. Exhaustive parser tests cover all 256 metadata bytes at the first and last steps of each track in all four packed legacy versions: 8,192 checks. Earlier raw FUN1–FUN6 records have no such packed metadata byte. Their existing import and runtime bounding rules remain in place; unusual accepted raw note counts/times are not claimed to be bitwise preserved through an explicit normalized save. Validated legacy flash records are retained verbatim in the cache, so simply loading them no longer requires a successful newer serialization.

The canonical step tuple retains four 8-bit note/nudge bytes, seven metadata bits (3 count, 2 time, 2 flags), velocity/ratchet (8), hit (8), accent (8), probability/ratchet (8): 71 bits. All runtime nudge and ratchet bits survive. Bits are emitted LSB first within each field, continuously across 64 steps per track: 568 bytes. There is no per-step padding. Existing note/velocity clipping and accent-within-hit normalization are unchanged. Invalid count/time/probability is rejected, not truncated.

## Exact current layout

Offsets are zero-based; multibyte integer representation follows the existing little-endian firmware format. The numeric magic is stored little-endian (bytes `42 4E 55 46`), consistent with prior versions.

| Offset | Bytes | Content |
| --- | ---: | --- |
| 0 | 68 | Existing header, globals and parameter count (118 at byte 66) |
| 68 | 2752 | Four tracks: 118 parameter bytes, engine, preset, 568 step bytes each |
| 2820 | 84 | Complete chain, including inactive rows and reserved bytes |
| 2904 | 388 | Complete motion block, including 128 events |
| 3292 | 17 | Zero padding |
| 3309 | 3 | Existing MIDI channel/route encoding |
| 3312 | 512 | Four 128-byte FM6 patches |
| 3824 | 12 | Uppercase, zero-padded name |
| 3836 | 4 | Existing FNV-1a hash over bytes 0–3835 |

The outer storage CRC and header-last A/B commit mechanism remain unchanged. The old 118-parameter FUNA body needed 3,855 bytes: 15 beyond capacity. FUNB saves 32 bytes, leaving 17 spare bytes and remaining exactly 3,840 bytes long. There is no chain compaction.

FUNB rejects malformed step fields, unsupported parameter counts, invalid engines, chain/motion structures, noncanonical names, non-seven-bit FM6 bytes and invalid/reserved MIDI channel encodings, in addition to hash/size/version failures. Legacy parsing retains its prior tolerances and migrations. Checksums detect accidental corruption, not malicious forgery.

## Compatibility and backup

Factory SAMPLE/GRAIN IDs 0–4 and USR1–USR8 IDs 5–12 are retained. FUNB SAMPLE source 4 is the current GUITAR alias. Pre-FUNB records keep the existing historical SAMPLE PERC-to-DRUM migration; no ambiguous assetless selection is inferred. New-format GRAIN/USR8 and all 13 selections round-trip.

Legacy cache records retain their original length and bytes. Backup exports that validated length; restore validates known legacy sizes, imports with existing migrations/bounds, then writes FUNB using the existing transaction. Runtime capture and LIST now fit without changing backup IDs or protocol framing. Backup restore remains non-atomic across objects. No editor permissions are widened.

## Verification and limits

`tests/project_codec_test.c` exercises the real project, backup and RAM-NOR storage paths: metadata gate, 4,544 bit-position cases, all source IDs, all step flag bytes, raw chain preservation, motion/MIDI/FM6, FUN1–FUNA import, LIST/restore, repeated saves, truncation/corruption, 3,840-byte canaries, 7,746 partial-program cases, 34 partial erases and four lost-ack/readback cases. It also runs with strict ASan/UBSan. Test-only NOR programming obeys one-to-zero semantics.

Host fault injection does not establish physical flash timing or hardware power-loss safety. Target linking, RAM/stack measurements and physical FM-1 validation remain outstanding. No additional global project buffer is introduced; the new step codec uses a nine-byte local tuple. Known unrelated legacy/UI test failures are reported separately.
