# V12 deployment and raw IMU findings

Source c177c3d4eaa89f5c15b87cb4fca761835000be02 rebuilt from clean worktree
F:/myproject/jidian/worktrees/comprehensive-v12-imu-cal.
ARM text/data/bss 102080/64/12048; BIN 102148 bytes.
BIN SHA256 FE4C8E49114DBA4A519FBE07CD6056DC539F40F1333F664AFD3F85CF56D12858.
HEX SHA256 F5BF543E55ABD86BC495A5CDD57A7EDCA49101FD626733B9BFFB0E27BFCF2F37.
COM11 enumerated; 57600 baud, PreserveLastPage, application pages 0..49 only.
Audio page 0x0807F000 and motor calibration page 0x0807F800 outside erase/write.

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 50 firmware pages; calibration page preserved
WRITE OK: 102148 bytes
VERIFY OK: 102148 bytes
GO OK: 0x08000000
```

Five subsequent STOP-only `g` queries (no reset, no driving mode) found:
- S=1 CAL=0 REJECT=4 LAST_REJECT=4; NOT READY.
- ACC x=1508..1552, y=-32..34, z=21806..21940.
- GRAW x=-197..-194, y=-222..-217, z=-16..-14.
- AGE=18 ms, PENDING=0, BACKLOG=0. One automatic calibration-timeout retry.

Confirmed immediate rejection: positive Z exceeds 18500 upper threshold.
At configured 16384 LSB/g this is about 1.33 g while user reports flat/still.
This does not establish whether acceleration offset/scale, device behavior or
data acquisition causes the unusual reading. Do not call it confirmed damage.
Y gyro also exceeds the old +/-196 raw threshold, so V12 fixed a real additional
barrier but did not complete physical calibration. Current yaw=0 is invalid.
No physical yaw-angle or driving test. K210 unchanged. Board now runs V12,
not V11; main firmware remains unchanged.
