# V13: bounded stable-gravity tolerance for yaw calibration

Branch test/comprehensive-v13-imu-gravity-20260909, created from main c740a16,
merged V12 c177c3d. Prior candidates preserved. No deployment in this task.

Measured V12 rejection was REJECT=4: AZ=21806..21940 instead of the accepted
14000..18500. Gyro Y=-222..-217 also exceeded the pre-V12 absolute bias limit.
V12 addressed the latter. This change permits yaw calibration with a bounded,
stable but non-nominal acceleration reference; it does NOT repair/calibrate the
accelerometer or establish why its magnitude is about 1.33 nominal g.

## Acceptance and diagnostics

- Positive mounting-Z range 12288..24576 (~0.75..1.5 nominal g).
- Existing |AX|,|AY| <=4500, correct Z sign, STOP ownership, absolute gyro
  limits and <=65-count gyro span are retained.
- NEW: each acceleration axis must stay within an 800-count peak-to-peak
  span across the same 200 consecutive samples. Any failing sample resets
  both gyro and acceleration accumulation. No acceleration output rescaling.
- Accepted mean acceleration is published as ACC_REF=x,y,z. ACC_WARN=1 if
  the mean Z is outside the original 14000..18500 nominal interval.
- REJECT bit 32 means acceleration varied during calibration. Other rejection
  bits and raw GRAW/ACC remain available through STOP-only serial g.
- Yaw integration still uses sensor-timed gyro Z minus its measured bias;
  acceleration magnitude is not integrated or used to scale gyro output.
- Mean/reference warning survive automatic recovery with retained gyro bias;
  explicit c resets both and requires a new stationary calibration.

Hardware verification must still show S=2 CAL=200 with fresh AGE, no backlog,
acceptable stationary yaw drift, and subsequently measured physical yaw accuracy.
ACC_WARN=1 is allowed for yaw-only operation but remains an unresolved sensor
quality issue. Constant external rotation or acceleration cannot be distinguished
from bias/gravity with these gates alone: physically keep the car stationary.
If acceleration is unstable/out of the bounded range, calibration remains blocked.

## Verification

Full sign_line and gyro_turn suites passed, including real DriveBase/bypass,
45-degree inward return, sensor faults/recovery and STOP. Replay repeats the five
measured AX/AZ and gyro tuples as a stationary fixture (AY=0); it reaches 200
samples with bias=-15 raw and warns on the abnormal acceleration reference.
Additional tests cover ideal static yaw drift/integration, unstable acceleration,
range boundaries, inverted mounting, tilt, and explicit reinitialization.
This fixture is not a two-second raw hardware capture or physical validation.

Formal ARM build passed: text/data/bss 102568/64/12080; BIN 102636 bytes.
BIN SHA256 B53205952EE5709F8D0E78C55E751AB19477F6351ED278B36515E5BFD1D3EE48.
HEX SHA256 3044B999BBA711881AFCB4C6499DB39D731ED1680CC7164BA58DC2D2E40AE3D5.
Artifacts: manual-build-unified-motion/exp7_unified_motion.bin and .hex.
Modes, motor mapping and K210 assets are unchanged relative to V12.
Board remains c177c3d; no flash, GitHub push or main firmware merge performed.
