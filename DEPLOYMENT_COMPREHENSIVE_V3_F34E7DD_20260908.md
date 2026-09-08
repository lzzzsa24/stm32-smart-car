# Comprehensive STM32 test deployment: `f34e7dd`

Date: 2026-09-08 11:24 +08:00

## Source and scope

- Exact flashed source:
  `f34e7dd3fa1f16f7d8abd0255c94a25d199cbb63`.
- Test branch: `test/comprehensive-v3-20260908`.
- Previous comprehensive source: `2c2ed97`.
- Before the final increment, `Core`, `tests` and `K210` at parent `8c7070b`
  were byte-for-byte equivalent to the corresponding tree at `2c2ed97`.

The new line-tracking increment changes persistent sole-outer-sensor behavior:

- a brief sole X2/X4 contact retains the existing 0/2200-CPS forward pivot;
- if the same sole outer remains continuously valid for 120 ms, control
  escalates to mirrored -2200/+2200-CPS powered counter-rotation;
- any different raw pattern, a sample gap above 30 ms, reset or queue overwrite
  cancels the escalation and requires fresh confirmation;
- an explicit zero forward cap/STOP remains authoritative;
- line-loss recovery remains silent and the independent manual phrase is
  unchanged.

The prior comprehensive layers remain present: silent/slow line recovery,
ten-second sign-probe hold and continuous short-distance obstacle-bypass
travel. This test source was not merged into `main`.

## Build and host verification

- Complete line-recovery/load/bypass suite: passed at both configured search
  speeds.
- Persistent-edge coverage includes mirrored outer sensors, 119/120-ms
  boundary, 60 interruption cases, timer wrap, ISR continuity and interruption,
  queue overwrite, frozen snapshots, zero cap and STOP.
- Real DriveBase tests confirmed brief 0/2200-CPS pivots escalate to mirrored
  -2200/+2200-CPS four-wheel counter-rotation and withdraw immediately on new
  middle evidence.
- Sign-line suite, including the real ten-second probe hold: passed.
- Vision-line-v4 suite: passed.
- Formal ARM build: passed.
- ELF text/data/bss: `84420 / 64 / 11600` bytes.
- BIN size: `84488` bytes.
- BIN SHA-256:
  `D44DF291F6A49176E67D76BD16D6B1356B07D919603580005318D74BA47ED54A`.
- HEX SHA-256:
  `0D4F79D56ACB603B9D705F44A4C445C2D519244686135A683E81962ED4138414`.

Cached artifacts:

- `manual-build-candidate-comprehensive-v3-f34e7dd/exp7_unified_motion.bin`
- `manual-build-candidate-comprehensive-v3-f34e7dd/exp7_unified_motion.hex`

## STM32 deployment evidence

- `pnputil` enumerated USB-SERIAL CH340K as `COM11` immediately before the
  deployment attempt.
- An application serial `0` was sent before bootloader entry. The board banner
  reported `DEFAULT STOP`.
- The first attempt entered the ROM bootloader and completed selective erase of
  42 application pages while preserving the final calibration page. Windows
  then returned access denied on COM11 before a complete firmware write. No
  successful write or verification was claimed for that attempt.
- COM11 remained enumerated as the same CH340K device. The retry entered the
  bootloader again, selectively erased the same 42 application pages, wrote all
  `84488` bytes, and completed full readback with
  `VERIFY OK: 84488 bytes`.
- Execution completed with `GO OK: 0x08000000`.
- Per the user's standing request, no routine post-GO serial STOP/four-wheel
  zero-output check was performed.

## Boundaries

- K210 was not accessed, reset or rewritten.
- No lifted-wheel test was performed.
- No ground-driving test was performed.
- Programmer readback proves the Flash contents, not physical steering or
  tracking behavior.
- Local `main` remains on accepted bypass source `453cad2`; this experimental
  line/sign composite remains separate and was not pushed to GitHub.

Rollback source tag:
`rollback/2026-09-08-before-comprehensive-v3-f34e7dd` -> `2c2ed97`

Deployed source tag:
`deployed/2026-09-08-comprehensive-v3-f34e7dd` -> `f34e7dd`
