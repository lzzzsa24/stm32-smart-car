# Comprehensive V12: inward return and bias calibration

Branch: test/comprehensive-v12-imu-cal-20260909. Based on current main 5efa7df,
merged complete V11 fda63ce and requested af46a5fe4219328ff1e5885284b3f004e4f07730.
V11 steady mode3/4 tracking, V9 sonar continuity and V8 recovery are retained.
Old worktrees and main firmware are preserved. Not flashed or pushed.

The imported bypass uses fresh same-generation IMU yaw to switch to continuous
forward return after measured inward angle exceeds 45 degrees with obstacle
clearance; fresh outer-line contact hands off to line following. See the V10
return document for boundary details.

## Calibration correction

The old code rejected any raw gyro axis over 196 counts (~3 deg/s) BEFORE
estimating bias. A stationary sensor with a larger constant bias could therefore
never calibrate. New candidate accepts absolute raw values up to 1310 (~20 deg/s)
while retaining 200 consecutive samples, <=65-count span on each gyro axis,
stopped ownership and the existing upright acceleration checks. Only Z bias is
needed/removed for yaw integration. Pose and waveform stability checks were not
relaxed. Large outliers, instability, FIFO faults and data freshness still apply.

This fixes a reproduced software limitation, not a proven measurement of this
board's cause. Current board CAL=0 did not expose raw samples. A steady external
rotation within the acceptance envelope cannot be distinguished from DC bias by
these sensors alone; keep the car physically stationary during calibration.
Temperature drift and long-term heading error remain physical-test concerns.

STOP-only serial `g` now reports ACC=x,y,z, GRAW=x,y,z, REJECT, LAST_REJECT and
REJECTS. Reject mask: 2=tilt, 4=Z orientation/magnitude, 8=raw gyro limit,
16=unstable gyro window. WAIT_STATIONARY state 4 indicates lack of stopped
ownership. REJECT describes the latest calibration sample; LAST_REJECT and
REJECTS survive automatic peripheral retry and clear on explicit `c`.
Successful calibration is S=2, CAL=200 with fresh AGE and no pending frames.
Zero displayed yaw before READY is not proof of a working angle estimate.

## Validation

ARM build passed: text/data/bss 102080/64/12048. Complete sign_line, gyro_turn
and line_recovery suites passed. New tests cover positive and negative DC bias
over the old limit, bias-corrected integration, unstable/tilted/inverted/outlier
rejection and STOP ownership. The former constant-1000-raw timeout fixture was
changed to alternating +/-1000: constant DC is now calibratable, variation must
still time out. Imported 44.999/45/45.001-degree mirrored return tests passed.

BIN SHA256: FE4C8E49114DBA4A519FBE07CD6056DC539F40F1333F664AFD3F85CF56D12858
HEX SHA256: F5BF543E55ABD86BC495A5CDD57A7EDCA49101FD626733B9BFFB0E27BFCF2F37
Artifacts: manual-build-unified-motion/exp7_unified_motion.bin and .hex.
K210, motor direction mapping and audio implementation unchanged.
Board remains V11 fda63ce; no new hardware result or calibration success claimed.
