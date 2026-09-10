# Comprehensive b32e03a deployment

Source b32e03aa5957dcafa2e24390a618d6c2b2224c09; clean comprehensive worktree.
BIN119440 bytes, SHA256401A6D6BAB855CAF504E709D743DE27039F9F040CEAE567CF89EDBFB4CA9049D.
HEX SHA256F1F51E076F2B213AD5FA9B7086E24FE94C6ED1E62E97306D3BA77DE5E1BA58A7.
Fresh CH340K COM11 enumeration; PowerShell7 programmer at57600 baud,
PreserveLastPage; session32004 exited0:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 59 firmware pages; calibration page preserved
WRITE OK: 119440 bytes
VERIFY OK: 119440 bytes
GO OK: 0x08000000
```

Full readback passed, audio/calibration pages excluded. Includes9eba339
direct2s observation without centering. K210 unchanged; no post-GO query,
motion test, ground test or push. Prior boarde9049bb superseded.
