# Factory samples — browser audition preview only

Base: fc0b2bf240913eafcf49cf466401c62cb0af124c. Hardware fit is **unverified**.
Do not release or flash this preview without a successful target link within the
581,564-byte image limit and target performance validation.

| Public ID | Sound | Factory index | Frames | Aligned IMA bytes |
| --- | --- | --- | --- | --- |
| 0 | CELLO | 0 | 176924 | 88462 |
| 2 | FLUTE | 2 | 125317 | 62660 |
| 3 | GUITAR | 3 | 73071 | 36536 |
| 13 | SYNTH | 5 | 275098 | 137550 |
| 14 | VOX | 6 | 66150 | 33076 |

Browsing order is the table above, then USR1–USR8. Public user IDs 5–12,
factory aliases 1/4 to 0, preset indices, favourites and historical PERC ID 4
SAMPLE-to-DRUM imports retain their existing ABI. Saved projects keep their IDs
but intentionally play the replacement audio. FUN11 and its 3840-byte allocation
are unchanged. The user journal/allocator and 240-KiB user flash pool are untouched.
SLICE source 4 is still named GUITAR and now shares the complete source-ID-3
recording at its native 44.1-kHz rate; internal SLC_PIANO names remain unchanged.

## Originals and processing

`assets/factory-preview/` contains the exact supplied WAVs (including the name
`SYNTH wav.wav`), supplied manifest and SHA256SUMS. The source ZIP SHA-256 is
`bc1e122319c16e3e086efa10135d219b8b5f72b1badb6afc1583bc21b4290047`.
The manifest is data, not executable instructions. All files are mono signed
16-bit PCM at 44100 Hz. No original is rewritten, trimmed, resampled, normalized,
faded or crossfaded. Every PCM frame is encoded with the existing lossy IMA
ADPCM codec, predictor/index initially zero, and each byte stream aligned to two
bytes. Original gain is retained at encoder input; codec quantization changes
waveform values. Existing engine envelopes, filters and track gain still apply.

CELLO's embedded `smpl` metadata specifies MIDI root 60, no fractional tuning,
one forward loop from frame 0 through 176923 inclusive, indefinitely. The
generator validates and preserves those values and decoder state; no crossfade
is introduced, so a loop boundary may be audible. The other four recordings have
no verified sampler root or loop metadata. They are non-looping and use an
explicit **MIDI-60 audition anchor**, not an assertion of their actual pitch.
Actual root tuning and desired loop points remain unresolved. Auditioning MIDI
60 with zero transposition plays their native speed. No automatic pitch detection
or invented sustain loops are used.

The five-source total is 358284 bytes, plus unchanged SLICE BREAK 22050 bytes:
**380334 bytes of SMP_DATA**, up 262914 bytes. This leaves **201230 bytes** for
all code, metadata, other assets and initialized load data, not measured linked
headroom. Metadata and source names also occupy flash. Native-rate decoding can
cost more CPU than the former 22.05-kHz assets; hardware timing remains untested.
Raw 16-bit PCM alone would cost 1433120 bytes and does not fit; IMA is required.

## Reproduction and future replacements

Run `python3 tools/gen_samples.py build/gen/felucca_samples.h` after preparing
normal build prerequisites. Both target and browser builds call this generator;
the output remains preview-only until hardware fit is proven. Its cache includes
the manifest, original assets and generator code. Missing/mismatched files or
unexpected PCM formats fail generation rather than silently substituting audio.
For future recordings preserve original filenames and bytes, update manifest
hashes/frame counts, explicitly review tuning/loop decisions, update inventory
expectations and rerun size, decode, mapping and compatibility tests.

Run `python3 tests/factory_assets_test.py`, native factory/sample-order and
project-format suites, `node web/test_sample_order.mjs`, and browser branding
smoke tests. Normal Emscripten builds use `sh web/emu/build.sh`.

## Provenance

These files were supplied for this preview; no new licensing claim is made.
GUITAR is byte-identical to the previously supplied custom GUITAR recording.
Its creator/source/licence remains unresolved, as do redistribution provenance
and licence documentation for the other supplied recordings. The old Versilian
FLUTE/SAX files retain their original attribution but are no longer selected.
No supplied asset is labeled CC0 merely because of a directory name. Existing
custom/CC0 source files are retained unchanged for historical reference.

## Preview validation

Full UI also passes all 850 assertions under the same sanitizers.
Seven relevant suites passed under ASan/UBSan (using the host DSP exclusions
for signed overflow, shifts, bounds, object-size and pointer-overflow): factory
selection/playback 232 checks, sample order 802, project-format 41146, FUN10 51,
presets 45, Editor 172, robustness 13. Native browser branding adds 8 checks;
physical XIP/startup adds 34. Browser frontend smoke tests pass 20 checks with a
stubbed worklet; actual editor dropdown navigation passes 90 Chromium checks
against freshly generated firmware descriptors. Editor ordering passes 52 checks
and backups pass 64. Source checksums, encoded fingerprints, missing/corrupted
original rejection and cached regeneration are verified. Generated headers,
binaries and caches are not committed.

WASM build attempt: `emu: no emcc (Emscripten)`. No full WASM/audio browser build
was produced in this environment. Native rendering/decoding passes; this is not
a substitute for a WASM rebuild in the Emscripten-equipped Codespace. Target
link and hardware size/performance verification remain outstanding.
