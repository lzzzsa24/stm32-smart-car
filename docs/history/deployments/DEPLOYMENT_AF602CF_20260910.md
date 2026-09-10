# Comprehensiveaf602cf deployment

3bd989a merged onto29d490b asaf602cf80b9f5b12405b693a57898e485702f051.
Mode3 observation centering added; mode4 visible-line priority retained.
Full sign and line (both speeds) suites, mode5/gyro integration and ARM build
passed. Clean comprehensive-v15 checkout. BIN116700 bytes.
BIN SHA256:281ED549028AB60A4EAF0589739B9CF581837215550819AEF9FDF2396DD08B42
HEX SHA256:6B2358EF7DAC107B7C9EA569B14F463EE34D8CBC95646B7A3AB99BD87F554D96
Fresh COM11 enumeration. PowerShell7 stm32_uart_flash.ps1,57600 baud,
PreserveLastPage,exit0:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 57 firmware pages; calibration page preserved
WRITE OK: 116700 bytes
VERIFY OK: 116700 bytes
GO OK: 0x08000000
```

Full readback comparison passed. Audio/calibration pages excluded. K210 and
main firmware unchanged; no post-GO query, OLED/physical test or push.
Previous board947794d. Rollback:rollback/2026-09-10-before-3bd989a.
