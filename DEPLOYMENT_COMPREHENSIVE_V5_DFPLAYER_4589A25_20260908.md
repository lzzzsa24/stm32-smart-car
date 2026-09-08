# Comprehensive v5 deterministic MP3 selection deployment

Date: 2026-09-08

## Source and change

- Branch: `test/comprehensive-v5-dfplayer-20260908`
- Functional source commit: `4589a254258f6825af207a6a7ccd976175623ba2`
- Base audio integration: `c31df93`
- The unsuccessful `83e6bb5` compatibility sequence was removed. The right
  direction key now increments the tracked number and explicitly plays
  `/mp3/0002.mp3`, `/mp3/0003.mp3`, and so on, rather than relying on the
  module's physical-order `NEXT` command. Current-track looping remains enabled.

## Computer verification

- DFPlayer protocol/key-map host test: passed.
- Nonblocking queue test verifies explicit track 2 selection, looping, volume
  bounds and STOP: passed.
- ARM build/link: passed with text/data/bss `86568/64/11632`.
- BIN: `manual-build-candidate-comprehensive-v5-4589a25/exp7_unified_motion.bin`
  - size: `86636` bytes
  - SHA-256: `54EDEDE5D61F644EE0F025293F5556BD22E6EA78054DC99287CBDC517B3FE150`
- HEX: `manual-build-candidate-comprehensive-v5-4589a25/exp7_unified_motion.hex`
  - SHA-256: `2DB3801A43AEA64686A3492BA4903CFF28F3D08D181E6BFBCDAF8637B28229C2`

## STM32 deployment evidence

- Before the successful attempt, Windows reported a CH340 RTS-control failure;
  no erase acknowledgement was emitted and the attempt changed no Flash.
- After the user reinserted USB, `pnputil` identified USB-SERIAL CH340K as
  `COM11`. The reinsert also rebooted the previous image into its default STOP,
  so the application serial port was not reopened before bootloader entry.
- ROM bootloader acknowledged and selectively erased 43 application pages while
  preserving the final calibration page.
- All `86636` bytes were written and fully read back with
  `VERIFY OK: 86636 bytes`.
- Execution completed with `GO OK: 0x08000000`.
- No routine post-GO STOP/four-wheel telemetry check was performed.

## Physical verification boundary

- The user has not yet reported the result of the centre/right/up/down listening
  test on this exact image.
- No lifted-wheel or ground-driving test was performed.
- K210 was not accessed or changed.
