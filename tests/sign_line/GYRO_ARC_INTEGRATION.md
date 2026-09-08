# MPU-assisted sign semicircle candidate

> 综合整合说明：下文的“未导入模式1”只描述原始 `ac10ba5` 独立候选。
> `test/comprehensive-v6-gyro-20260908` 已有意合入 `842fc1/74526f4`，由
> 模式1和模式3/4共用唯一 MpuYaw 服务；组合结果见仓库根目录
> `COMPREHENSIVE_V6_GYRO_CANDIDATE.md`。

Base main: 49288f1. Branch: fix/sign-gyro-arc. Entry/arc handoff corrections
4c345b8 and 43262c7 are ported as 63f7dbc and 68db032. The requested 74526f4
supplies byte-identical mpu6050_yaw.h/.c and mpu6050_bus.c. GyroTurn is an
in-place motor owner and is deliberately not used to drive a painted semicircle.
No mode-1 bypass code from its donor branch is imported.

One MpuYaw service initializes after DriveBase and runs each main-loop iteration.
Stationary calibration requires app STOP, DriveBase STOPPED, zero requested/PWM
outputs and each wheel speed within +/-30 CPS. Keep the chassis stationary for
about 3 seconds after reset before starting. Calibration collects 200 FIFO
samples at 100 Hz. A fault remains latched until reset/reinitialization; no
automatic moving recalibration. Physical bus/mounting is PB10/PB11, AD0 PE0,
component side up, positive yaw left as documented in 74526f4.

Mode 3/4 stage conditions (initial ground-tuning values):
- PROBE: reserve arrow and sample approach yaw. Selected outside line followed
  by stable middle-only contact requires 60..120 degrees of measured turn in
  the selected direction to enter ARC. Wrong turn beyond 30 degrees cancels.
  Ten seconds remains a maximum, not required turn time.
- ARC: forward differential tracking follows real sensors. Relative measured
  yaw toward the circle must reach 150 degrees, wheel travel must exceed 150 mm,
  and the exact outward adjacent pair must persist 30 ms before EXIT_SELECT.
  225 degrees maximum and 20 s bound withdraw failed navigation. Full black
  never identifies an exit. Wheel counts supply distance only, never angle.
- EXIT_SELECT: stable centre, at least 45 degrees of exit rotation and heading
  within +/-15 degrees of the original approach are required. This track has
  opposing entry/exit lines and the same travel heading before/after the ring.
  Different track geometry requires changing that final-heading assumption.
- No fresh ready yaw: NAVF=6 cancels route ownership; wrong entry yaw: NAVF=7.
  Ordinary line/search continues and STOP remains authoritative. Continuous
  sign sightings cannot immediately restart a cancelled attempt.

SIGN3/SIGN4 telemetry: IMU=1 means ready/fresh, YAW is phase-relative measured
millidegrees (positive left), MM is wheel distance, R is route state, NAVF is
withdrawal reason. A READY sensor is not proof of correct physical yaw sign.
MpuYaw freshness is checked each sign-control cycle; no encoder-yaw fallback.
The K210 program/model are unchanged.

Validation: production-config gyro-route tests cover left/right complete
entry/arc/exit, early exit contact, all-black, missing measured rotation,
final-heading capture, invalid IMU, wrong arc direction and tick wrap. Real
MpuYaw mocked-bus tests cover stationary calibration, signed FIFO integration
and stale fault. Donor bus tests cover pin scope, ACK/NACK, repeated starts,
FIFO read and timeout. Existing legacy route tests disable the IMU requirement
only to isolate their original cases; production requires it.
ARM build: text/data/bss 88788/64/11728. Mode-5 regression passed.

No main merge, shared-state update, serial access, flash or physical test.
Ground acceptance must verify yaw sign, calibration, entry capture, forward
arc behavior and both exits. An integrated gyro drifts; these angle windows
and sensor placement still require real-track calibration. This candidate
does not establish that the car has successfully passed a physical circle.
