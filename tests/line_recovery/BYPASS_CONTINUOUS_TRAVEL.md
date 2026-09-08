# KEY1 short bypass legs: slow continuous travel

## Observation and reproduced path

The user reports a small sudden advance followed by wheels that appear stuck,
repeating several times. Both forward creep and opposite-side motion may be
visible. The user explicitly rejects endpoint micro-pulses because they do not
move the chassis on the actual surface, and requests slow continuous motion.

Current canonical starting point: 16e525e69f129291de5dcd79810834faf0ac9041.
The latest recorded deployed firmware is 3170221, whose KEY1 profile requests
1900/2600/2200/2300 CPS for reverse/advance/clear-probe/return and 2500 CPS turns.
The old bypass sends 20/40-mm translation segments to EncoderLinear, which uses
DriveBase endpoint position control. The real-source host reproduction with
2600 CPS and 70 remaining counts returns a 787-CPS target for both distances.
That is within the existing endpoint pulse region. Individual endpoint
corrections can also reverse a wheel. This demonstrates an inappropriate
software path; it does not prove that every reported ground turn stall has
the same cause. Actual bypass turns already use continuous LineBypassTurn.

## Change

- KEY1 bypass translations now use LineBypassTravel. All four wheels request
  same-direction velocity throughout the segment, including its final counts.
  The existing acceleration ramp, battery compensation and speed PI remain.
- Accepted requested speeds are 1400..3600 CPS; the short-leg target is capped
  at 1800 CPS, for both forward and reverse travel. Consequently the deployed
  reverse/forward/probe/return profile all uses 1800 CPS for these short legs.
  Lower existing requests remain lower. The position-pulse controller is never
  entered and no individual wheel reverses to seek its exact endpoint.
- Stop the whole car when mean signed encoder travel reaches the target and
  every wheel has reached at least 75 percent. Then use the existing whole-car
  brake with 120-ms settling, bounded by 300 ms. These finite segment-end stops
  remain; this change is not uninterrupted cruising through every maneuver.
- Signed travel includes braking travel and handles encoder/tick wrap. Three
  moving wheels cannot conceal one stationary wheel. Distance is wheel travel,
  not measured chassis displacement; endpoint overshoot is possible.
- Invalid requests, external STOP, drive faults and insufficient progress keep
  bounded fault handling. Each segment times out after four nominal travel
  times plus 1200 ms, at least 2000 ms. Existing main automatic-wait handling is
  unchanged; no new infinite wait is introduced.
- Side-IR interrupts, route/line reacquisition, turning speed, turn controller,
  mode dispatch and shared DriveBase/EncoderLinear are unchanged. The new
  module is only used by line_obstacle_bypass, so other position users retain
  their existing implementation.

## Verification

`cmd /c tests\line_recovery\run.cmd` passes, including both configured search
speeds, real DriveBase/turn/bypass, STOP ownership and RAM fault logs.
The reproduction is retained alongside the corrected-path tests:

- 20/40 mm in both directions and at signed encoder/tick wrap: slow continuous
  1800-CPS requests and all four same-direction PWM outputs in the former tail.
- Whole-car completion, invalid/occupied requests, external STOP, stationary
  wheel, real stall fault and bounded insufficient-progress timeout.
- Real KEY1 sequence in both directions, including default and deployed power
  profiles, zero-travel valid IR turn completion and ordinary 45-degree turn.
- Twelve consecutive forward segments for each direction without position
  mode, followed by too-close IR causing outward turn and invalid IR fault.

Formal `build_unified_motion.ps1` passes: text/data/bss 83824/64/11584.
BIN size 83892; SHA256
5757D92BC4B2741AE23908A0D494B5DD36757281F36AA9CC60766F9E46FB5E63.
HEX SHA256 E5FE872642E0D49DD66271D2778FE624026DBA4EEF3E938F0B7D2762C648B8CE.

No serial access, flash, reset, lifted-wheel or ground test. Continuous speed
control is not a guarantee of motion on a high-friction or wrinkled surface.
The real turning effort issue still needs physical evidence if it persists.

## Integration

Return this worker commit to the integration task. Do not edit PROJECT_STATE
in the worker. Previous visible-line arc candidate c5a4c76 is a separate
increment and is not included in this baseline or patch; preserve/apply it
separately if selected. Review test-file overlap and rebuild actual combined
main. Formal build discovers Core/Src/*.c automatically; other build systems
must include the new line_bypass_travel.c. The host run.cmd has been updated.
