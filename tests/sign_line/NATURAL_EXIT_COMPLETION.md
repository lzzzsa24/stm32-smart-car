# Mode 3: recognize an actual departure before the nominal angle gate

2026-09-10. Base `e047c13a2157dc76a0816f9fa69da2c1b6fafde4`, the current
recorded comprehensive deployment. Rollback:
`rollback/sign-before-exit-progress-e047c13`.

The user's latest ground observation is that recognition stops work, while
exit selection remains unreliable and a car that has naturally left the
circle may remain in ARC. This change targets those control paths. It does
not change the observation dispatcher, 26% stop threshold, two-second pause,
recognition voting or the established wheel-speed interface.

## Reproduced failure

Previously, natural departure required `exit_region_seen`, which could only
be set by a narrow line at a signed road heading of at least 70 degrees. A
trace-shaped host scenario with lower-half entry, upper-half peak 55 degrees,
then a centered outgoing line at 0 or 25 degrees never cleared ARC despite
continued encoder travel. The real route -> shared follower -> DriveBase
regression reproduced this before the repair.

The latest observation contains no fresh angle trace, so this is a confirmed
code path matching the symptom, not proof of the exact angles in that run.

## Added natural-departure confirmation

Mode 3 keeps the stopped-road reference and signed angle convention from
`SIGNED_HEADING_EXIT.md`. Multiplying left-positive MPU heading error by
sign direction (L=-1, R=+1) gives negative lower-half and positive upper-half
headings for either chosen half-circle.

A second completion path now recognizes this sequence:

1. On-line lower-half heading of at least 15 degrees in the entry direction.
2. On-line upper-half peak of at least 25 degrees, after at least 150 mm of
   encoder-estimated arc travel.
3. Heading returns at least 15 degrees from that peak, to within +/-30 degrees
   of the straight-road reference.
4. Narrow-line evidence continues for at least 120 ms, with heading range at
   most 6 degrees and at least 40 mm of additional forward encoder travel.
   The completing sample must contain a narrow middle-line contact.

These conditions are observed while normal ARC line following continues;
there is no added stop, timed straight command or speed change. White/wide
input, heading variation above 6 degrees, or a control-sample gap over 50 ms
restarts confirmation. Time without travel cannot complete it. Simply passing
the midpoint has not supplied the preceding upper-half peak.

Confirmed departure calls the existing completion routine: LOCK, direction
zero, route commands withdrawn, and ordinary slow tracking/search owns the
following samples. The previous 70-degree/centered fast-completion path and
active EXIT TURN/EXIT LINE completion remain available.

## Earlier exit-side choice when the car has already begun turning out

The normal 70-degree edge and 90-degree angle gates remain. An additional
candidate allows selection below 70 degrees only if:

- lower-half progress was seen;
- signed heading is still at least +35 degrees but has fallen at least
  8 degrees from the earlier on-line upper-half peak;
- the current narrow pattern includes the chosen outgoing outer sensor;
- the existing 150 mm travel and 30 ms selection confirmation gates hold.

Thus a 60 -> 50 degree outward correction can capture the intended exit,
while a steady 60-degree contact, opposite-side contact or ambiguous wide
pattern cannot invoke this additional branch. Existing angle convergence,
timeout/cancel behavior, forward-pivot CPS and post-exit L/R clearing remain.

## Verification and evidence limits

Passed the complete `tests/sign_line/run.cmd`, including:

- natural departure at a 55-degree peak with outgoing headings 0 and 25,
  mirrored choices, tick wrap and immediate ordinary control after completion;
- continued corner curvature, stationary wheels, white/both-sides/full-black
  masks and delayed samples cannot falsely complete the new path;
- early outward heading return with correct/wrong side evidence, subsequent
  completion and manual STOP;
- existing 70/90-degree exits, pause-reference tests, shared slow-profile
  parity, horn, K210 identity, MPU/bus, mode selection and mode-4 trajectory.

Mode1/2, gyro and mode5 integration checks and `git diff --check` passed.
Formal build: text/data/bss **112812/64/23488**, BIN **112880 bytes**.

BIN SHA256: `91F76814CB9F41C273E5E39D257C5AF7D3B77B553ED229EEC8AA1B7AFB1CD767`

HEX SHA256: `18F2BB638D315433D2F08631AB4804E443B25D0305EFA744B7838ABC8DE11E89`

STRACE appends `exit_heading_peak` after its existing 16 columns, reporting
the signed upper-half peak observed on narrow line, relative to the straight
reference. This distinguishes it from the older phase-relative `arc_peak`.

This is a source/host validation result. No serial access, fresh on-car report,
flash, wheel or ground test was performed. Wheel travel does not prove chassis
displacement, and the heading/line criteria remain candidates for this fixed
track. Mode-4 control, mode1/2/5, motor mapping, encoder closed loop, K210,
RGB-off and horn are unchanged; shared diagnostic layout gains one field.
Integrate only this delta onto the latest comprehensive branch, preserving
any newer independent mode changes. This worker does not update shared state,
merge main, push or flash.
