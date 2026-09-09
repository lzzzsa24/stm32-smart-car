# Comprehensive V11 deployment and IMU observation

Source: `fda63ce4597340df0f531b9f6940804f2795b00d`, functional source `a8b7e8b`.
Clean candidate rebuilt using formal build_unified_motion.ps1.
ARM text/data/bss: 99420/64/11976. BIN: 99488 bytes.
BIN SHA-256: `F18BC8A7C516CCAC9B99CD2F1705B29E727CB2327197699094DADECCF213A84B`.
HEX SHA-256: `C877421C2C097BE9EDED5F11EECECDBB0C8E0F6EF6EBB77BAC696345A5FA50AE`.

2026-09-09: enumerated COM11, programmer at 57600 baud with PreserveLastPage:

```text
BOOTLOADER ACK: boot=DTR value=True reset-active=False
ERASE OK: 49 firmware pages; calibration page preserved
WRITE OK: 99488 bytes
VERIFY OK: 99488 bytes
GO OK: 0x08000000
```

Selective erase/write did not include audio page 0x0807F000 or motor calibration
page 0x0807F800. K210 unchanged. No driving command or physical motion test.

User requested IMU confirmation. Two STOP-only observation rounds at 115200,
without DTR/RTS toggling, queried `g` five times each. No calibration override.
IMU communicated and delivered fresh FIFO data but NEVER reached READY:
`S=1 CAL=0`, `AGE=8..13`, `PENDING=0`, `BACKLOG=0`, `LAST_F=5`.
Restart count rose from 1 to 4. State 1 is calibrating; fault 5 is calibration
timeout. Zero yaw and rate are not valid yaw results before calibration.
Observed drive mode was STOP. This establishes an unresolved calibration issue,
not a missing-device diagnosis. The telemetry does not expose which raw-axis
acceptance condition failed; mounting tilt/sign, movement, offset/noise are
possibilities, not confirmed causes. Code rejects tilt, wrong Z sign, gyro
absolute raw magnitude above 196 and calibration raw span above 65.
Final raw log: `F:/myproject/jidian/validation/imu-v11-20260909/query_imu.log`.
V11 preserves mode1 encoder fallback; gyro-dependent sign routing is unavailable
until valid calibration. Main firmware not changed; this is a test deployment.
