# Comprehensive STM32 test deployment: `2c2ed97`

Date: 2026-09-08 10:54 +08:00

## Purpose and source composition

This is an intentionally unmerged comprehensive test image built from current
canonical `main` (`f17bffa`) in the isolated branch
`test/comprehensive-v2-20260908`. Its exact source commit is
`2c2ed973ef5e34705f301c16871aed043e9ad461`.

The composite contains these three candidate changes, applied once and in this
order:

- `64f489eb9976961b4c7b8ccb0d4a6ba99c5995e4`, replayed as `c82c8b3`:
  slower outer-edge line pivots, silent automatic loss recovery, and DriveBase
  turn assist that permits the commanded stopped side to remain at zero.
- `4c345b8`, replayed as `0a6a0f3`: keep the selected sign-probe direction and
  outer-sensor priority for ten seconds.
- `68c6444`, replayed as `2c2ed97`: replace obstacle-bypass endpoint pulses
  with slow continuous travel.

Commit `64f489e` already includes the relevant `c5a4c76`/`074ef682` line
prerequisites, so those commits were not applied again. Starting from current
`main` also retains the published mode-1 power configuration from `3170221`.
The continuous-travel bypass change deliberately limits its short forward and
reverse segments to 1800 CPS; its turn command remains 2500 CPS.

## Build and host verification

- Formal ARM build: passed.
- ELF text/data/bss: `84148 / 64 / 11592` bytes.
- BIN size: `84216` bytes.
- BIN SHA-256:
  `D53897200AD5BD1C0913028446D2C4C2A2B667ABE32FBEAA9AC3B6849DF4815E`.
- HEX SHA-256:
  `DDADBDBDAD506AF344834504035875B213F2B70CDDB1597E1189533135BD2F74`.
- Line-recovery suite: passed at both configured speed variants, including
  120 visible cases, silent recovery, 0/2200 CPS outer pivots, zero-target-side
  turn assist, strong exits, overlaps, continuous bypass travel, four-wheel
  load checks, fault ownership and STOP ownership.
- Sign-line suite: passed, including the real ten-second probe hold.
- Vision-line-v4 suite: passed.

The exact cached artifacts are:

- `manual-build-candidate-comprehensive-v2-2c2ed97/exp7_unified_motion.bin`
- `manual-build-candidate-comprehensive-v2-2c2ed97/exp7_unified_motion.hex`

## STM32 deployment evidence

- Live programmer interface: USB-SERIAL CH340K `COM11`.
- Before bootloader entry, an application STOP command showed mode 0 and zero
  output on all four wheels. Battery telemetry was approximately 8.04 V.
- STM32 ROM bootloader acknowledged.
- Selective erase covered 42 application pages and preserved the final
  calibration page at `0x0807F800`.
- All `84216` bytes were written.
- Full readback completed with `VERIFY OK: 84216 bytes`.
- Execution completed with `GO OK: 0x08000000`.

Per the user's standing request, the routine post-GO serial STOP/four-wheel
zero-output check was intentionally not performed. This omission is not a
runtime-state claim; it only avoids the extra post-programming interaction.

## Boundaries

- K210 was not opened, reset, inspected or rewritten.
- No lifted-wheel test was performed.
- No ground-driving test was performed.
- Build/tests/readback establish source and Flash integrity, not physical line
  tracking, obstacle bypass, sign behavior or wheel motion.
- The composite is local test-branch source. It is not merged into `main`, not
  part of `v1.2.0-rc.2`, and was not pushed to GitHub in this operation.

Rollback source tag:
`rollback/2026-09-08-before-comprehensive-v2-test` -> `c5a4c76`

Deployed source tag:
`deployed/2026-09-08-comprehensive-v2-2c2ed97` -> `2c2ed97`
