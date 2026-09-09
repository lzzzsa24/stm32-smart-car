# STM32 deployment: main mode-1 power candidate `3170221`

Date: 2026-09-08 10:08 +08:00

## Source and build

- Functional source commit: `317022123395016354249c3c7bc9e9b26c02ff7e`
- Main state commit before deployment: `3282ad61ee71ae59ee22b580b4380621882d8190`
- Change: raise mode-1 ultrasonic and obstacle-bypass motion power only
- Formal ARM build: passed
- ELF text/data/bss: `84384 / 64 / 11536` bytes
- BIN size: `84452` bytes
- BIN SHA-256: `14B484C5091A3549B24360236F941A61CC12688D34900427D73EFAC2D181E9C0`
- HEX SHA-256: `D30049A841C4F9C43F2925C118CDA70C675E45D0B2774D87C94D734549BF7AA8`

The complete line-recovery/bypass suite passed at both configured search
speeds, including four-wheel DriveBase, obstacle-bypass and STOP-ownership
checks.

## STM32 deployment evidence

- Live programmer port: USB-SERIAL CH340K `COM11`
- Before bootloader entry, an application STOP command produced
  `DRV M=0 P=0 F=0` and zero target/measured/PWM output on all four wheels.
- Battery telemetry immediately before programming was approximately 8.24 V.
- STM32 ROM bootloader acknowledged.
- Selective erase covered 42 application pages and excluded the reserved final
  calibration page at `0x0807F800`.
- All 84452 bytes were written.
- Full firmware readback completed with `VERIFY OK: 84452 bytes`.
- Execution completed with `GO OK: 0x08000000`.
- After GO, another STOP was sent without changing DTR/RTS. The application
  reported mode 0 and zero target/measured/PWM output on every wheel.

## Boundaries

- K210 was not opened, reset or rewritten.
- No driving mode was started.
- No lifted-wheel test was performed.
- No ground-driving test was performed.
- Build and programmer verification do not establish physical avoidance
  performance.

Rollback source tag:
`rollback/2026-09-08-before-main-mode1-power-flash`

Deployed source tag:
`deployed/2026-09-08-main-mode1-power-3170221`
