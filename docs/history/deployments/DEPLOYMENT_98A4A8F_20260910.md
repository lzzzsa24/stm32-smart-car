# Comprehensive98a4a8f deployment

Source98a4a8fecd0153977ff768bab3d3cf65958ecfdc, clean comprehensive-v15
checkout. Prior tested build reused after exact BIN/HEX hash check.
BIN115920 bytes:1E58F28F729DD3484D48701C2609D09DFFE2706B29316D967101D2475EC3E574.
HEX:789E14B8E7029473DCC0393C03CBA137FB7E8838EA31300A617E838E29365F46.
Artifacts: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Fresh registry/SerialPort enumeration:COM11. PowerShell7 stm32_uart_flash.ps1,
57600 baud,PreserveLastPage, exit0:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 57 firmware pages; calibration page preserved
WRITE OK: 115920 bytes
VERIFY OK: 115920 bytes
GO OK: 0x08000000
```

Full readback comparison passed. Application pages0..56 only; audio0x0807F000
and calibration0x0807F800 excluded. K210 unchanged. No post-GO query, motion,
ground test or push. Previous boardd01ab4c; main firmware unchanged.
