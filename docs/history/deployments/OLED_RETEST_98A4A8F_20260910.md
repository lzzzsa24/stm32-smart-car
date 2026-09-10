# OLED latest-version retest

After user-reported OLED recovery on d01ab4c, user requested latest again.
Clean comprehensive source98a4a8fecd0153977ff768bab3d3cf65958ecfdc and BIN
115920 bytes rechecked: SHA256
1E58F28F729DD3484D48701C2609D09DFFE2706B29316D967101D2475EC3E574.
Fresh COM11 enumeration, PowerShell7 stm32_uart_flash.ps1,57600 baud,
PreserveLastPage; exit0:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 57 firmware pages; calibration page preserved
WRITE OK: 115920 bytes
VERIFY OK: 115920 bytes
GO OK: 0x08000000
```

Audio/calibration pages excluded. No post-GO query, motion, K210 change or
push. OLED repeat outcome pending user observation; root cause not established.
