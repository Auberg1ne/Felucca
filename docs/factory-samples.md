# Five factory sample selections

Source IDs are a persisted ABI, distinct from factory table and preset indices.

| Source ID | Sound | Factory/preset index |
| --- | --- | --- |
| 0 | GUITAR | 0 |
| 1 | GUITAR alias (retired TRANH) | 1, hidden |
| 2 | VOICE | 2 |
| 3 | HIT | 3 |
| 4 | GUITAR alias; historical SAMPLE PERC imports become DRUM | 4, hidden |
| 5..12 | USR1..USR8 | user slots 0..7 |
| 13 | FLUTE | 5 |
| 14 | SAX | 6 |

`SMP_NSETS` remains the frozen five-entry legacy prefix; `SMP_NFACTORY` is
seven including aliases. Explicit mapping resolves public sources to factory or
user slots. Five distinct presets are shown: indices 0, 2, 3, 5, 6. Existing alias,
PERC migration and favourite indices remain unchanged. The waveform display and
SAMPLE/GRAIN use the same mapping. SLICE source 4 keeps its exact shared GUITAR
audio and now displays GUITAR; the internal SLC_PIANO identifiers remain stable.
No user flash allocator, journal, migration or simulator code is changed.

Old saved projects retain their selections and audio. New FLUTE/SAX projects
require this firmware's source map; older firmware has no corresponding selections.
FUN11 serialization and the 3840-byte project allocation are unchanged.

## Provenance audit

FLUTE (three zones) is documented as Versilian VSCO-2 Community Edition material;
SAX (three zones) is documented as Versilian VCSL material. The exact upstream
paths and CC0-1.0 dedication are in assets/samples-cc0/ATTRIBUTION.txt. These existing
WAV files are used unchanged.

GUITAR, VOICE and HIT first appear together in repository commit
b7bbe615ff9cee9bf6ce9b461385f862d33e999a, "Coffee Grains development checkpoint
before Codex review". Each assets/custom-samples/NAME.wav is byte-identical to
assets/samples-cc0/NAME/NAME_C3.wav. All are mono, 16-bit PCM at 44100 Hz.

| Asset | Frames | SHA-256 (both copies) |
| --- | --- | --- |
| GUITAR | 73071 | 9907ef1bfc03b4fe9b2acb941c1c06c5a68b70255f1aa83917cfded97089cb62 |
| VOICE | 154350 | 93ba5ce8c749491eb3fbb2a4cf4c03d96c34c5f3fbfd4ccfaf65095bc79c1335 |
| HIT | 50939 | 8bcf3632acb59fa20b7adfcc5fc89590e43e6ab801cc1380bdafa0abe868a96e |

The commit records who added the files, not who created or licensed the recordings.
Neither the supplied attribution manifest nor inspected WAV chunks identifies a
verifiable source, permission or asset-specific licence for these three sounds.
Their presence under samples-cc0 is not evidence of CC0 status. The repository's
general code licence does not resolve this provenance gap. No licensing claim is
made here; owner/source/licence documentation remains outstanding. Their files,
processing, existing zones and generated ADPCM prefix are preserved exactly.

## Measured storage

At the existing 22050-Hz IMA ADPCM settings (generator alignment included):

| Sound | Zones | ADPCM bytes |
| --- | --- | --- |
| GUITAR | 1 | 11026 |
| VOICE | 1 | 10474 |
| HIT | 1 | 11026 |
| FLUTE | 3 | 31422 |
| SAX | 3 | 31422 |
| Factory total | 9 | 95370 |
| Existing SLICE BREAK | — | 22050 |
| Total generated SMP_DATA | — | 117420 |

The increase is 62844 ADPCM bytes plus six zone records, two factory/preset
records and source metadata. This is compiled firmware data, not user-pool space.
A target link is required to measure final flash usage and establish fit.

Tests: factory_assets_test.py verifies immutable custom WAV hashes, unchanged
legacy ADPCM/zones, aliases, missing-library ABI and exact added sample sizes.
factory_sample_test.c exercises five distinct decoded audio renders, real preset
browsing, waveform selection, SAMPLE/GRAIN project and preset round trips,
favourites and historical PERC migration. Existing format and eight-slot tests
cover persisted user IDs and older project imports.
