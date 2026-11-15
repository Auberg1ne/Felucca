# Frozen pre-I1 downgrade fixtures

Created before any conversion/production change, using exact revision
`ca62bb15047ffc42d586b1be0edebecced36d49c` and the existing physical NOR
`tests/persistence_test.c` fixture, `reset()`, four GRAIN tracks,
`project_capture()` and `proj_pack()`.

- `fun11-base.bin`: actual FUNB writer output, 3840 bytes.
- `proposed-func.bin`: same bytes with only magic changed to proposed FUNC
  (`0x46554e43`) and its existing FNV-1a checksum recalculated. This is a
  prospective-format probe, **not an implemented WIN-aware format**.

Both retain all existing offsets. The new-format proposal specifically requires
unknown magic to be rejected by the old reader, so no new reader is necessary
to test the old firmware's behavior after that rejection. Do not regenerate
these fixtures with a later writer. This is one frozen legacy fixture; the
all-version migration matrix was not generated because the downgrade gate
failed before conversion work began.

SHA-256:

```
a97a685b1cede344e2938facfd22605edecd873b0919ca499afbee826dc292df  fun11-base.bin
f891c9dc0bbbe7f1a535e4ac0469e9589394bec7b1a7569b3d9daca8943ec0bb  proposed-func.bin
```
