# V13 flash and stationary IMU test

Source b326a6ad4fa975982111ec82c21508c1e46074a1, clean worktree rebuilt.
ARM text/data/bss 102568/64/12080; BIN 102636 bytes.
BIN SHA256 B53205952EE5709F8D0E78C55E751AB19477F6351ED278B36515E5BFD1D3EE48.
HEX SHA256 3044B999BBA711881AFCB4C6499DB39D731ED1680CC7164BA58DC2D2E40AE3D5.
Enumerated COM11, 57600 baud, PreserveLastPage:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 51 firmware pages; calibration page preserved
WRITE OK: 102636 bytes
VERIFY OK: 102636 bytes
GO OK: 0x08000000
```

Only pages 0..50 were erased; audio memory 0x0807F000 and motor calibration
0x0807F800 were outside erase/write bounds. K210 untouched.

STOP-only serial test returned 20 IMU status samples across 19.328 seconds:
- All S=2 READY, CAL=200, F=0, REJECT=0, RESTARTS=0, BACKLOG=0, PENDING=0.
- AGE=18..23 ms. BIAS_MRAW=-13245 (Z bias -13.245 raw).
- YAW_MDEG=21 to 150: stationary displayed change +0.129 degree in 19.328 s,
  approximately +0.0067 degree/s over this short observation only.
- ACC_REF=1529,4,21874; ACC_WARN=1 remains for biased gravity magnitude.
- LAST_REJECT=32, REJECTS=1 records one earlier unstable acceleration window;
  it was rejected, then a complete accepted calibration succeeded.

Evidence: F:/myproject/jidian/validation/imu-v13-20260909/static-20260909-101757.json
and matching .log. No driving mode, physical rotation or ground test was run.
This proves static calibration and current yaw data availability, not physical
turn-angle accuracy or long-term drift. Non-nominal accelerometer is not repaired.
Board now V13; main firmware remains unchanged, deployment records local only.
