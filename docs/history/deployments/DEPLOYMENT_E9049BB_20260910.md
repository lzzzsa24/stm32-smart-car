# Comprehensive e9049bb deployment

Source e9049bbe5b1d7b51990584dd6e1743c4858587b8; clean comprehensive worktree.
BIN120184 bytes; SHA256 B9CB6328CB60F73C7BF8844382B959589AD5CA47D00EF34EE694171F831C04B4.
HEX SHA256 75B0F7C32957EF07DFB9EAA5217F510D6521977B5FB1F897C5BDB7AB6438B783.
Fresh CH340K COM11 enumeration; PowerShell7 programmer,57600 baud,
PreserveLastPage. Tool session52073 exited0 before user interrupted the turn.

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 59 firmware pages; calibration page preserved
WRITE OK: 120184 bytes
VERIFY OK: 120184 bytes
GO OK: 0x08000000
```

Audio and calibration pages excluded. No repeated flash needed after the
interruption. K210 unchanged; no post-GO query, motion test or GitHub push.
Prior successful board source5940727 is superseded by this verified image.
