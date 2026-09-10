# Comprehensive d01ab4c deployment

Source:d01ab4c621cfa16436ada7139805db8224db1123, clean comprehensive-v15
checkout. Prior tested build reused after BIN/HEX hash verification.
BIN115456 bytes:CC55536F52C4C959653154545694D7E1548D130DC21C87EDF8AE417CC969C4D9.
HEX:962377807BAE1855F0C59E2EEC62D477B2D985630D4E1C58F9FC9A9083D6885F.
Artifacts: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Fresh COM11 enumeration; PowerShell7 stm32_uart_flash.ps1,57600 baud,
PreserveLastPage. Exit0:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 57 firmware pages; calibration page preserved
WRITE OK: 115456 bytes
VERIFY OK: 115456 bytes
GO OK: 0x08000000
```

Full readback comparison passed. Pages0..56 only; audio0x0807F000 and
calibration0x0807F800 excluded. No post-GO query, motion/ground test, K210
access or push. Previous board4eda8fe; main firmware unchanged.
