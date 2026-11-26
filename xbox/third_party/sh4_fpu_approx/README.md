# sh4_fpu_approx.h

Bit-exact models of the SH-4's (SH7091 / SH7750) FIPR, FTRV, FSCA and FSRRA,
verified against a real Dreamcast, by Stefanos Kornilios Mitsis Poiitidis
(skmp), MIT license (in the header).

- Source: https://gist.github.com/skmp/4bd7b38149fc455978b5104201573236
  (revision a61dd6de0304c631b77915352e9d4dba48f2262d), copied unchanged.
- Write-up: https://drk.emudev.org/blog/sh4-fpu/

The port calls it from `xbox/port/fpu.c` when a `HOST_FPU_*` setting asks for
the exact model (XBOX.md, Debug flags).
