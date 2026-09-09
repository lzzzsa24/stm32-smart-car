# Mode 3/4 outer feedback and black-bar speed

Base: main 33896c5, followed by replay of the existing steady-speed,
RGB-off and entry-direction dependencies (through ec3247d).
Deployment diagnosed: 0987233, not the main image.

## Physical evidence

STOP-only COM11 read at 2026-09-09 11:43, 115200 baud, DTR/RTS false.
Sent 0, lowercase j (read, not clear), g. No start, reset or flash.
Raw evidence: F:/myproject/jidian/validation/sign-read-20260909/trace-114358.log.
All four STOP wheel targets/measured/PWM were zero. STRACE END received.
Current IMU READY (S=2), CAL=200, F=0, AGE=22 ms; historical RESTARTS=3.
The old trace does not record per-row IMU validity or wheel targets.

At t=74545..75364 mask=1 persists: phase yaw -50.631 to -76.647 degrees
includes initial rotation; the settled segment t=74932..75364 changes only
0.102 degrees while encoder travel rises 285 to 383 mm.
At t=75553..76006 mask=1 persists, travel 10 to 115 mm, yaw only 0.231 degrees.
These are encoder travel estimates, not independently measured ground distance.
At t=76269 SELECTING right starts; mask crosses 3,7,15,13,8,0.
At t=76727 phase yaw +30.713 degrees triggers CANCEL reason 2 (wrong sign).
This frozen 256-row record has no ARC or EXIT_SELECT state. It establishes
an entry failure, not the cause of a later exit failure on another run.

## Fix

Shared DriveBase_EquivalentCpsFromPwm floors nonzero requests <=2200 to
1412 CPS. Lowering an outer request below 2000 in that adapter would therefore
not strengthen correction. Only the sign forward adapter now maps relative
weights linearly into CPS; shared DriveBase and reverse search stay unchanged.
Outer/same-side pair requests 800/2300 -> 417/1200 CPS (mirrored right).
Inner correction remains gentle: 2200/2300 -> 1147/1200 CPS.
Wide/center travel remains 1200/1200; selected route pivot retains a zero side.
All-black observation applies 700 CPS peak for the existing 1500 ms hold.
DriveBase scales both sides proportionally; no sign timeout increases the
ordinary 1200 CPS cap. Counter-rotation search is not capped by the black hold.

## Verification and integration

Host sign suite and formal ARM build; no new motion or flash validation.
The final delta is limited to sign adapter, simple slow controller and tests.
On comprehensive 0987233, apply ONLY the final delta commit; prerequisites
are already deployed. Do not replace that comprehensive image with this
main-based worktree: its other comprehensive features differ from main.
No exit thresholds changed without a captured ARC/EXIT failure.
Next ground evidence needed: stop and read lowercase j immediately after a
failure without power loss. Do not clear the frozen record before saving it.
