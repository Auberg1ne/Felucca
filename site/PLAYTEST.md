# COFFEE_GRAINS first browser playtest

Source checkpoint:29bc277a4c726b5b20b4881c6fa744e2d5b4cb9f.
Compiler:Emscripten3.1.74; pinned official Docker image af45409f3199d88db4b1b03af0098532c8fb33a375ac257463eeb0a622870d06.
WASM:site/web/emu/felucca.wasm,765889 bytes.
SHA256:de3f9cc7b2080d8a1b1ae2f99b50715fed4d47cf9dd5df318e9ac41a0ce8be51.

Run `sh launch.sh`, then open http://127.0.0.1:8765/ in Chrome/Chromium.
Python3 required; no compiler, SDK or Docker required to play. Keep the server terminal open; Ctrl-C stops it. A local HTTP server is required (not file://).

Start on the landing page. Optionally load the demo, then Switch on. Demo starts on track4 GRAIN/USR8. Left/right selects tracks1..4; keyboard Z/X/C/V/B/N/M plays notes. Click/drag the virtual controls; up/down turns SELECT, left/right turns ALGORITHM (track), [] changes octave. The on-screen PRESETS knob changes presets. Start with low volume.

Demo:track1 factory SAMPLE/GUITAR;2 factory GRAIN/GUITAR;3 SAMPLE/USR8;4 GRAIN/USR8. All eight synthetic user samples use the existing validated v2 journal and header/audio CRC format. The demo project and autosave are emitted by actual checkpoint FUNB serialization and st_save, not fabricated layouts. The original sample-pool CRC remains e7d353fa. Demo import explicitly writes only this URL origin's browser storage; it is not a browser sample-upload feature. Existing saves require confirmation and can first be downloaded from demo.html.

Read-only editor opens in a separate window. Sample upload/erase and backup restore cannot reach the simulator. No writable bridge or S4A permission change was made. Factory IDs0..4 contain three distinct materials with aliases. Worker preparation is optional via ?worker_prepare=1 and requires two gestures; default startup is unchanged. Startup timing is experimental. Browser persistence is not a substitute for backups. Physical FM-1 compatibility, real-time safety and audible quality are unverified. No hardware flashing.

Uncommitted S9A/S9D and partial S9E work is preserved in the original repository and excluded from this build. This playtest uses its own source/archive/output folders and does not modify the development checkout.
