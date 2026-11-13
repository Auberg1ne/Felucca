# Four scientists HOME — audio infrastructure checkpoint

Base: c0d64dfb3fe4034d08272ab28084866bb964eb30.
This checkpoint deliberately contains **no new HOME artwork or animations**.
The workspace contains the approved three-character startup illustration, not
four standalone scientists or animation frames. In particular there is no CH4
raised-hands character. Per the requested fallback, no new character is invented
and the startup image is not cropped/shrunk into a substitute HOME layout.

Required future asset mapping (not yet rendered):

| Audio part / array index | Future scientist | Accent |
| --- | --- | --- |
| CH1 / 0 | goggles and flask | cyan |
| CH2 / 1 | glasses and clipboard | yellow |
| CH3 / 2 | cap and microscope | magenta |
| CH4 / 3 | raised hands, happy | green |

## Existing display and HOME constraints

The display is 240 x 240 RGB565, using a small RAM canvas and strip/row DMA.
HOME has a 24-pixel header, parameter cards above Y_GRAPH=76, a 122-pixel graph
region, and a footer at Y_FOOT=202 (38 pixels). It can show scope/tracks/levels.
Normal UI frames are approximately 15 ms apart. A native-resolution static layout
must be reviewed with appropriate character sprites before replacing this region
or rearranging any essential information. None of those pixels, navigation,
button/encoder assignments, saved display preferences or existing menus change
in this checkpoint. There are no extra draw calls.

## Actual audio source and smoothing

`fx.c` captures each part's final dry stereo L/R contribution after DIST, INSERT,
SLICER/performance mute, track LEVEL and PAN/SPREAD. It observes the exact values
added to that part's dry mix, including stereo difference content. It is not
MIDI/activity state, not a combined mix, not the old read-and-clear `track.peak`.
Shared reverb/delay/chorus returns and master processing have no unique part
owner and are deliberately excluded. Thus channel activity is independent of
the master output volume and other channels' FX tails.

Four bounded Q15 block peaks feed ~10-ms attack and ~200-ms release envelopes at
44100 Hz. Peak magnitudes saturate at 32767 only for display; the audio samples
are never clamped or rewritten by this instrumentation. INT32_MIN magnitude is
handled with unsigned arithmetic. Silent/early-returning parts reset their raw
peak each block, and their envelope decays to rest rather than sticking.
The original output expressions and mix additions remain mathematically intact.

Each part owns one raw word during rendering. In a two-core build the existing
C1_WAIT/csync join completes before core 0 updates envelopes and publishes four
independent aligned volatile uint32 words. The UI only reads those mailboxes:
it does not clear audio state. UI and publisher are on core 0 (main loop versus
audio ISR); single aligned 32-bit word loads/stores use the target's existing
interrupt-mailbox convention. A four-channel read is not promised to be a
simultaneous transaction; individual words can be from adjacent blocks, which is
safe for independent animations. This is not a portable C11 multi-thread UI API.
The browser worklet likewise owns its DSP state; a future JS UI should receive
copied snapshots through the existing message mechanism, not share C globals.
No new wait, lock, allocation, retry loop or extra audio traversal is introduced.
The existing second-core join is reused. The host two-core test uses the existing
pthread harness; its test-only locks are not production meter code.

## Prepared HOME-only consumer

`home_levels_poll` is an unused integration point pending approved artwork.
It suppresses snapshots for other pages, menus, layers, confirmation/name/update
screens, count-in, sleeping display and the startup keyboard hold. A caller-owned
view (zero initialized) can request a four-bit channel dirty mask at most once
per 50 ms (20 Hz). Call it each UI frame, including when hidden, to invalidate
cached visibility. No animation/rendering is wired into existing HOME.

The intensity bands are rest below 64, low 64..2047, medium 2048..8191 and high
8192..32767 (Q15). These are visual thresholds, not audio processing. Active
channels can request 20-Hz animation ticks; unchanged resting channels do not
request redundant redraws. The first visible frame requests all four channels.
Unsigned elapsed-time arithmetic handles clock rollover and timestamp zero.

## Resource and compatibility status

- Sprite/image storage added: **0 bytes**.
- Meter state: **48 bytes** of raw/envelope/published words (plus linker alignment).
- Future UI view: **20 bytes per caller-owned view** on the host; no such view is
  allocated in production yet. No new pool buffers/framebuffer or persisted data.
- Existing compiled sample data: 380334 bytes. Startup artwork: 38755 bytes when
  enabled. Their 419089-byte sum leaves an arithmetic 162475 bytes for all code
  and other data in the 581564-byte image slot, not measured linked headroom.
- Added target code size, actual RAM headroom and real-time CPU cost are unknown
  until a successful target build/link and timing measurement. Do not claim fit.
- Target build: `build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)`.
- WASM build: `emu: no emcc (Emscripten)`. No installation/download was attempted.
- Factory audio/IDs, SAMPLE/GRAIN/SLICE, user storage/journal/allocator, project
  format, presets and MIDI code are unchanged.

## Validation

`home_levels_unit_test.c`: 34792 checks under full ASan/UBSan, including integer
extremes, stereo cancellation, four independent levels, smooth attack/release,
rest, monotonic intensity, hidden/rate-limited snapshots, rollover and bounds.
`home_levels_test.c`: 82 checks under scoped ASan/UBSan, with real channel audio,
full left/right pan, SPREAD on/off, held notes at zero LEVEL, old-meter read/clear
independence, HOME/overlay/display/startup gating. The existing pthread C1 harness
also passes all 82 checks across rotating part splits. Integration sanitizers
retain the repository's DSP exclusions for signed overflow, shifts, bounds,
object-size and pointer-overflow; AddressSanitizer remains enabled.

For identical stimuli, 14336000 PCM bytes are bit-identical between enabled
metering, FELUCCA_HOME_LEVELS=0, simulated two-core rendering, and a probe compiled
against the untouched c0d64df base. SHA-256 of each stream:
`13db58d82f2d7815926006784aa3f80a0a259e6e161c8c919338d1b7b8b28566`.
The host runner retains disabled/enabled and two-core comparisons as regressions.
The full UI suite passes 850 assertions under scoped ASan/UBSan. Browser frontend
and backup/order checks are also run; actual WASM execution and hardware timing
remain unavailable. No visual sprite/readability tests can be claimed without
the missing four-character artwork.
