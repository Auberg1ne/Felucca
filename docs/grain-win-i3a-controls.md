# WIN I3A: device and simulator controls

Base: `838540d20ac6a8b9d2356793c979c9e8bf95bbfa`.
Branch: `feat/coffee-grains-win-i3a-controls`.

## Implementation

GRAIN EDIT page1 is now:

```
SRC      POS      SIZE     DENS
PTCH     G-SPRD   RAND     TONE
STRETCH  WIN      empty    empty
```

`params.c::GRAIN_GRID[0][2][1]` resolves to existing parameter109 through the
same `page_param_id/page_desc` path as rendering, physical encoders and modified
knob operations. No parallel binding, new page or row state was added. Page2,
all other grid positions, SELECT wrapping/entry, EDIT switching, GLO tempo
priority, track switching and the stored SELECT preference are unchanged.

`grain_edit_grid.c::draw_grain_grid` retains the normal numeric value and gauge.
At0/64/127 its existing unit line shows GAUSS/HANN/TUKEY; intermediate values
remain exact numeric morph values with no discrete-name implication. The
canonical descriptor remains integer0..127/default64, not an enumeration.
MIDI learn, locks/motion and matrix destination eligibility remain disabled for
WIN. Ordinary editing and existing temporary LFO-held knob edits work through
the authoritative resolver without creating automation records.

The browser simulator inherits this same firmware screen and K2 behavior through
its existing `web_enc` path. No window synthesis, alternative parameter store,
new knob mapping or new simulator-only control was implemented in JavaScript.
The separate browser editor uses its existing descriptor-driven range control.

## Browser controls and provenance

`web/editor.html` exposes the control only for:

- the known118/110 layout and a selected engine advertised as GRAIN;
- parameter109 with label WIN, integer format, range0..127, default64;
- no descriptor unit or named/discrete list.

Reserved, absent, wrong-format/default descriptors stay hidden. Stale UI events
are checked again before scalar requests. Existing `rereadEngineDesc` reads109
on engine and track changes. Browser anchor readouts include the numeric value
(`64 HANN`); intermediate values use the exact number.

For current four-track firmware, WIN uses existing `TRACK_PARAM` command31 with
an explicit track index and the canonical firmware descriptor. Coalescing keys
include that index; a delayed reply for a previous track cannot update the
newly selected track. Last-sent bookkeeping includes track identity and the
refreshed value, so sending0 on several tracks, or sending0 again after factory
reset to64, is not incorrectly suppressed. These changes are specific to WIN;
other parameter controls retain their existing request behavior.

All I1 library/file protections remain:

- Legacy file and audition bulk loops still skip109 after normal initialization,
  retaining Hann semantics for old JSON0.
- Non-Hann unversioned library export/upload/audition remains refused.
- The guard now checks the known layout rather than depending on cached
  `keys[109] == WIN`. That key may still describe the reserved slot of a previously
  selected non-GRAIN track. Either the supplied or advertised engine being GRAIN
  triggers the guard; a stale label cannot allow the transfer.
- Backend ambiguous legacy replacement refusal, FUNC projects,6/7 preset records,
  PER4 learn quarantine, raw backups and B1 ownership guards are unchanged.

No new command, capability tag, fixed CC, library version, learned mapping or
motion/matrix destination was added. The frontend keeps its ordinary existing
optimistic display cache; firmware parameter109 remains authoritative and is
read back in the integration tests.

The simulator currently emulates physical controls; its existing Web MIDI export
handles channel messages, not a new editor SysEx bridge. This task does not add
such a bridge. The standalone editor's scalar transport is tested against the
real native firmware handler. Firmware-backed simulator input/rendering is tested
through the native browser unity build. See the explicit WASM limitation below.

## Native screenshots and pixel evidence

All PNGs are actual240×240 RGB565 render captures expanded losslessly to RGB888,
without scaling, image generation or decorative substitutions. The third row
is selected to show the existing persistent highlight.

|Capture|Image|
|---|---|
|I2 page1 before|[Before](images/grain-win-i3a/page1-i2.png)|
|WIN0|[GAUSS](images/grain-win-i3a/page1-win0.png)|
|WIN64|[HANN](images/grain-win-i3a/page1-win64.png)|
|WIN96|[Intermediate96](images/grain-win-i3a/page1-win96.png)|
|WIN127|[TUKEY](images/grain-win-i3a/page1-win127.png)|
|Page2|[Before](images/grain-win-i3a/page2-i2.png) / [After](images/grain-win-i3a/page2-new.png)|

120 before/after frames cover four tracks, all three selected rows, nine WIN
values on page1 and page2. All43988 changed pixels are inside the existing WIN
cell rectangle **x62..118,y136..185**. Page2 is pixel-identical. A separate225-frame
navigation stream has changes only inside that cell in its nine GRAIN page1
frames; all other pixels and all captured navigation/parameter state match I2.
No unrelated snapshots/goldens were regenerated.

Dirty redraws for every value0..127 match a subsequent forced full redraw,
including anchor text disappearing on intermediate values. Text/bounds/contrast
hooks check all parameters, personalities, palettes and tracks. Native visual
inspection confirms that both number and anchor fit the existing cell.

## Test results

|Test|Result|
|---|---:|
|New shipping device UI: mapping, all values/four tracks, boundaries, modifiers, no learn/automation|62025 strict checks passed|
|Existing diagnostic grid/navigation suite, updated only for the new cell and disabled WIN learn|2447 strict checks passed|
|Browser-unity grid full-value/text/contrast/bounds suite|2191 checks passed|
|Native captures, dirty redraw and real simulator encoder export|120 frames;148 checks passed|
|Actual browser editor handlers + native firmware transport|71 checks passed|
|Existing browser provenance checks|11 passed|
|I2 strict morph numerical|85344961 passed|
|I2 strict actual grain kernel|3315208 passed|
|I2 strict production latching|1981446 passed|
|I1 physical NOR persistence|5818 passed in each shipping/diagnostic build|
|Exact B1 downgrade oracle|1654 passed|
|I1 canonical editor/learn quarantine|110 passed|
|User-record/parser suite|45 passing assertions,0 failing|
|Default final-output Hann regression|330 checks per side;57344000 bytes identical|

The90641615 I2 arithmetic/kernel/latch assertions and the new62025 UI checks use
ASan+UBSan with no category exclusions and no recoveries. The native web renderer
retains existing simulator exclusions for signed overflow, shift, bounds,
object-size and pointer-overflow; explicit pixel/text bounds checks remain.
The native editor/audio gateway excludes only inherited common-DSP signed
arithmetic/shift diagnostics. No DSP, window table or I1 storage source changed.

The browser integration uses Node with a minimal element test double and the
**actual** `paramRow`, `userSet`, `sendSet`, selection and descriptor-refresh
functions extracted from the editor. Its wire adapter runs real firmware editor
framing, canonical GET/SET/DESC/TRACK_PARAM and the final C mixer; it does not mock
parameter values or synthesize windows. Diagnostic firmware is used only to test
real GRAIN↔non-GRAIN metadata transitions. It also simulates old reserved metadata,
rejects stale control events and tests equal values across tracks and after reset.
This is not a claim of a real browser/WASM session.

Fresh firmware processes render each audio case, avoiding residual DSP state.
All four tracks are set through browser control handlers before rendering:

|WIN|Peak|RMS|Nonzero samples|Captured active grains|
|---:|---:|---:|---:|---:|
|0|15110|2876.226418227|124314|16|
|64|17280|3727.117971477|124518|16|
|127|17871|4798.705889722|124594|16|

All active grains carry the requested128+WIN latch. PCM-word hashes differ
between anchors and repeat identically for a second fresh Hann run; see
[browser-audio.json](data/grain-win-i3a/browser-audio.json). These are objective
nonzero/difference measurements, not subjective listening claims. The separate
existing default-Hann regression performs a complete byte comparison.

Two fixture issues were corrected without production workarounds: audio notes
must start after the fixture services pending initialization panic, and repeated
short EDIT tests must be spaced outside the existing double-tap interval. The
factory-reset last-sent suppression was separately reproduced against the real
firmware and fixed in the WIN frontend bookkeeping.

Broad web tests report exactly the same five source-map/name failures on exact
I2 and I3A. No new failures were observed. I2's historical MIDI/motion and broad
UI/editor fixture failures are documented in its report; those full suites were
not claimed fixed or rerun here. Focused UI/editor/provenance and persistence
checks above were rerun.

## Resources and toolchain limits

Same `grain_only_navigation_test.c` host harness and exact source-tree override,
GCC14 `-Os -ffp-contract=off -fstack-usage`:

|Native allocation|I2|I3A|Delta|
|---|---:|---:|---:|
|Read-only text total|844422|844534|+112 bytes|
|`.text`|280478|280574|+96|
|`.rodata`|475736|475752|+16|
|Data|31800|31800|0|
|BSS|506376|506376|0|

Host grain64, private part7296 and track1840 bytes remain asserted unchanged.
No firmware framebuffer, audio buffer, heap allocation, animation state or image
asset is added. Existing12-cell signature storage is reused; a value change
redraws the existing57×50 cell. PNGs are documentation assets, not firmware assets.
Browser HTML source grows1567 uncompressed bytes. It adds small descriptor/queue
helpers and track bookkeeping to its existing control cache; they do not add DSP state.

The existing JieLi executable is absent, so no target link was attempted and no
toolchain was installed/downloaded. The browser build attempt using
`sh web/emu/build.sh` stops with:

```
emu: no emcc (Emscripten)
```

Native firmware/browser-unity and transport tests passed, but actual WASM build,
AudioWorklet/browser execution and hardware flash fit remain unverified in this
workspace. Build the same source with the existing Emscripten toolchain in the
preview environment before browser deployment. No merge, deployment or flashing
was performed.

## Reproduction, changed files and I3B

With existing generated headers/pinned engine checkout:

```sh
WIN_I2_TREE=/absolute/clean/838540d-worktree \
WIN_B1_TREE=/absolute/clean/6055566-worktree \
WIN_I3A_OUT=/tmp/grain-win-i3a tests/run_grain_win_i3a.sh
```

`grain_win_i3a_captures.py` checks the exact clean base and reproduces120 raw
frames, cell-only pixel comparisons and PPM screenshots. The existing
`grain_only_navigation_test.c` can additionally be compiled against each source
using `GRID_UI_SOURCE`; its225 records compare state exactly and pixels except
for the intentional WIN rectangle on GRAIN page1 (page index15).

Changed production files: `firmware/src/params.c` (one grid entry),
`firmware/src/grain_edit_grid.c` (anchor unit line), `web/editor.html` (gated
scalar control/readout, per-track queue/cache handling and stronger library guard).
Updated tests: `tests/grain_grid_test.c`, `web/emu/grain_grid_visuals_test.c`,
`web/test_win_persistence.mjs`. Added tests: `tests/grain_win_i3a_ui_test.c`,
`tests/grain_win_i3a_gateway.c`, `tests/grain_win_i3a_captures.py`,
`tests/run_grain_win_i3a.sh`, `web/emu/win_controls_test.c`,
`web/test_win_controls.mjs`. This report, seven native PNGs and the audio evidence
JSON complete the checkpoint.

I3B still needs separately approved versioned library/bulk-transfer provenance
and any capability extension. Non-Hann unversioned transfers must stay blocked.
Persistent WIN learn needs explicit settings provenance; motion/locks and matrix
eligibility need separate approval. None is enabled here. An I1 firmware can
advertise the same valid storage descriptor while its DSP remains headless;
audible morphing requires I2 or newer. No version-string guess is used to expose
a reserved slot.
