# HOME sequencer bean indicators

Base: df690aca45a5b5d9612eaec2abd17ee55c5901fb.
Only the scientist HOME footer's existing step row changes. Other pages and
legacy HOME rendering retain the original bars. The upper HOME header, four
knob indicators, scientist sprites/meters and selected-channel frame are untouched.

The same bar_x layout fits up to 16 slots into x=12..227, in the current
16-step bank. Empty steps use a 7x2 underscore. Stored events use a procedural
9x11 integer oval with a curved diagonal cleft made of four short lines. Beans
use the existing selected-channel accent; accented notes retain a small glint.
Light themes get a contrasting outline/cleft. No image assets or sprite sheets
are added. All glyph pixels stay within the footer's first 16 rows (y=202..217).
The remaining footer text, hints, MIDI-learn row and messages remain unchanged.

Interpretation uses the existing step_on predicate: ST_NOTE with n>0 or a nonzero
drum hit mask. This includes valid MIDI note zero and drum-grid events without
pitched note[] entries. There is no recorded-origin tag in step_t: recorded,
manually edited and preset notes are all note-containing events. Ties sustain
previous notes but do not contain a new note, so tie-only/rest steps display
underscores. Stale note bytes in a rest, flags alone, muting, probability and
playback activity do not create/remove a stored-event bean. This follows the
existing sequencer/UI interpretation; no unresolved format interpretation remains.

The source is seq_steps(TSEL), including the active SONG lane's pattern, not
live voices, MIDI state or the mixed output. Existing footer hashing observes
that same data and selected channel, so edits and channel switches refresh
immediately. Pattern length/banks retain their existing visible-cell behavior;
the new renderer checks both the active length and NSTEP before indexing.

The playhead remains an independent one-pixel underline. It does not change the
step body: an empty playing step remains an underscore. The existing editing
cursor keeps its separate two-pixel underline and priority over the playhead.
No step data is changed, and no new sequencer/MIDI/input/audio code or callbacks,
allocations, buffers, persistent state or timers are introduced.

Validation: focused tests cover empty/full/mixed patterns, MIDI note 0, drum hits,
ties/rests with stale payloads, probability-silenced/muted events, playhead on
empty/populated/out-of-bank steps, editing cursor, actual channel encoder, SONG
sources, last bank, short patterns, invalid banks, dark/light themes and unchanged
surrounding pixels/data. UI/navigation and scientist HOME regressions are rerun
under the existing scoped ASan/UBSan configuration (the repository's DSP exclusions
remain in effect). No golden files are changed to obtain passing results.

Results: 235 focused checks, 850 full UI assertions and 89 scientist HOME checks
pass under ASan/UBSan. The 208-snapshot control/navigation and non-HOME pixel
trace matches df690ac exactly. The native browser-backend heavy song remains
bit-identical (705600 float32 bytes, SHA-256
66cf06868a1ef248a04257b3a71c31d60bb33e7cff80e89b709ed024161b6ce7).
Chromium frontend 20 and editor-order 48 checks pass. A native 240x240 mixed
pattern render was visually inspected; no preview image is added to the runtime
assets or repository. Actual WASM compilation/execution is not claimed here
(Emscripten is unavailable). No unresolved sequencer-data interpretation issue
remains beyond the explicit stored-event/tie distinction above.
