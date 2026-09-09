# Deployment a6c61d3

Source: a6c61d30f4936aabf25ecc70ff9ae2bd304c088a.
Checkout: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn.
Candidate build/sign/line/gyro evidence: ../candidates/COMPREHENSIVE_A6C61D3_20260909.md.
Source remained clean and BIN size/hash matched before flashing.
Artifact: checkout/manual-build-unified-motion/exp7_unified_motion.bin.
Size:108800 bytes.
BIN SHA256:8D35B9376F602C68DD8B05BF62D7360DB7DD3973C89F9EB30CAF131EEBD500CB
HEX SHA256:FFD47049FE1210E9B0ED9521EBB25B0F08A530C33407A3ACB7FEAB2F1331E0E8

Fresh COM11 enumeration; PowerShell7 stm32_uart_flash.ps1 at57600 baud with
PreserveLastPage. Exit0:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 54 firmware pages; calibration page preserved
WRITE OK: 108800 bytes
VERIFY OK: 108800 bytes
GO OK: 0x08000000
```

Application-only erase/write excludes audio0x0807F000 and calibration0x0807F800.
No post-GO query or physical test. Mode3 consolidated sign control;4=STOP.
Side IR remains disabled. K210 unchanged at20e8c72; main code unchanged.
Previous board533e355. No GitHub push.
