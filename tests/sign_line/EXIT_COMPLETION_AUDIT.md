# Mode 3: finish the route when the outgoing line is acquired

The user reports that the car has physically left the circle but OLED still
shows SEARCH R, later EXIT TURN R, and the old route pulls it off the ordinary
line. The user confirmed this is **mode 3**, not the new mode-4 gyro-tangent
experiment. The repair releases both the displayed direction and motor
ownership after confirmed exit; it does not just rename the display.

## Source and hardware evidence

The working branch started this investigation at b67ca7b. After the shared
state changed, the single repair was rebased onto the recorded deployed
composite **58c3744de08796edba355ef3becf8453819aa546**, retaining its newer mode 4
and mode 1. Canonical main at final checking was d124f76, with mode-5 rc.5
published; its modes 3/4 are older than this composite. This is a delta for
the composite, not a wholesale replacement of current main.

Before changing control code, all available onboard reports were read at
2026-09-09 21:13 through an exclusive COM11 handle. Commands were only `0`
(STOP), `j`, `f`, `g`; no reset, calibration, log clearing, motion or flashing.
The port was closed after 18,940 bytes were captured. Files:

`F:/myproject/jidian/validation/sign-exit-report-20260909/211325/`

- `serial.bin`, `serial.txt`: complete STRACE BEGIN/END (256 records), LFAULT
  BEGIN/END (0 records), LSEARCH BEGIN/END (16 records), IMU and drive status.
- `events.json`: commands, timing, port settings and closure.
- `ANALYSIS.md`: full evidence/angle audit, prepared before firmware edits.
- Raw serial SHA256:
  `CFC2911DE474D1AFA622A19CBC93F9A3AA360CEF51629AEE4894B032E681BC24`.

The navigation trace froze on an older failure: 282728–288353 ms since boot.
The latest search event is 899923 ms, over ten minutes later. Entry is outside
the 256-record window. These are not a complete record of the latest attempt.

The retained trace shows ARC R with continuous 1111 for at least 2.116 s,
then a transition into EXIT TURN at 171.867 degrees despite no narrow-line
evidence. It cancelled 621 ms later at +30.020 degrees in the new phase:
`-direction*yaw < -30000` fired, not the 6-second timeout. Motor requests and
approach-heading error were absent, so the reason for positive rotation and
the car's physical position cannot be recovered from this trace alone.

At reading, IMU was ready with F=0, age 24 ms, no pending FIFO/backlog/restarts;
drive was STOP with zero wheel outputs. ACC_WARN=1 is retained as a calibration
warning, not proof of an incorrect yaw scale. The configured +/-500 deg/s,
65.5 LSB/(deg/s) conversion and yaw units agree in the driver. No IMU driver,
encoder interface, motor polarity or speed-controller changes are included.

## Mode-3 control changes

- ARC completion can be recognized from a previously observed narrow-line
  arc peak of at least 150 degrees, followed by centered outgoing-line evidence
  and approach-heading error within +/-15 degrees for 30 ms. Completion goes
  directly to LOCK, clears L/R and the observation window, and withdraws route
  commands. The follower exits its ARC policy in that same control iteration.
- Reaching 170 degrees alone cannot start exit alignment. A current narrow
  line and at least 150 mm of encoder travel are required. White, all-black,
  both outer sensors and other wide/ambiguous patterns cannot trigger it.
  Search motion is still present in continuous yaw; this gate does not claim
  to subtract every search rotation from physical arc travel.
- In EXIT TURN, every visible line retains ordinary line steering. Only
  all-white input permits the qualified heading-alignment command.
- Once aligned, EXIT LINE gives up forced straight travel on the first real
  line contact, including a contact in the phase-transition sample. Subsequent
  loss cannot restore that blind straight command; ordinary search owns it.
  Centered evidence for 30 ms completes the route without an additional 60 mm.
- LOCK has direction zero and no route motor command. The existing 1.5-second
  cooldown plus fresh no-target frames for 0.8 seconds still prevents immediate
  retriggering by the old sign. CANCEL also releases navigation and keeps
  line tracking/search available. Manual STOP remains authoritative.

## Angle limits

Positive MPU yaw means left; sign direction is L=-1/R=+1. Approach heading is
captured in PROBE. Arc yaw is measured from the entry-turn apex, not global
zero; the phase origin is locked after opposite curvature plus travel.
Heading error for mode 3 is normalized to +/-180 degrees.

| Boundary | Mode-3 behavior in this repair |
|---|---|
| Early entry capture | Existing selected edge, both middle sensors, >=15 degrees and 30 ms retained |
| Entry limits | 60-degree fallback / 120-degree maximum retained; SELECT capture fallback 45 degrees retained |
| Search sector | Entry 110 degrees / acquired ARC +/-25 degrees retained; neither owns ordinary post-exit search |
| Arc-origin lock | Existing 20 degrees plus 150 mm now also requires current narrow-line evidence |
| Natural exit | Remember >=150-degree narrow-line peak, then centered line within +/-15 degrees of approach for 30 ms |
| Active exit trigger | Existing >=170 degrees and >=150 mm now also requires current narrow-line evidence for 30 ms |
| Heading alignment | Within +/-10 degrees, or crossed the target and within +/-15 degrees; no additional 45-degree exit turn |
| Divergence | All-white alignment cancels if absolute heading error grows >15 degrees beyond its best value |
| Exit bounds | Existing 6 seconds / absolute 135-degree phase rotation; replaces asymmetric reverse-30-degree rule in mode 3 |
| Other bounds | ARC 20 seconds / 3 m / 225 degrees; EXIT LINE 2 seconds / 250 mm retained |

Geometry remains an assumption requiring a ground test: the opposite circle
connections have approximately parallel approach and departure headings. Four
binary sensors and a gyro do not prove which physical branch has been reached.

## Display and reports

The former SEARCH text hid ARC/EXIT whenever the sensors were all-white.
Its suffix R was the latched **sign choice**, not the actual wheel direction.
The shared sign OLED now always shows the real phase and appends `S` when the
actual line action is search, for example `ARC R S`. EXIT TURN means heading
alignment; EXIT LINE means outgoing-line acquisition; LOCK means completed
navigation/retrigger inhibition; CANCEL means withdrawn navigation.

STRACE retains its original first eleven CSV columns and appends heading error,
peak arc yaw observed on a narrow line, and requested left/right CPS. It still
prints only while stopped. Entering mode 3 from STOP/another mode starts a new
trace, and a later newly armed/probed route unfreezes an older cancellation.
Dumping and STOP alone preserve the report. RAM records still disappear on
power loss/reset. Shared display/report improvements also apply to mode 4;
its driving profile and angle thresholds are unchanged.

## Verification and integration

Passed:

- `cmd /c tests\sign_line\run.cmd`: real route -> shared follower -> four-wheel
  DriveBase checks for mirrored natural/active exits, opposite post-exit bend,
  repeated loss and no delayed EXIT TURN, timer wrap and STOP; 2,000-sample
  profile parity, all 16 masks, two-second observation, horn, mode selection,
  unchanged K210/model identity, MPU/FIFO/bus and mode-4 tangent regressions.
- Expanded `test_gyro_route.c`: all white/wide/nonadjacent masks cannot trigger
  exit; alignment after 40 degrees no longer waits for an extra 45-degree gate.
- `python tests\line_recovery\check_mode12_integration.py` and
  `python tests\gyro_turn\check_integration.py`.
- Formal `powershell -NoProfile -ExecutionPolicy Bypass -File .\build_unified_motion.ps1`:
  ELF text/data/bss 111128/64/22448 bytes; BIN 111196 bytes.
- Shared main state checker and `git diff --check`.

Formal BIN SHA256:
`682EC10EAF177126609169CFE3FFA28258F967BDA0A9950774E1F0C62B51FC4C`

Formal HEX SHA256:
`5E46B4B72941851C8C9E1B1FE8D1551A0420D0369D6BCA677247CC5DF34136FF`

Mode-2 slow targets remain straight 1412 CPS, initial outer correction 0/2200
or mirrored, and search +/-2493 CPS through the existing encoder closed loop.
The two-second observation stop, horn behavior and RGB-off behavior are retained.
No source changes to the shared follower, DriveBase, K210 or mode-4 parameters.

Only cherry-pick this repair's resulting commit onto the current composite;
do not overwrite main or another worktree with this branch's entire artifacts.
Current main intentionally has a different mode-5 integration. This task does
not merge/push, update PROJECT_STATE, flash or claim a wheel/ground-test pass.
