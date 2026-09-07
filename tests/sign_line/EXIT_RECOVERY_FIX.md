# Exit recovery candidate

Base: canonical main 188c96e. Ported the deployed d1d22d9 mode 3/4 source
increments as 9ac1d6b, 2446554, 76c17b7. Canonical PROJECT_STATE is unchanged.

User reports better recognition but occasional misses and wrong turns. The
photo shows M4 SIMPLE, LINE:1111 A:4, VIS:- 0, ROUTE:EXIT TURN L. The vehicle
is held up, so the photo cannot reconstruct ground sensor inputs or prove the
initial cause of the wrong turn. Current deployment record identifies d1d22d9.

Confirmed defects: ARC bounds set warning 3, which bypassed the minimum travel
and heading test for exits. The outside-plus-any-middle test also accepted
all-black. SELECT/EXIT_SELECT bounds merely set warning 2 and never withdrew
the old side preference, so failed exit selection could persist indefinitely.

Changes:
- ARC exit requires exact outward adjacent pair (1100 left / 0011 right),
  valid minimum geometry and no exceeded bound. Ambiguous wide input is not exit.
- Exceeded selection/arc/exit-clear bounds cancel route ownership, clear its
  direction and retain reason 2/3/5. They do not stop the underlying line
  controller or its continuous search. OLED displays CANCEL when on-line;
  all-white still displays SEARCH, telemetry retains underlying state 11.
- Cancellation cannot be rearmed by continuous sightings of the same sign.
  Existing 1500 ms cooldown, fresh no-target evidence for 800 ms and subsequent
  fresh confirmation apply. Manual mode reset remains authoritative.

Original limits remain: selection 2500 ms, arc 20000 ms, exit-clear 2000 ms,
plus existing geometry bounds. These estimates can reject a physically slow
but valid maneuver: fallback follows the line without guaranteeing the desired
route. The exact adjacent-pair requirement can miss other physical exit masks;
sensor/route traces are still needed for calibration. It is not an automatic
return to the missed junction and does not repair model misclassification.

Host sign/ring tests cover left/right valid exits, all-black/both-outer marks,
wrong arc geometry, persistent failed exit, sign hold/rearm, time wrap, continuous
search and operator stop. No serial operation, flashing or physical test.
Branch: fix/sign-exit-recovery. Restore source d1d22d9 to compare behavior.
