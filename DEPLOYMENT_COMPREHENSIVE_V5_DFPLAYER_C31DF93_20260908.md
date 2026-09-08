# Comprehensive v5 DFPlayer deployment record

Date: 2026-09-08

## Source identity

- Branch: `test/comprehensive-v5-dfplayer-20260908`
- Functional source commit: `c31df93`
- Base comprehensive test: `0a02d3d`
- Replayed DFPlayer integration: `db116eb` as `8fc917a`
- The previous v4 branch and commit were not modified.

## Added behavior

- DFPlayer Mini remains on J8 UART4 (`PC10=TX`, `PC11=RX`, 9600 8N1).
- Direction-pad centre selects `/mp3/0001.mp3`.
- Direction-pad right sends DFPlayer `NEXT`.
- Direction-pad up/down adjust the requested volume by `+2/-2`, clamped to
  `0..30`; default volume is `10`.
- Current-track looping is enabled by default. Power-on remains stopped and
  silent until a play/next request.
- Direction-pad audio actions do not select a vehicle mode. Numeric `0`, mode
  changes and existing safety-audio arbitration stop external playback.

## Computer verification

- DFPlayer protocol/key-map host test: passed.
- Nonblocking command queue, volume clamp, next, loop and stop host test:
  passed.
- Complete line-recovery/load/bypass suite at both configured search speeds:
  passed.
- Sign-line, ten-second probe, K210 runtime and mode 3/4 bindings: passed.
- Vision-line-v4 controller and mode 5 integration: passed.
- ARM build/link: passed with text/data/bss `86648/64/11640`.
- BIN: `manual-build-candidate-comprehensive-v5-c31df93/exp7_unified_motion.bin`
  - size: `86716` bytes
  - SHA-256: `F941E340F0456E3443AF29326A0A19E48555812B33359F7771E134A4488EBB0D`
- HEX: `manual-build-candidate-comprehensive-v5-c31df93/exp7_unified_motion.hex`
  - SHA-256: `19809201E42C4934044B9674CBD160085B11176C59A37316E92C3D915B87D96F`

## STM32 deployment evidence

- `pnputil` identified USB-SERIAL CH340K as `COM11` immediately before flash.
- Application serial `0` was sent before bootloader entry; the board returned
  its default-STOP startup banner.
- An initial invocation through legacy Windows PowerShell failed at parse time,
  before opening the port or altering Flash.
- The first PowerShell 7 attempt entered the ROM bootloader, selectively erased
  43 application pages while preserving the final calibration page, then
  received unexpected `0xF9` at write command address `0x08012700`. It did not
  complete verification or GO.
- The immediate full retry used the same BIN, again preserved the final page,
  wrote all `86716` bytes, completed full byte readback with
  `VERIFY OK: 86716 bytes`, and executed `GO OK: 0x08000000`.
- Per the user's standing request, no routine post-GO STOP/four-wheel telemetry
  check was performed.

## Not yet physically verified

- No lifted-wheel or ground-driving test was performed.
- DFPlayer/card/speaker sound, looping, volume change and next-track behavior
  still require the user's listening test. Build/readback evidence does not
  prove the external audio hardware works.
- K210 was not opened, reset or rewritten in this deployment.
