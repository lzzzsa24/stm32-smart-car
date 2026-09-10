# OLED comparison rollback

User reports external OLED suddenly dark, onset unknown; requested older
firmware comparison. Rebuilt exact d01ab4c621cfa16436ada7139805db8224db1123
in F:/myproject/jidian/validation/oled-rollback-d01ab4c-20260910, detached.
Current comprehensive branch98a4a8f remains unchanged.
BIN115456 bytes SHA256 CC55536F52C4C959653154545694D7E1548D130DC21C87EDF8AE417CC969C4D9.
HEX SHA256 962377807BAE1855F0C59E2EEC62D477B2D985630D4E1C58F9FC9A9083D6885F.
Both match the earlier d01ab4c deployment. Fresh COM11 enumeration,
PowerShell7 stm32_uart_flash.ps1 at57600,PreserveLastPage,exit0:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 57 firmware pages; calibration page preserved
WRITE OK: 115456 bytes
VERIFY OK: 115456 bytes
GO OK: 0x08000000
```

Audio/calibration pages excluded. No post-GO query, motion, K210 access or
push. OLED recovery has NOT been observed; user comparison pending.
