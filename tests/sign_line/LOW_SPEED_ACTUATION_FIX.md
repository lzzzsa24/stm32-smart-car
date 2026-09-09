# Modes 3/4: entry oscillation and ineffective exit steering

## Observations and baseline

The user reports that entry now oscillates left/right more than the previous
version and that exits remain unreliable. The recorded board source is
`5fdb84a`; the latest unflashed composite is `8b91f49` (mode 1 offset changed to
240 mm). Canonical main was rechecked at `4f58372`, whose change since `8ce89b1`
is documentation only. Preserve the earlier candidate at
`rollback/sign-before-early-arc-3a00fa8`; this task did not restore or flash it.

STOP-only serial diagnostics were saved to:
`F:/myproject/jidian/validation/sign-read-20260909/trace-162319.log`.
The read used commands `0`, `j`, and `g`, with DTR/RTS disabled. The complete
256-row trace ended with `STRACE END`; the final DRV records show STOP and zero
targets, measured speeds, and PWM on all wheels. No driving command was sent.

The retained ARC segment contains 80 all-white samples in 149 samples. Its
phase yaw rises from about 79.8 to 175.5 degrees. The following EXIT_SELECT
segment covers only 1.528 seconds: encoder travel reaches 135 mm, while phase
yaw initially rises to +20.6 degrees and ends at +13.6 degrees with a right
exit command. This records an attempted exit without completed alignment in
that segment. It does not prove a timeout or capture the entire failed run.
A later IDLE segment has prolonged outer-only contact with almost no yaw
change before all-white search produces a rapid yaw change.

The stationary IMU query reports READY, 200 calibration samples, zero fault,
22 ms age, no backlog/restart, and +19 millidegrees/second rate. These values
do not identify an IMU outage as the cause of this saved segment. The trace
does not contain moving PWM; the actuator issue below was established from
source and a host reproduction, not inferred as a measured PWM value.

## Changes

1. Remove the artificial `-route.direction` search hint on transition into
   ARC. Early ARC entry may occur while the car is still turning onto the
   chosen branch. Immediate line loss must retain the observed direction,
   instead of reversing solely because the state name changed.
2. Add opt-in low-speed actuation in DriveBase SPEED mode. Previously every
   nonzero sub-1412 CPS command was clamped to continuous PWM >= 2200, so
   different low targets could converge to the same actual output. The new
   path accounts for encoder movement and alternates existing calibrated
   powered output with coast intervals to regulate average low speed.
   Per-wheel budgets reset on STOP, direction changes, and enabling/disabling
   this path. Suspect direction/signal feedback uses bounded pulse density;
   it cannot accumulate wrong-sign encoder counts into continuous power.
3. Enable this path only from modes 3/4; all other modes explicitly disable it.
   Position control bypasses it even when enabled. The shared driver has a
   new opt-in path, but its default behavior remains unchanged.
4. Cap counter-rotation search at 500 CPS in modes 3/4. It previously bypassed
   the sign speed cap. Existing forward caps remain 1200 CPS normally, 700 on
   all-black, and 500 after a recognition frame. Existing route-angle gates,
   exit heading alignment, observation pause, horn, and K210 script are retained.

## Verification

- `tests/sign_line/run.cmd`: passed, including real route/follower entry
  handoff, immediate all-white after early ARC entry, gyro exit logic, pause,
  horn, trace, K210, and the new real DriveBase PWM regression.
- `tests/line_recovery/run.cmd`: passed at both configured speeds, including
  real DriveBase, load/fault and position-control regressions.
- `tests/gyro_turn/run.cmd`: passed, including gyro/FIFO, bypass and return.
- `build_unified_motion.ps1`: passed; text/data/bss = 108208/64/18344 bytes.
- Candidate HEX SHA256:
  `217AA977D2CEFAF13E166BDD670ADD61B0617397E79EC632C1375FADDEDCFADC`.
- State checker passed on canonical main; its artifact warnings refer to
  different source/artifact snapshots and are not new deployment evidence.
- `git diff --check`: passed.

`test_low_speed_drive.c` links the real driver to mocked PWM pins and an
explicitly synthetic inertial wheel response. At targets 400/1200 CPS the old
path reproduces equal continuous output and modeled averages 1997/1997 CPS;
the enabled path yields 399/1201 with coast intervals. It also covers 500/500,
-500/500, one stationary side, tick wrap, STOP, disabling the option, degraded
feedback, and identical position-control PWM sequences with the option on/off.
Those speeds are host-model results, not measured car speeds.

This candidate was not flashed or driven. Real tire friction, inertia,
left/right wheel coupling and the chosen coast timing can still cause motion
ripple or insufficient yaw response. No new active reverse braking is added.
The changes repair two reproducible software problems; stable physical entry
and exit remain to be checked on both semicircles with the new firmware.

## Integration packet

```text
role: sign-mode fix implementation; local commit and handoff only
start_commit: 96b8372 (parent 716c5b4; earlier functional sign fix 0768639)
result_commit: the commit adding this document; resolve with git log -1 --format=%H -- tests/sign_line/LOW_SPEED_ACTUATION_FIX.md
files_changed: Core/Inc/drive_base.h, Core/Inc/sign_slowdown.h,
 Core/Src/drive_base.c, Core/Src/main.c, Core/Src/sign_slowdown.c,
 Core/Src/simple_line_mode.c, tests/sign_line/check_integration.py,
 tests/sign_line/run.cmd, tests/sign_line/test_entry_handoff.c,
 tests/sign_line/test_sign_line.c, tests/sign_line/test_low_speed_drive.c,
 tests/sign_line/LOW_SPEED_ACTUATION_FIX.md
verification_completed: formal ARM build; sign, line, gyro host suites; state and whitespace checks
not_verified: candidate flash/readback, lifted-wheel response, floor entry/exit
risks_or_assumptions: healthy encoder response normally; synthetic plant is not a floor model
integration_notes: cherry-pick only this final delta onto 8b91f49 or its successor;
 preserve mode 1 rectangle/speed changes. Do not deploy the entire worker branch.
```

The worker branch does not include the independent newest mode 1 rectangle and
speed changes. Its built HEX is verification output, not a replacement for the
latest comprehensive image. Rebuild the integrated source before any separately
authorized deployment. Test-only commit `96b8372` synchronized a stale gyro
ownership assertion to the already-deployed composite; it need not be replayed
onto that composite. No remote push or canonical state edit was performed.
