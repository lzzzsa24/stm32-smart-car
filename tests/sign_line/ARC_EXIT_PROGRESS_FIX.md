# Enable exit navigation after early capture

role: branch implementation and host verification
start_commit: bf3e8e1; current main bbeb31c documentation refreshed in 75b82b7
recorded_deployment: comprehensive 3a00fa8, K210 20e8c72
files_changed: sign_route.c, sign_route_config.h and sign_line regressions

## Reproduced faults

The added early-capture regression failed before the controller edit:
selected outer followed by stable center at 20 degrees left navigation in
PROBE instead of ARC. A later opposite circle curvature can then violate the
PROBE angle check, yielding CANCEL and ordinary following with no exit plan.
The latest floor report did not identify the displayed state, so this is a
reproduced software path, not a confirmed trace of that particular run.

At 500 CPS, the configured 47 mm wheel and 129 mm track imply an ideal
single-side 90-degree pivot duration of about 2.85 s, already beyond the old
2.5 s selection timeout. Wheel slip and ramp-up can take longer. A sampled
heading could also skip the +/-10-degree exit band and keep turning.

## Changes

- Verified entry_line_ready now enters ARC immediately in PROBE/SELECTING,
  while preserving the prior entry angle bounds. No forced entry movement is
  added. The older 60-degree path remains a fallback, not a prerequisite for
  the confirmed selected-outer -> stable-center handoff.
- Track the entry turn's extreme yaw, including continued entry after early
  center capture. Freeze the arc origin after >=20 degrees of opposite arc
  curvature and >=150 mm travel. Pure yaw search cannot alone freeze it;
  excessive continued entry beyond 120 degrees still withdraws navigation.
- Keep the 170-degree arc exit trigger measured from that origin. Alignment
  gets a separate 6 s deadline; entry selection remains at 2.5 s. Overshoot
  commands a forward pivot back toward the saved approach heading. Crossing
  the target is accepted only within +/-15 degrees, or the usual +/-10 band.
  A 20-degree overshoot must be corrected before straight motion.
- Preserve bounded straight exit travel (2 s / 250 mm), stable line capture,
  manual STOP, first-frame pause, branch-search handoff, and gyro validity.

## Verification and limits

Full sign suite passed: regression fails on old controller and passes after
fix, mirrored 20-degree capture -> 80-degree entry apex -> semicircle -> four
seconds of slow alignment -> skipped heading band -> straight line reacquire;
frozen origin, zero-travel yaw, 20-degree overshoot correction, 6 s stalled
alignment withdrawal, existing timeout, STOP, trace/horn/K210/gyro regressions.
Formal ARM build passed: text/data/bss 107384/64/18320 bytes.
HEX SHA256: 54B0B0314C9DB4B1FE474EC84BB5A8E5D3B723C863D700BDE230E3D996A54B37.
No serial access, flash, physical motion or remote operations performed.
No claim that every possible repeated-circle failure is eliminated: absent
branch capture, invalid yaw, bad geometry or failed exit can still CANCEL;
CANCEL retains the previously requested ordinary following/search behavior.

integration_notes: cherry-pick only this final delta onto 3a00fa8 or its
successor. This development branch does not contain 3a00fa8's independent
mode1 speed patch; do not flash its entire image as a replacement for the
current combined firmware. Preserve current mode1 speed and all other modes,
then rerun sign_line and the formal build on the integrated source.
