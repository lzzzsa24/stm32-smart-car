# Sign-course entry direction and bounded heading search

User reports PROBE L followed by an entry U-turn in both sign modes. The
track photo shows rounded corners and opposite connections on the circles,
not a required in-place hairpin. Root-code reproduction: the deployed
c50d3c8 main rewrites SL2 last_direction on every PROBE iteration. A new
opposite inner contact can set the correct curve hint, only to have it erased
before the next all-white sample. An additional post-Step SetDirection can
erase current contact even on a route handoff cycle.

Branch fix/sign-entry-direction starts at canonical main e75a452. Steady
speed and RGB-off patches are retained as 3eec683 and c365f00. Main's current
mode-1 gyro/bypass implementation remains unchanged; this branch is not a
whole-image replacement for the comprehensive V15 deployed candidate.

SimpleLine_StepRoute is the single route-to-line adapter:
- Seed each PROBE only once (including a direction first confirmed later).
- Seed expected arc curvature only upon entering ARC. Current contact always
  runs after these seeds and can replace them. No hint writes after calculation.
- All visible line masks use the steady forward differential profile.
- SimpleLine_UpdateYaw supplies fresh MPU yaw and generation before every step.
  On loss, search reverses at +/-25 degrees about the latest visible-line
  heading. It never grows its window by time or advances its origin on white.
  The direction memory is no longer forced by an unchanged PROBE label.
- Stale/unready yaw with white withdraws rotation (zero target, SEARCH state).
  Fresh yaw or a visible line resumes control; operator STOP cannot be resumed
  by the helper. A changed IMU generation invalidates the previous anchor.

25 degrees is an initial recovery-sector choice, not a ground-calibrated
overshoot guarantee. Gyro error, inertia, false contacts and sensor placement
can still defeat it. Motor reversal follows the existing DriveBase ramp.
This is intentionally different from indefinite same-side search: a smooth
track does not justify letting line loss become a full U-turn.

V15 integration (required before flashing a comprehensive image):
1. Port simple_line_mode.[ch] increment, retaining the existing slow profile.
2. Keep the target's MpuYaw_Refresh/read and SignRoute_UpdateYaw path. Add
   SimpleLine_UpdateYaw with readiness and generation from that same reading.
3. After SignRoute_Step/GetStatus, call SimpleLine_StepRoute once. Remove ALL
   old SimpleLine_SetDirection blocks and direct StepSlow/StepArc calls from
   sign_line_task. Keep route motor arbitration, trace, horn and RGB off.
4. Exclude sign modes from service_bounded_line_wait forced-rotation recovery.
   This is already true in this main-based branch. The deployed V15 enables
   that guard in all non-STOP modes; leaving it enabled would bypass the new
   yaw-invalid hold and erase navigation after 800 ms. Do not change mode-1
   recovery while making this exclusion.

Validation: reproduced the old wrong direction, tested both sides, repeated
loss after opposite curve evidence, same-cycle capture precedence, late sign
confirmation, STOP, 1000 simulated gyro search steps inside the sector,
invalid-yaw withdrawal and generation reset. Existing sign/ring and mode-5
tests passed. Production sign-cycle integration assertions reject residual
hint setters. ARM build text/data/bss 102848/64/12064.

No serial, hardware operation, flash, main merge or shared-state edit. Next
ground test must check normal entry, controlled local loss, reacquisition and
both circle exits; software success does not prove physical U-turn elimination.
