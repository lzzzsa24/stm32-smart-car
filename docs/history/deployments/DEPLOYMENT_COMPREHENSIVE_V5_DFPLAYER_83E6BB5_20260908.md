# Comprehensive v5 DFPlayer next-track fix deployment

Date: 2026-09-08

## Source and change

- Branch: `test/comprehensive-v5-dfplayer-20260908`
- Functional source commit: `83e6bb55493b0623dc645e2ffacdecde9c61faa8`
- Parent deployment: `c31df93`
- Fix: modules that ignore `NEXT` while current-track repeat is active now
  receive a nonblocking `disable current loop -> NEXT -> enable current loop`
  sequence. Volume and all car-control behavior are otherwise unchanged.

## Computer verification

- DFPlayer protocol/key-map host test: passed.
- Nonblocking queue test now verifies the three-command next-track sequence,
  volume bounds, playback and STOP: passed.
- ARM build/link: passed with text/data/bss `86728/64/11640`.
- BIN: `manual-build-candidate-comprehensive-v5-83e6bb5/exp7_unified_motion.bin`
  - size: `86796` bytes
  - SHA-256: `1C29504BC1E6C5D66E5037D7420CEAD800538021F89FF9CAAC03E777D069CDEB`
- HEX: `manual-build-candidate-comprehensive-v5-83e6bb5/exp7_unified_motion.hex`
  - SHA-256: `0FCD0B0DA0DBB5231127E5BFC80CF2E6725FB2A8A3E1162FBECE4A7C266ECACF`

## STM32 deployment evidence

- After the user reinserted USB, `pnputil` identified USB-SERIAL CH340K as
  `COM11`.
- Application serial `0` was sent before bootloader entry; the running v5
  firmware returned its default-STOP and DFPlayer-control banners.
- The ROM bootloader acknowledged, selectively erased 43 application pages,
  and preserved the final calibration page.
- All `86796` bytes were written and fully read back with
  `VERIFY OK: 86796 bytes`.
- Execution completed with `GO OK: 0x08000000`.
- No routine post-GO STOP/four-wheel check was performed, as requested.

## Physical verification boundary

- The user has not yet reported the result of the new right-key listening test.
- No lifted-wheel or ground-driving test was performed.
- K210 was not accessed or changed.
