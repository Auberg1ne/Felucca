# Frozen B1 compatibility inputs

Generated **before changing converters** by `tests/grain_win_fixture_gen.c`,
compiled against the clean B1 `tests/ui_test.c` at
`6055566f6d6f3a9735d822cf4d1f6283c1d23e57` using `WIN_BASE_UI_SOURCE`.
`SHA256SUMS` freezes the original bytes. Never regenerate these with the new writer.

The synthetic FUN1–FUN11 objects exercise every historical reader. GRAIN engine
8 in older versions is deliberate synthetic input, not a claim that those
releases shipped GRAIN. FUN7/8/9 use their independent `old_pack.h` layout.
FUN10 uses 114 parameters: the largest legal old geometry; an overlapping
118-parameter FUN10 cannot validly contain reserved common ID109. FUN11 and
old FUNA runtime/noinit objects cover raw reserved109 values
0, 1, 63, 64, 65, 126, 127 on all four tracks.

User records cover versions 1–5, including note/grid encodings and the same
historical values where ID109 exists (versions 4/5). Versions 1–3 use np53.
All project fixtures were accepted by the exact B1 reader while freezing.

The earlier `../grain-win-i1` files and historical failing downgrade gate are
unchanged. Actual FUNC / UP6 / UP7 oracle inputs are emitted to a temporary
output directory by `grain_win_persistence_test`, never substituted here.
