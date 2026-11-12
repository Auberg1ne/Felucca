# FUN11 project storage

FUN11 repairs the FUN10 parameter/tail overlap without changing the persistent
project allocation, storage backend, sample IDs, or FM6 runtime/editor protocol.
The new wire magic is `0x46554E42` (FUNB); the internal project/cache magic remains
FUNA. Old firmware rejects FUNB. New firmware imports FUN1 through FUN10 as before,
including DIGITAL, PHYS and retired SAMPLE PERC migrations.

The object remains 3840 bytes. Header fields, parameter-count mapping, four track
records, nine-byte steps, 84-byte chain and 388-byte motion retain their FUN10
layout. With 118 parameters the variable payload ends at offset 3324.

| Region | FUN10 | FUN11 |
| --- | --- | --- |
| MIDI routing (3 bytes) | 3309 | 3373 |
| Four FM6 patches | 3312..3823 (512 bytes) | 3376..3823 (448 bytes) |
| Name (12 bytes) | 3824 | 3824 |
| FNV-1a checksum (4 bytes) | 3836 | 3836 |

Each patch contains 128 seven-bit values. FUN11 stores value bit j at stream bit
`7*i+j`, least-significant bit first within each byte. Each patch occupies exactly
112 bytes, with no unused padding bits. All four patches are independent and
lossless. The new writer rejects values above 127 before packing. There is no
change to factory patches, sound synthesis, the FM6 bank, or user preset encoding.

The 49 bytes from 3324 through 3372 are zeroed spare space. Compile-time and runtime
bounds protect the MIDI/FM6 tail. Import validates the version, object size, hash,
parameter count and layout before reading records. FUN10 keeps its original raw
patch and MIDI offsets; an overlapping FUN10 image is rejected rather than
reinterpreted as FUN11. Corruption checks use the existing non-cryptographic hash.

`tests/project_format_test.c` covers every parameter on four tracks, GRAIN
settings/automation, all seven-bit values at all FM6 byte positions, full motion
and chain data, mixed MIDI channels, sample IDs 5..12, independent FUN10 fixtures
with counts 103..114, missing-parameter defaults, PERC migration, every single-bit
image corruption, truncated images, and rehashed malformed records. Existing
project/backup tests exercise older formats and actual save/restore paths.
