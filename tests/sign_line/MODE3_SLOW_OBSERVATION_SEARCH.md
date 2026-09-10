# Mode3 slower pre-observation search

Base: f399de7. The user reports overly aggressive left/right rotation while
finding a line before the two-second observation stop, and requires encoder
feedback and sufficient drive effort.

Only LineRecovery_StepCentering (capture style3, called by mode3 observation
search) selects LINE_TRACKING_MIDDLE_GUARD_CPS=1800 instead of the nominal
LINE_SEARCH_TARGET_CPS=2493. This reduces requested wheel speed about28 percent;
it is not a measured chassis yaw rate. Other recovery styles retain their targets.
There is no persistent speed-limit flag to leak into later driving modes.

The final +/-1800 targets pass unchanged through PrepareLineTurnAssist and
SetWheelCps, keeping the assist authorization matched to the speed request.
DriveBase still closes the loop on each wheel encoder and retains target ramps,
voltage compensation, calibrated continuous-output floors, startup boost,
bounded PI and turn-load assistance. No raw PWM command, gain or output-rail
change is introduced. Existing faulty-feedback degradation policy is unchanged.

The middle-sensor capture, immediate zero target, full2s observation, direction
memory, phase-relative heading rebase, subsequent entry/exit control and STOP
remain unchanged. This opt-in API has only one production caller, the mode3
observation branch in sign_line_follow.c.

Host validation uses the real route/follower/DriveBase with mocked encoders,
GPIO and battery. Both turn directions retain1800 requested/controlled CPS.
At8.4V, summed four-wheel PWM is8800 at target speed and12008 with600-CPS
feedback; at7.0V it is10144 at target speed and13805 with stationary encoders.
These values show feedback and voltage influence, not measured torque or
physical starting ability. Individual outputs remain nonzero and within3599.
Middle contact still stops all wheels in the same sample. Existing full sign
and line-recovery suites and formal ARM build are required for handoff.

No serial, flash, lifted-wheel or floor test in the worker task. Ground friction,
slip and the calibrated minimum PWM can limit how closely physical speed follows
the requested reduction; physical adequacy has not been claimed.
