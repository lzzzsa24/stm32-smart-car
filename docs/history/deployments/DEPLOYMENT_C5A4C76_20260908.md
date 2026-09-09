# Temporary STM32 deployment: `c5a4c76`

Date: 2026-09-08 10:27 +08:00

## Source and build

- Exact source commit: `c5a4c76ab84666f44d03510227bfbeac41083641`
- Source branch: `fix/line-visible-arc`
- Subject: `feat(line): steer forward on visible line and spin on confirmed loss`
- Formal ARM build: passed
- ELF text/data/bss: `84568 / 64 / 11536` bytes
- BIN size: `84636` bytes
- BIN SHA-256: `BE9820DCB9F9673652F18636CBECA4B9BAAC344577796327873BA86C9429B6E2`
- HEX SHA-256: `F89A8BDF6D6F31A08E60F16263497859BA30A3F4DBB47E814E0F33975018686F`

The complete line-recovery suite passed at both configured search speeds,
including 120 visible-mask/gain/state cases, forward visible-line steering,
confirmed-loss rotation, strong exits, ordered overlaps, four-wheel DriveBase,
bypass and STOP ownership.

This comparison source predates and does not contain main's mode-1 obstacle
power commit `3170221`.

## STM32 deployment evidence

- Live programmer port: USB-SERIAL CH340K `COM11`
- Before bootloader entry, an application STOP command produced
  `DRV M=0 P=0 F=0` with zero target/measured/PWM output on all four wheels.
- Battery telemetry before programming was approximately 8.13 V.
- STM32 ROM bootloader acknowledged.
- Selective erase covered 42 application pages and excluded the reserved final
  calibration page at `0x0807F800`.
- All 84636 bytes were written.
- Full firmware readback completed with `VERIFY OK: 84636 bytes`.
- Execution completed with `GO OK: 0x08000000`.
- After GO, another STOP was sent without changing DTR/RTS. The application
  again reported mode 0 and zero target/measured/PWM output on every wheel.

## Boundaries

- K210 was not opened, reset or rewritten.
- No driving mode was started by Codex.
- No lifted-wheel test was performed.
- No ground-driving test was performed.
- Programmer verification proves the Flash bytes, not physical line-tracking
  behavior.
- This commit remains an unmerged comparison branch and is not part of Release
  `v1.2.0-rc.2`.

Rollback source tag:
`rollback/2026-09-08-before-c5a4c76-test`

Deployed source tag:
`deployed/2026-09-08-line-visible-arc-c5a4c76`
