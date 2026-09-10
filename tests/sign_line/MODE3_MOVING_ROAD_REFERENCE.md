# Mode 3: road reference and irrevocable exit handoff

Historical design, superseded by the user's stopped-heading-first instruction.
See [MODE3_STOP_HEADING_PRIMARY.md](MODE3_STOP_HEADING_PRIMARY.md) for current
mode3 reference ownership and exit gates. The passive exit handoff remains;
the moving-reference prerequisite and relative-sweep exit decisions do not.

Base: `af602cf80b9f5b12405b693a57898e485702f051`, the recorded flashed composite.
Rollback: `rollback/mode3-before-road-reference-af602cf`.

## Problem

Double-middle contact at a stationary recognition stop does not prove chassis
alignment with the road. The former unconditional stopped-heading reference
shifted both the forced-exit gates and the natural-exit completion window.
A car already on the outgoing road could retain ARC until an unrelated bend
crossed the old angle gate.

## Mode 3 changes

1. Before PROBE, IDLE/ARMED can qualify a moving road heading from at least
   200 ms of centered-line observations, at least 60 mm encoder travel on each
   side, and a yaw range no greater than 4 degrees. Both sides must advance.
   A 10 ms motion window tolerates encoder quantization in the 1 ms main loop.
   White, broad/outer contacts, stationary samples, reverse-side movement,
   invalid IMU or sample gaps cannot build a qualified window.
2. The reference expires after 5 s before entry, excluding an observation hold.
   Once PROBE begins, the road heading is frozen through the arc. Observation
   cannot replace a qualified road reference with a skewed stopped pose.
   Without a qualified reference, the stopped/PROBE pose remains diagnostic
   only: it cannot authorize the old absolute-angle forced exit.
3. Independently of that reference, record directed yaw extrema while moving
   forward on an unambiguous arc line. Do not seed this history from earlier
   in-place search extrema. At least 150 mm of arc travel, 110 degrees of
   observed arc sweep and a 15-degree return from its peak create a departure
   candidate. The midpoint's zero heading alone does not qualify.
4. A candidate immediately enters passive EXIT LINE. Existing live sensor
   tracking/search owns every motor sample. It cannot return to ARC/EXIT TURN.
   Confirm with forward travel of 40 mm, at least 120 ms of line evidence,
   heading spread at most 6 degrees and current middle contact. No requirement
   remains to align with the possibly biased stopping pose on this path.
5. If passive confirmation fails within 600 ms or exceeds 140 mm, CANCEL the
   old route without STOP. Also cancel a missed arc window after more than
   200 degrees of observed sweep. Both retain ordinary tracking/search; neither
   can execute a delayed old exit turn. Completion clears the pending direction.

Qualified-reference 55/65-degree exit gates and prior first-black exit
reacquisition remain. The 2 s recognition stop, 26-percent stop gate,
double-middle centering search, RGB-off/horn behavior, encoder target interface,
and all speed settings remain. Mode 4 retains its independent trajectory.

## Diagnostics

STRACE appends three fields after the prior 17 CSV fields (older fields retain
their order):

- `road_ref_valid`: 1 means a qualified moving road estimate; 0 means unavailable
  or provisional. Existing `pause_ref=1` alone no longer implies trust.
- `exit_reason`: 0 none, 1 angle exit, 2 natural departure, 3 missed window.
- `arc_sweep`: directed visible-line yaw range in millidegrees.

Reason 3 uses navigation warning 9 and CANCEL, not a motor STOP. Trace preserves
diagnostic angles after cancellation/completion even though they no longer have
control authority. No new serial writes occur while moving. External CSV readers
must tolerate the three appended fields.

## Validation

- Full `tests/sign_line/run.cmd`: PASS on final sources. Includes direct route
  and actual route/follower/DriveBase tests with both signs, stop offsets
  -30/-20/-10/0/+10/+20/+30 degrees, qualified and unavailable references,
  immediate opposite-line correction during passive confirmation, subsequent
  ordinary bends/re-loss, STOP and clock wrap.
- Negative tests cover stationary center contact, full-black input, continuous
  curvature, 1 ms encoder quantization, stale/invalid reference, midpoint
  correction, ambiguous/white departure, failed passive confirmation and a
  missed full window. Existing mode 4, centering, 2 s stop, exit reacquisition,
  IMU, trace, horn and K210 tests pass.
- Mode 1/2 integration, gyro integration and mode 5 app-profile checks: PASS.
- Formal `build_unified_motion.ps1`: PASS; text/data/bss = 118556/64/25696 bytes.
- Canonical project state checker and `git diff --check`: PASS.

No serial, flash, lifted-wheel or ground validation was performed. These windows
are track-specific initial values, not calibrated physical results. Encoder
travel is wheel motion and may slip; one transverse sensor row still cannot
measure heading from a single frame. A sufficiently long straight approach is
needed to qualify the reference. Without it, ordinary line following and the
independent departure detector continue, but an absolute-angle forced exit is
deliberately unavailable. Cancellation prevents stale steering, not a guarantee
that the vehicle has physically left the circle.
