# Modes 3/4 low-speed breakaway power

The user reports that the wheels cannot turn normally, especially while
steering. Canonical main was checked at `59ed898`; its current deployment
record identifies `ec2dd2f`, which replays `11a243f` onto the comprehensive
24-cm bypass source. The affected DriveBase source and low-speed tests are
identical between that deployment and this worker's `11a243f` baseline.
Rollback reference `rollback/sign-before-torque-11a243f` preserves the baseline.

## Cause and narrow fix

The preceding change retained low-speed encoder accounting, but powered
intervals still fell through to the continuous-speed feedforward/PI output.
At a 500-CPS command and 7800 mV, its PWM starts near 2200. Even the maximum
PI correction is only 500 PWM. Both the existing recovery boost and turn-load
helper require targets of at least 1412 CPS; the sign low-speed path also
clears the timed boost. Thus a stalled low-speed wheel can remain below the
power required to start under load. Raising only the requested average speed
would work against the requested slow sign recognition.

Only the powered phase of the already opt-in sign low-speed path changes:

- Straight travel uses 3000 PWM at the reference 7800 mV.
- Differential steering, including one stationary side and counter-rotation,
  uses the existing per-wheel/direction recovery profile: 3350 PWM on M1/M2/M4
  and 3250 on M3 at 7800 mV.
- Existing battery compensation applies, with a 3400-PWM ceiling. A zero
  target remains zero. Encoder-accounted coast intervals still regulate
  average low speed; the low-speed path no longer falls through to the weak
  continuous-speed PI output. It clears that PI accumulator on powered cycles.
- No route decisions, direction mapping, recognition threshold, target-speed
  caps, observation pause, gyro exit rules, horn or RGB behavior change.
  The gate remains SPEED mode, sign opt-in, and nonzero magnitude below
  1412 CPS. Modes 1/2/5 opt out; position control is unaffected.

This is a powered-output correction, not a new route-state-machine iteration.
No serial port, flash, physical-car command, remote push or main edit occurred.

## Evidence

The previous host wheel model assumed any nonzero PWM would move a wheel;
it missed static friction. The extended test adds an explicitly synthetic
start threshold of 2800 PWM straight / 3100 turning, a lower 2500 holding
threshold, and wheel inertia. These are test parameters, not measured car
calibration. Before the fix, the real old driver failed the assertion that all
four turning wheels move within one second. After the fix, it passes.

`tests/sign_line/run.cmd` passed, including loaded straight, both differential
directions, both counter-rotation directions, one stationary side, low-speed
average regulation, zero/STOP, opt-out, position sequence equality, degraded
feedback and clock wrap. In the synthetic loaded model, targets 400/1200
produced averages 399/1201 CPS, and 500/500 produced 499/499 CPS. These are not
physical speed measurements.

`tests/line_recovery/run.cmd` passed at both configurations, including real
DriveBase load, stall/degraded-feedback, position, bypass and mode-1/2 ownership
regressions. `git diff --check` passed. Canonical state checker passed with the
expected local-build-versus-flashed-artifact warnings.

Formal `build_unified_motion.ps1` passed: text/data/bss 108376/64/18344 bytes.
HEX SHA256:
`6179104DA7E49FA6E395EB777FB88558E77F34277BE3C1FDBB19FBE183237426`.

The candidate has not been flashed or tested on the floor. Higher powered PWM
improves available motor effort, but actual starting torque, pulse ripple and
arc-exit response still depend on battery, friction and wheel coupling. The
existing nonblocking stall-observation policy is retained; a physically stuck
wheel can therefore remain powered. Degraded-encoder pulse density remains a
bounded fallback without a physical speed guarantee.

## Completion packet

```text
role: sign-mode drive-power fix; branch/local-commit handoff
start_commit: 11a243ff75bc0a3c7df691d54233d06938f2b091
result_commit: the commit adding this document; git log -1 --format=%H -- tests/sign_line/LOW_SPEED_POWER_FIX.md
files_changed: Core/Src/drive_base.c, tests/sign_line/test_low_speed_drive.c,
 tests/sign_line/LOW_SPEED_POWER_FIX.md
verification_completed: failing old-driver load reproduction, full sign and line suites,
 formal ARM build, canonical state and whitespace checks
not_verified: candidate flashing, lifted-wheel response, physical starting/steering/exit
risks_or_assumptions: synthetic friction values; increased powered effort;
 real speed and yaw under load remain unmeasured
integration_notes: apply only this delta onto ec2dd2f or its successor;
 retain independent mode-1 rectangle/speed changes and rebuild integrated firmware
```

This worker branch's complete image lacks the latest independent mode-1
rectangle/speed changes. Its HEX is build evidence only; do not replace the
comprehensive firmware with this whole branch image.
