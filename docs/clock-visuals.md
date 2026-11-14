# COFFEE GRAINS CLOCK preview

Based on `cd71462b180b95d4a5ec51e396fb039b5ae54c4a`. CLOCK remains HOME 2/2
with its original input mappings, global parameter descriptors and footer.
The browser-default skin follows `COFFEE_HOME_VISUALS`; target artwork remains
opt-in. `COFFEE_CLOCK_VISUALS=0` provides the original view for comparisons.

At 240×240, the compact header is followed by three 80×44 procedural controls.
BPM and SWG reuse MIXER's normalized Q15 80 ms envelope, monotonic millisecond
clock, 50 ms delta cap and exact settling. Their numerical values always format
the actual globals immediately. CLK is discrete (INT/USB/TRS), using an instant
three-position indicator. The shared routines were moved without modification
into ui_knob_motion.c; MIXER's appearance and behavior are unchanged.

The central ring has sixteen procedural coffee beans, grouped in four muted
cyan/yellow/magenta/green quarters using the existing palette. Its highlighted position
is the selected track's real seq_idx modulo 16, consistent with the footer's
pattern view, rather than an invented global clock. Patterns longer than sixteen
steps wrap around the ring; the footer retains its existing bank navigation.
Different track divisions, swing, USB/TRS synchronization and tempo changes remain
owned by the sequencer. The pending-start sentinel displays step one rather than
the previous pattern's last step. Stopped transport shows STOP and no active bean.
When an external clock pauses, the view follows the sequencer (including its
existing clock-loss timeout); display time alone cannot advance a beat.

No bitmap or scientist decoration was added: the 122-pixel central area prioritizes
beat and source readability. The existing bean footer is reused. Only CLOCK's
visible view advances its cache; screen sleep and page transitions invalidate it.
Large-text preference is retained while this view uses its fixed compact geometry.

Additional persistent cache: 32 bytes. No framebuffer, allocations, audio callback
work, sample/storage/serialization/MIDI changes. The existing canvas is reused.
Initial ring render: 29,280 pixels; a beat update changes at most two 13×15 nodes
plus a 72×48 center (3,846 pixels / 7,692 RGB565 bytes). Each top control blits
3,520 pixels only when its text/position changes or on a forced redraw. Read-only
position table: 32 bytes, parameter IDs: 3 bytes, plus labels/code. Actual target
flash delta and hardware timing/headroom require a successful pi32v2 link.

## Validation

ASan/UBSan, using the existing DSP exclusions documented in mixer-top-knobs.md:
4,599 CLOCK checks, 7,583 MIXER checks, 32 smoothing checks and 850 UI assertions
passed. CLOCK covers live values across full ranges, immediate discrete source,
settling, physical edits, hidden-page inactivity, layout bounds and real USB/TRS
Start/Clock/Stop/reset events covering all sixteen positions, plus internal swing.
Pixel assertions verify cardinal playhead frames and the recorded-note footer bean.
No bounds violations or text clipping were observed. Non-CLOCK pixels, input
state, all track parameters and globals matched across 270 navigation snapshots.
Old/new audio matched all 705,600 bytes, SHA-256
`c311ea3939bbcbe9e82af419809fcf47ada77d2d5bafa9532df91f3eaadd5024`.
The frontend branding smoke passed 20 checks. `git diff --check` passed.

Browser build attempt: `emu: no emcc (Emscripten)`.
Target build attempt: `build.sh: no toolchain in /home/agent/.jieli/toolchain
(run tools/get_toolchain.sh)`. No tools were installed. Native browser-backend
rendering was inspected at 240×240. There is no newly linked WASM or target ELF,
so target flash fit and physical LCD performance remain unverified.
