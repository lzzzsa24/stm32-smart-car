# Mode 1 track speed candidate

role: isolated tuning worker for 黑线避障 integration

start_commit: 948fff8d3477bbe72b24a98932c9cf970956c71c (canonical main at worktree creation)

result_commit: the commit containing this record; exact SHA is also in the external completion packet

branch: test/mode1-track-speed-20260909

## Change and scope

Mode 1 now opts into a higher stable-centre speed ceiling on each normal
line-following cycle. Both middle sensors must see black, with both outer
sensors white, continuously for 350 ms before acceleration starts. The
starting equivalent-PWM command remains 2600; the ramp remains 20 per 20 ms.
The ceiling rises from 2700 to 2850, with an explicit final clamp because
2850 is not a multiple of the ramp step. The production DriveBase mapping
converts those ceilings from 3815 to 4544 CPS (about 19.1% higher target).

Any raw departure from centre removes the added straight acceleration.
Single-outer pivot/counter-rotation, adjacent-pair correction, curve PD,
transverse-mark handling, 60/100-ms white gaps, silent loss search and the
500-ms return settling phase keep their previous commands. Reset clears the
opt-in. Mode 2 keeps its original ceiling; a mode switch cannot retain boost.
The mode-1 wrapper reapplies the opt-in after automatic reset or bypass return.
Ultrasonic caps and STOP/motor ownership still apply after target calculation.

Only clear continuous return after the measured inward angle exceeds 45 degrees
gets the higher bypass cap: 1800 -> 2100 CPS (16.7%). Main already requests
2300 CPS, so it reaches the new cap. Lower configuration values are respected,
including the original 1700-CPS default. Short 20/40-mm obstacle probing and
reverse segments remain capped at 1800 CPS. Gyro angles, turn speeds, obstacle
thresholds, return contact acceptance and motor assistance are unchanged.

files_changed:

- Core/Inc/line_tracking.h
- Core/Src/line_tracking.c
- Core/Src/main.c (one mode-1 opt-in call)
- Core/Src/line_obstacle_bypass.c
- tests/line_recovery/test_line_recovery.c
- tests/line_recovery/test_line_turn_load.c
- tests/line_recovery/check_mode12_integration.py
- tests/gyro_turn/test_return_gate.c
- tests/gyro_turn/test_real_drive.c
- this candidate record

## Verification

verification_completed:

- Full line_recovery suite, both configured search speeds. New cases cover
  349/350-ms eligibility, gradual ramp and exact ceiling, all 15 non-centre
  raw patterns after acceleration, latest left/right edge on loss, cap/STOP,
  return settling, mode reset and tick wrap.
- Real DriveBase regression checks four targets at 4544 CPS, ultrasonic
  limiting, immediate outer pivot, STOP and mode-2 restoration to 3815 CPS.
- Full gyro_turn suite. Production bypass/gyro/DriveBase simulation reaches
  both return directions and sustains 2100 CPS for five seconds; existing
  contact, obstacle, STOP and IMU validity checks pass. Boundary injection
  also verifies lower return configurations and the continuous speed floor.
- Complete sign_line, dfplayer and vision_line_v4 suites.
- Formal build_unified_motion.ps1: text/data/bss 102032/64/12032 bytes;
  BIN 102100 bytes. git diff --check and project-state checker pass.
- The source/test patch passes read-only git apply --check against
  comprehensive worktree a56f6c7da7e49c4df75f1a737f3f09753dfd113f.

BIN SHA256: A1D3F3EB1C3D112F65385AB7A59BCDFED8E5A00D9FDAA0C5D94DC846893E2025

HEX SHA256: 2244616C4B0CA7725F66D5A7652B394C60A66FBD424AD3CEE4BB2B8D77D4ECA8

not_verified: physical wheel speed, traction, stopping distance and lap time.
No serial port, flashing, movement or remote push was performed.

risks_or_assumptions: This is a moderate target-speed experiment, not a measured
lap-time gain. Tracks that never produce sustained middle-pair contact will
not engage the extra straight boost. A 2600 ultrasonic cap still limits that
segment to its previous speed. Greater momentum may increase corner overshoot;
compare a full lap including left/right acute corners and obstacle return.

## Integration and rollback

integration_notes: Apply only this candidate commit to the coordinator's
latest comprehensive source, then run regressions and rebuild that composite.
Do not replace the comprehensive tree with this main-based worker tree or use
this worker BIN as the latest full comprehensive image. At inspection,
PROJECT_STATE.md still named 917df47/426dedd, while main's newer deployment
record 948fff8 identifies board source a56f6c7. The five relevant line/drive/
bypass implementation files are identical between a56f6c7 and worker baseline;
the newer sign-mode changes are outside this patch. The coordinator owns the
shared-state update. This worker did not edit PROJECT_STATE.md.

For rollback, revert this single tuning commit on the integrated branch and
rebuild, preserving subsequent unrelated sign/controller changes. Baseline
948fff8 remains named by local rollback tag rollback/mode1-track-speed-20260909.
Latest observed board-source rollback is a56f6c7, distinct from the worker base.
