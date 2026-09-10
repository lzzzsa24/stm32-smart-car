# Deployment ed7eb3b (2026-09-10)

Source: ed7eb3b8dbe9301b61374ad3a81fa32f18050203.
Checkout: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn (clean).
Artifact: manual-build-unified-motion/exp7_unified_motion.bin,111280 bytes.
Previously built and host-tested candidate; exact BIN/HEX hashes rechecked
before flashing, no source changes or old main rc.5 artifact substitution.

BIN SHA256: 6F168BB950249E82D809E9CC07A60C408A66615135C3632CC42B67F03AC43E06
HEX SHA256: 5868BA95057AEA96A19B36D91AF0B48B3A56717790FD7D97020D679026C4BD76

Fresh SerialPort/registry enumeration identified COM11 as the non-Bluetooth
serial device. PowerShell7 stm32_uart_flash.ps1 at57600, PreserveLastPage:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 55 firmware pages; calibration page preserved
WRITE OK: 111280 bytes
VERIFY OK: 111280 bytes
GO OK: 0x08000000
```

Exit0. Full111280-byte comparison passed. Erase range is application pages0..54,
excluding audio0x0807F000 and calibration0x0807F800. No post-GO query, mode
start, wheel/ground test, K210 access or GitHub push. Prior board58c3744.
Mode3 exit release, mode4 drawn route and mode5 fixed bypass/fast four-line
follow are included. Mode5 no longer consumes visual line commands.
