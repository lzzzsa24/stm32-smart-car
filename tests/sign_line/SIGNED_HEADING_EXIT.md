# Mode 3: stopped-road reference and signed exit heading

2026-09-10. Base: deployed composite `ed7eb3b8dbe9301b61374ad3a81fa32f18050203`.
Worker: `fix/sign-gyro-exit-straight`; rollback:
`rollback/sign-before-exit-choice-ed7eb3b`. This supersedes the mode-3 angle
and unfinished-exit ownership rules in `EXIT_COMPLETION_AUDIT.md`.

The user reports intermittent additional laps, asks for a very small increase
in observation-stop confidence, and explicitly proposes the two-second stop
as the heading reference. Their drawing distinguishes lower/upper semicircle
by **signed heading**, not by an unsigned magnitude or a guessed entry tangent.

## Geometry and reference

In the user's right-positive drawing, a right semicircle goes from positive
heading through zero to negative heading. The firmware MPU convention is
left-positive, so those signs reverse. Normalize once for both sign choices:

```text
heading_error = wrapped(current_MPU_yaw - stopped_road_yaw)
road_heading = sign_direction * heading_error     (L=-1, R=+1)
lower half: road_heading < 0
middle:     road_heading approximately 0
upper half: road_heading > 0
```

Mode 3 freezes fresh MPU yaw on the falling edge of the real two-second
observation pause, before the follower resumes motion. A later correction or
crossbar does not move that reference. Ending a pause does not start an entry
turn in mode 3. The existing selected-side line/center capture and entry bounds
still determine when the controller enters ARC; this is inferred track
evidence, not a separate physical position sensor.

An unconfirmed repeat pause replaces the candidate reference. If there was no
usable pause, gyro was invalid at completion, or the reference is over five
seconds old when PROBE begins, PROBE yaw is the fallback. Expired direction,
cancel, completion and reset invalidate reuse. Once accepted into the route,
the pause reference does not expire halfway through the semicircle.

The estimated entry apex and `arc_peak` are retained for diagnostics. They no
longer open mode-3 exit selection or determine its arc-angle warning limit.
The previous 150/170-degree exit constants have been removed.

## Exit selection and completion

After entering ARC, with at least 150 mm encoder-estimated forward travel:

- At signed `road_heading >= 70 deg`, a narrow line at the outgoing side
  permits early exit selection. With a right choice that is the right outer
  sensor (alone/adjacent); left is mirrored.
- At `road_heading >= 90 deg`, any narrow-line pattern permits exit selection
  even if the outer-side signature was missed. The existing 30 ms confirmation
  is retained. White, all-black and both-sides/wide patterns cannot trigger it.
- EXIT TURN keeps the qualified forward pivot while heading is not aligned,
  including when the still-continuous ring line remains visible. Otherwise
  ordinary line tracking could revoke the exit and follow the ring for another
  lap. Wide/both-side evidence still suspends route turning.
- Alignment uses the actual road-heading target: +/-10 deg, or a crossing of
  the target within +/-15 deg. There is no additional mandatory 45-degree turn.
- Heading divergence greater than 15 deg from the best achieved error cancels
  an active alignment command on either white or narrow-line input. The
  existing six-second / 135-degree alignment bounds remain.
- Having observed the upper exit region on a narrow line, a natural return
  to centered line within +/-15 deg for 30 ms completes the route directly.
  The earlier midpoint passage through zero is simply the middle of the arc;
  it has not passed through the upper exit region.
- EXIT LINE immediately yields forced straight travel on outgoing-line
  contact. A subsequent loss cannot restore that blind straight command.
  Stable centered capture clears L/R and all route commands and goes to LOCK.

Mode-3 ARC warnings now use road heading outside [-120,+120] deg, plus the
existing 20-second / 3 m bounds. CANCEL releases navigation to live tracking
or search; it adds no stop. Mode 4 retains its independent drawn trajectory.

The 70/90-degree candidates assume this track's opposite circle connections
and approximately parallel approach/departure roads. They need a ground test;
gyro heading alone does not certify a physical position or correct a bad
stationary reference. Encoder counts are wheel travel, not measured chassis
displacement. No new on-board report or physical run was collected this turn.

## Stop threshold and diagnostics

The shared mode-3/4 arrow-triggered observation-stop gate changes **25% -> 26%**.
The fixed two-second duration, unconfirmed 0.5-second retry and confirmed
direction inhibition are preserved. STM32 direction voting remains at 20%;
K210 threshold/model/script and horn behavior are unchanged.

SIGN telemetry adds `REF=PAUSE`/`REF=PROBE` and `H=` (heading error, millidegrees,
MPU left-positive). STRACE keeps its first 15 columns and appends `pause_ref`:
1 means the active route has a usable pause-derived reference. Existing `yaw`
and `arc_peak` columns still describe phase-relative diagnostic angles, not
the new signed exit criterion. `heading_error` is the straight-road reference.

## Validation and integration

Before repair, a real route -> shared line follower -> DriveBase regression
failed when continuous center-line evidence disabled the qualified exit.
A second regression failed because the crossbar overwrote the stopped heading
after a 20-degree approach correction. Both pass after this change.

Passed on the final control source:

- Full `cmd /c tests\sign_line\run.cmd`: mirrored continuous-line exits,
  early/outside capture, direct natural completion, later opposite bends and
  losses, STOP, 32-bit tick wrap, stopped-reference preservation, 26% gate,
  unchanged slow-profile parity, horn/K210 identity, MPU/FIFO/bus and mode 4.
- Reference replacement, five-second fallback, reset and invalid-gyro cases;
  signed heading gives the same exit decision despite a different entry apex.
- `check_mode12_integration.py`, `tests/gyro_turn/check_integration.py`, and
  `tests/sign_line/check_integration.py`.
- Formal `build_unified_motion.ps1`: text/data/bss **111564/64/22432**,
  BIN **111632 bytes**. `git diff --check` and canonical state checker passed.

BIN SHA256: `7F2EFBF7E33CC01F00A18CD6FE71CD18D77435F5839FC2703E591E0B29D6CDCF`

HEX SHA256: `38EE48990B205D633FD4A98CF650119FD0DEC890F0A44CFDF314A12A52D91842`

No wheel-speed, encoder PI, motor polarity, mode-1/2/5 control or K210 source
changes. Mode-4 trajectory is preserved; only its shared observation-stop
threshold and shared diagnostics change. No serial, flash, merge or push.
Integrate only this worker's new delta on the current composite; current main
rc.5 has a different sign-controller baseline and must not be overwritten with
the worker's whole checkout or image.
