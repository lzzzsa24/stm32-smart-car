# Comprehensive4eda8fe deployment

Source:4eda8fe800e29cd347a8cc2730cccd4d1310873b, clean comprehensive-v15
checkout. Previously tested build reused after exact BIN/HEX hash check.
BIN:115184 bytes,7089519FF32B50F97EF7C847CB1242A685075C92B399A5C696E4AB229F5B0E7C.
HEX:C9331E411F54927CDB99E77F33458438D3E4BFCC7EA8262E78A76A88B09B1912.
Artifacts: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Fresh registry/SerialPort enumeration:COM11 (non-Bluetooth serial device).
PowerShell7 stm32_uart_flash.ps1,57600 baud,PreserveLastPage; exit0:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 57 firmware pages; calibration page preserved
WRITE OK: 115184 bytes
VERIFY OK: 115184 bytes
GO OK: 0x08000000
```

Application-only pages0..56 exclude audio0x0807F000/calibration0x0807F800.
Full BIN byte comparison passed. Mode3 natural exit, mode4 missed-arc recovery,
mode5 pulse steering included. K210 unchanged; no post-GO query, motion or
ground test, no push. Previous boarde047c13. Main firmware unchanged.
