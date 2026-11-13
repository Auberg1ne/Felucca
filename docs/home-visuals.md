# Animated scientists HOME preview

Base bbfbd03e25bbe9760e3e780d03f96d08374204c8. This completes the HOME renderer
using the supplied reference and separate derivative poses. Hardware fit and
real-time timing remain unverified; do not release or flash as validated firmware.

## Layout, controls and assets

The display is 240x240 RGB565. A compact 24-pixel header shows COFFEE GRAINS,
actual selected channel, BPM, transport state, octave, recording and battery.
Existing status messages temporarily use the existing header. Four neutral
procedural knob indicators occupy y=24..51. K1..K4 and `--` reserve label/value
space; their neutral pointer is not a reading or a new parameter assignment.
Physical knob mappings are unchanged. The existing footer remains at y=202..239
with steps, help, engine, sound name and HOME/navigation context.

Four 116x72 panels at (2,54), (122,54), (2,128), (122,128) hold separately decoded
90x56 sprites, CH labels and small procedural meters. CH1 flask/cyan, CH2
clipboard/yellow, CH3 microscope/magenta and CH4 celebrating/green are fixed to
actual part indices 0..3. Faces/accessories are authored from the approved
reference, not a scaled full UI screenshot. White labels remain readable even
when the existing palette's mono font ramp would suppress cyan/green ink.
Sprites, labels and selection frames are separate; character activity never
changes which channel is selected. ALGORITHM and other existing selection paths
still set song.sel. The bright frame moves at the next UI draw, without waiting
for the animation clock. Idle and silent selected channels remain highlighted.

The approved original PNGs, supplied SHA256SUMS and README are retained in
assets/home-scientists/reference. The generated derivative pose sheet and its
conversion are documented in assets/home-scientists/AUTHORING.md. Regenerate
with `python3 tools/gen_home_sprites.py` (Pillow 13.0.0). There are twelve distinct
poses, sharing one indexed RGB565 palette; no label, meter, knob, value or frame
is baked into them. The source hashes and decoded frame hashes are recorded.

## Animation and rendering

Only the existing independent home_level_pub signals are read. No audio callback
code, metering, sample, MIDI, input or serialization changes are made. UI-only
hysteresis enters activity at Q15 level 96 and rests below 48; it enters energetic
mode at 9000 and leaves below 7000. The underlying attack/release envelope remains
unchanged. Quiet/medium activity alternates idle and active poses at slower
cadence; loud activity alternates active/energetic at up to 10 pose changes/s.
The shared 50-ms snapshot clock bounds all animation updates to 20 Hz. ANIM OFF
uses static intensity poses instead of cycling. Returning from hidden views
resets animation phase; hidden pages/menus/layers/dialogs/count-in/sleep/startup
never advance the characters. The approved startup bitmap/deadline are unchanged.

Only changed sprite rectangles (90x56) or meter rectangles (5x46) are copied.
An unchanged resting view causes no tile transfers. Selection redraws only old
and new procedural borders plus the compact header/footer when needed. First
entry/forced reconstruction draws complete panels. The existing canvas is reused;
no image transformations, allocation or full-screen framebuffer are added.
The old tempo-roll history is synchronized while HOME uses a static BPM label,
so leaving HOME preserves the existing other-page transition behavior.

## Storage and hardware status

| Item | Bytes |
| --- | ---: |
| Compressed 12 sprite pixel streams | 29571 |
| Shared RGB565 palette | 512 |
| 13 offsets | 52 |
| Total sprite payload | **30135** |
| UI state, including existing 20-byte snapshot view | **44** |

The 48-byte audio meter state already existed in bbfbd03 and is unchanged.
Sprites decode directly into the existing canvas; no sprite RAM buffer is added.
Other tiny procedural constants and UI code are additional flash, not included
in the asset payload above. Samples 380334 + splash 38755 + sprites 30135 =
**449224 bytes**, leaving an arithmetic **132340 bytes** for all code, other
constants, load data and alignment in the 581564-byte image slot. This is not
actual linked headroom.

A pose-only update transfers 10080 RGB565 bytes; a meter update transfers 460.
Updating both selection borders transfers 2944 bytes, excluding header/footer.
At the absolute 20-Hz bound, four changing sprite rectangles would transfer
806400 bytes/s; normal energetic cycling changes poses at up to 10 Hz. Driver
DMA setup, meter/header/footer traffic and decompression also cost time. These
are static bounds, not measured hardware frame-time/CPU guarantees.

The new renderer defaults on for browser builds. Target artwork is opt-in with
`FELUCCA_HOME_ART=1`; use `FELUCCA_BOOT_ART=1 FELUCCA_HOME_ART=1 ./build.sh` to
attempt a target fit check with both image features. Default target legacy
rendering remains available until fit is proven. Target build stops here with
`build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)`.
WASM build stops with `emu: no emcc (Emscripten)`. No toolchain was installed and
no hardware was flashed. Actual target ELF size/headroom, hardware timing and
physical-screen appearance are still unknown. Native browser-backend rendering
and a 240x240 review screenshot are provided instead.

## Validation in this environment

The actual native browser backend produced the 240x240 screenshot in
home-scientists-preview.png. Its UI test injects separate level samples to show
mixed intensities; it is not a screenshot of a live WASM build. Original/derived
artwork is visually reviewed at native size, with readable CH labels and
recognizable faces/accessories. No runtime screen-sized animation frames exist.

- 89 HOME checks under ASan/UBSan: channel/character state independence, quiet and
  loud hysteresis boundaries, selection persistence even during other channels'
  activity, immediate selection via the actual ALGORITHM encoder, 50-ms rate
  limiting, HOME-only phase advancement, readable labels in GREY, canvas/blit
  bounds, zero transfers for unchanged resting frames, meter-only partial copies,
  ANIM OFF and fixed storage/state sizes.
- 208 input snapshots with new and old renderers have identical channel/parameter
  state, navigation and every captured non-HOME framebuffer, including transitional
  tempo digits. No golden file is changed to obtain this result.
- Full legacy UI: 850 assertions passed. Startup: 15 checks passed, including
  exact splash pixels, 3499/3500-ms boundary, wrap and MIDI audio under the splash.
- The two-second heavy four-part song is bit-identical with HOME visuals on/off:
  705600 float32 bytes, SHA-256
  66cf06868a1ef248a04257b3a71c31d60bb33e7cff80e89b709ed024161b6ce7.
- Asset tests verify both supplied reference checksums, exact regeneration,
  all twelve distinct decoded-frame hashes and every compressed stream's bounds.
  Startup and factory asset integrity tests also pass.
- Chromium frontend 20 checks; editor order 48; backup 64. Actual browser WASM
  compilation/execution is blocked by missing Emscripten, not claimed as tested.

Integration sanitizer runs retain the repository's existing DSP exclusions for
signed overflow, shift, bounds, object-size and pointer-overflow, with ASan still
checking memory. Sprite stream lengths and decoded output bounds are independently
checked in Python and the UI test instruments every canvas blit.

Changed areas: home_scientists.c, its HOME dispatch in ui_draw.c, optional target
build flag, generated sprite sources/assets and converter, focused sprite/UI/
navigation tests, host runner and documentation. Audio callback, metering producer,
MIDI, physical input processing, sample/storage and serialization sources are
unchanged. No dependencies are installed, and no hardware is flashed.
