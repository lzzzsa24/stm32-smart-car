# Comprehensive c50d3c8 deployment

Source `c50d3c81a992fc3d5cd7e91d42ceeaa9c3385821`, comprehensive V15
with mode 3/4 RGB-off `6908938` and five-repeat horn `9cea6bc`.
Clean source and BIN SHA256 rechecked before deployment:
`2C980CA65E5C267D604AB70806C55B21F478E2998E3A1A1643A73497EBFD0DB8`.
HEX SHA256: `4B3C026C7E18D0670452438C8D4952B0A4E41867030D84F4D94000E3D9974263`.
Build and regression evidence: `PREPARED_C50D3C8_RGB_OFF.md`.

After user replug, initial WMI enumeration still listed only Bluetooth ports;
fresh SerialPort enumeration and SERIALCOMM registry then showed COM11.
ROM bootloader ACK confirmed successful connection. Programmer at 57600 baud
with PreserveLastPage reported:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 51 firmware pages; calibration page preserved
WRITE OK: 104232 bytes
VERIFY OK: 104232 bytes
GO OK: 0x08000000
```

Only application pages 0..50 erased; audio 0x0807F000 and calibration
0x0807F800 remain outside erase/write bounds. No post-GO serial query,
movement, listening or physical lamp test performed. K210 was not accessed;
new highest-score/horn K210 script still requires separate deployment.
No GitHub push. Main firmware source remains unchanged; this is test deployment.
