# COFFEE GRAINS startup splash

Preview branch base: 3f747b7e6da70487e32cbb4370b38fe15dfb313e.
The FM1 uses a 240 x 240 ST7789-class RGB565 SPI display. Pixels are sent
high byte first; its DMA source must be RAM, not flash. The browser emulates
the same 240 x 240 framebuffer and panel byte order.

## Source and conversion

The approved source remains unchanged at
`assets/startup-splash/coffee_grains_boot_splash_source.png` (1402 x 1122).
SHA-256: `68e4a4c9190aeb789f88c408c6d67dfc71b68bf92f742bc4ea205ac8cfebed30`.
The supplied ZIP SHA-256 is
`a7cc3b680f5b7def24a2d9d1d2cd850b8641279523d55baf4c58dc1b700cdc6f`.
Its checksum file is retained verbatim. No attached instructions are executed.
No new ownership/licensing claim is made for the user-supplied artwork.

Reproduce with `python3 tools/gen_boot_splash.py` (Pillow 13.0.0 for the
checked-in results). The existing Inter Tight font, SIL OFL 1.1, is used at
11 px for a crisp duplicate credit in the lower letterbox margin:
**CREATED BY Auberg1ne**. The full illustration is uniformly scaled with Lanczos
to 240 x 192 (nearest integer dimensions), placed at (0,12) on black. No crop
or independent-axis stretch is used. All three scientists, FM1, logo, original
subtitle and original credit remain. The converted PNG is the exact RGB565
preview, not the higher-precision intermediate image.

The conversion quantizes to a deterministic 256-color palette without dithering,
then stores the palette as panel-byte-order RGB565 and compresses each row with
PackBits-style literal/repeat tokens (1..128 pixels). A 241-entry uint16 row
index permits independent rows. Committed outputs are boot_splash.h,
boot_splash_240.png and conversion.json; tests regenerate and compare them.
Different Pillow/font rasterizer versions can change output and must be reviewed.

## Storage and hardware status

| Component | Bytes |
| --- | ---: |
| Encoded pixels | 37761 |
| RGB565 palette | 512 |
| Row index | 482 |
| Total asset, excluding code/alignment | **38755** |
| Uncompressed RGB565 | 115200 |

The renderer uses the first 240 pixels of the existing cv_px RAM canvas, waits
for the preceding DMA before reusing the row, blits one RAM row, then syncs the
last transfer. It adds no framebuffer or persistent allocation. No audio engine,
factory sample, user storage, preset or project-format behavior is changed.

Artwork is enabled by default in browser builds. Hardware retains its legacy
text rendering until explicitly built with `FELUCCA_BOOT_ART=1 ./build.sh`, which
passes COFFEE_STARTUP_ART=1 to the compiler. This provides the same hardware-sized
renderer for a fit check without presenting unverified firmware as ready.
Both paths use the 3500-ms startup deadline.

Sample data 380334 + artwork 38755 = **419089 bytes**, leaving **162475 bytes**
of the 581564-byte image slot for code, other constants, metadata, initialized
load data and alignment. This is an arithmetic remainder, **not actual linked
headroom**. Target build attempt stopped with:
`build.sh: no toolchain in /home/agent/.jieli/toolchain (run tools/get_toolchain.sh)`.
No target ELF or firmware size could be produced. Hardware fit, timing and color
appearance remain unverified; do not release/flash this preview as validated.
The WASM attempt stopped with `emu: no emcc (Emscripten)`.

## Timing and initialization

The shared helper uses unsigned elapsed fm1_ms ticks, including wrap-around,
with an explicit pending flag so timestamp zero is valid. The picture is drawn
once and retained through 3499 ms; at 3500 ms (or the next hardware UI iteration)
the splash clears and the normal synth UI is forced. No loading animation.

Hardware's old 400-ms delay loop is removed: initialization proceeds, and its
regular main loop services watchdog, USB/editor, MIDI, master gain and queued
storage services while gating only normal UI input/drawing. The existing 30-ms
input-settling delay and user-invoked calibration/update paths are unchanged.
The clock starts after rendering; hardware initialization before interrupts
start can add a small unmeasured interval. Browser audio continues before its
deadline check. Its elapsed clock follows audio worklet processing; suspending
the AudioContext pauses emulated device time. MIDI can play under the splash;
local keyboard/UI hold is released automatically at the deadline.

## Validation

`python3 tests/boot_splash_test.py` verifies the approved source hash, exact
regeneration, all 240 compressed row bounds, RGB565 bytes and preview hash.
`web/emu/boot_splash_test.c` runs the real browser backend natively: exact
framebuffer pixels, no early redraw, zero-start and wrapping clocks, 3499/3500-ms
boundary, automatic HOME transition and audible MIDI during the hold.
Browser startup/audio/XIP tests now wait for the intended longer splash rather
than assuming the old 430-ms startup. The WASM smoke test retains its 200-ms LED
sweep check, verifies the splash at 1 s, then expects HOME after 3.5 s.

Validated here under ASan/UBSan with the existing DSP exclusions (signed
integer overflow, shift, bounds, object-size and pointer-overflow): startup
15 checks, UI 850, factory playback/compatibility 232, project-format 41146,
SYNTH audio 10, browser branding 8 and physical XIP 34. No sanitizer diagnostics.
Chromium frontend smoke 20, editor order 48 and backup 64 checks also pass.
Python verifies reproducibility and every row's decoded size and pixel hash.
Native ELF symbols confirm 37761/512/482 asset bytes; these host sizes do not
establish target linker headroom. Actual WASM smoke execution remains blocked
by missing Emscripten, and target compilation by missing JieLi prerequisites.
