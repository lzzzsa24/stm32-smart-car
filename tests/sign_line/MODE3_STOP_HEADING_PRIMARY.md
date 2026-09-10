# Mode 3: stopped heading is primary; wider exit tolerance

Base: `891a7e193947bd9f0e33319cf4e28c1c68528d93`, the recorded deployed composite.
This follows the user's explicit instruction to keep the observation-stop
heading as the highest-priority reference and widen the exit angles. It
supersedes the moving-reference policy of 50ce17f and does not require the
rejected, unintegrated local-sweep candidate 7067f7b.

## Reference ownership

At completion of the actual two-second observation hold, valid MPU yaw always
replaces the mode3 reference. Later straight following, the crossbar, entry
steering and search extrema cannot overwrite it. A subsequent completed
observation can replace it; route completion/cancellation, sign expiry, invalid
IMU and reset invalidate the old reference. If no usable observation exists,
PROBE may use its current yaw as an explicit fallback; a completed observation
always takes precedence. `pause_ref=1` identifies the primary stopped reference.

No straight run is required for calibration. Moving-line reference learning
and its 60 mm prerequisite were removed. Arc-relative sweep is still logged
for diagnosis, but it cannot trigger exit or cancel a route at 110/200 degrees.
The 120/150-degree local-exit logic from 7067f7b is not in this branch.

## Mode3 angle changes

All heading gates below are relative to the stopped reference, with sign
direction applied so the lower half is negative and upper half positive.

| Condition | Previous | New |
|---|---:|---:|
| Outgoing-side outer contact can request exit | +55 deg | +45 deg |
| Other unambiguous line can request exit | +65 deg | +55 deg |
| Upper-region flag for natural completion | +70 deg | +45 deg |
| Alignment band that yields to live following | +/-10 deg | +/-25 deg |
| Zero-crossing / centered capture band | +/-15 deg | +/-30 deg |
| Returned-heading range for natural-departure verification | +/-30 deg | +/-40 deg |

Existing 30 ms confirmation, minimum150 mm arc travel, ambiguous-line rejection
and direction signs remain. The prior early outward-return edge rule remains.
Full black or both-side ambiguity cannot initiate a forced exit turn.

Natural departure still requires lower-half evidence, an upper-half peak of
at least25 degrees, a return of at least15 degrees from that peak, current
unambiguous line, and forward wheel motion. Its peak/drop are relative to the
stopped heading; they are not cumulative turn since the guessed entry point.
A candidate immediately enters passive EXIT LINE, removing ARC motor authority.
Forward line travel of40 mm and120 ms within a6-degree yaw spread confirms it.
A failed passive window cancels without STOP or a later old turn. Merely
standing still at a centered sensor pattern cannot complete this path.

Existing first-black-after-white exit handoff remains immediate, including
before alignment. The two-second recognition stop, all-white centering search,
mode2 slow encoder targets, RGB/horn and modes1/2/4/5 are retained.

## Validation

- Direct route regression varies stopped heading over +/-10/20/30 degrees,
  mirrors both signs, and varies entry apex from20 to100 degrees: the same
  stopped-heading 45/55 gates still apply. It verifies25-degree alignment,
  natural completion35 degrees away from the stop, midpoint rejection,
  first-black handoff, no late old turn, and clock wrap.
- Diagnostic sweep above200 degrees does not itself cancel or trigger exit.
- Actual route/follower/DriveBase regressions exercise preceding straight
  travel present/absent, stopped bias, current opposite-line corrections,
  normal bends after exit, subsequent search, and manual STOP.
- Existing sign-line, mode4, observation, centering, gyro, trace, horn and K210
  cases remain in the full sign suite. Threshold assertions now reflect the
  user's widened tolerances; natural-departure tests include actual encoder
  travel rather than treating stationary yaw changes as driving.
- Formal ARM build: text/data/bss =118020/64/25656 bytes; BIN118088 bytes.
- Mode1/2 integration, gyro integration and mode5 app-profile checks pass.

No hardware was accessed or flashed. Wider bands tolerate stopped-pose error
but cannot guarantee exit under every offset, slip or sensor pattern; ground
validation is still required. This is a targeted policy change on the latest
deployed composite, not a whole-firmware rollback.
