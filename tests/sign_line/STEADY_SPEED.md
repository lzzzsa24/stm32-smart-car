# Slow and steady mode 3/4 line profile

User reports oscillation on ordinary black line and says this course does not
require speed. Base main 72b0208, branch fix/sign-steady-speed. Recorded board
source remains independent comprehensive V8 c767baa; this branch is not its
replacement deployment and does not import its gyro/recovery stack.

Changes confined to mode 3/4 motor adapter/profile:
- All forward/pivot targets retain the 1200 CPS proportional limit. A sign
  disappearing or the 1500-ms evidence hold expiring cannot raise the limit.
  Evidence is still collected for telemetry. Mode reset still clears the
  global DriveBase speed limit for other modes.
- A centre line uses 2300/2300 equivalent PWM. Inner corrections use mirrored
  2200/2300 instead of 2200/2600, reducing target steps; outer contacts use
  mirrored 2000/2300 forward arcs. The outside command never exceeds straight.
- Every nonzero raw line mask withdraws a stale spin immediately. All-white
  retains the original powered search, remembered side and no timeout STOP.
- Forward tracking and route pivots no longer claim extra line-turn load
  assistance. Counter-rotation search retains that assistance. Existing speed
  PI, acceleration ramp, voltage compensation and startup/stiction boost are
  retained; deleting these could prevent slow wheels from starting.

No physical wheel-speed/oscillation improvement is claimed from host tests.
Reduced outer differential can miss tighter bends; actual wheel friction and
sensor chatter require ground tuning. Low-speed PWM floors can limit how
closely the physical car follows a 1200-CPS target.

Integration into comprehensive V8: import this bounded increment, preserve its
MPU services, route stages, recovery and mode 1/2/5 code. In sign_line_task use
SimpleLine_StepSlow for BOTH its ordinary and ARC calculations (replace the
existing SimpleLine_StepArc branch as well), after its held-direction update.
Do not replace its main.c wholesale or flash this main-based artifact as V8.
K210 transmit-selection change 1cc9e36 is independent and not imported here.

Verification: inner-input alternation 1000 times, all visible masks, immediate
search reacquisition, fixed cap despite changing reasons, search and STOP;
real sign-route arbitration now uses the slow helper. Formal ARM build passed
(text/data/bss 90596/64/11728). No serial, flashing, main merge or state update.
