# Recognition / ring-exit evidence handoff

User could not recall exit state and only tentatively recalled VIS:- 0. Do not
treat that as a captured failure trace or a confirmed exit root cause.
Canonical main was being edited by another task; this independent branch
starts from committed main c832f7b and leaves those edits untouched.
Recorded deployed source is V13 b326a6a; K210 remains the earlier 7256-byte
SIGN34 script, without 1cc9e36. No hardware access occurred here.

Includes 1cc9e36 replayed as 624e8b4. K210 label now distinguishes:
- TX:NONE CONFLICT: valid arrow candidates were rejected by conflict rules.
- TX:NONE NONARROW: only supported non-arrow detections survived validation.
- TX:NONE EMPTY: no valid candidate (including rejected invalid boxes).
- TX:L/R score: candidate submitted to UART, not an STM32 acknowledgement.
Model/threshold/serial protocol remain unchanged.

STM32 passive STRACE stores at most 256 records (~6 KiB). Normal sampling is
20 ms; state/fault changes are immediate. At regular sampling it spans about
5 seconds; frequent transitions shorten this. It records timestamp, sensor
mask, route state/direction/fault, phase yaw, wheel distance, received sequence,
online flag and last class/score. It never prints from Record or changes motors.
First CANCEL freezes history. Route reset and STOP do not erase it; power-off
does. It cannot recover short events between main-loop samples.

After a failed test, press remote 0 and keep power connected. Send lowercase
j at the existing diagnostic baud (115200), collect STRACE BEGIN through END.
Output is one record per main-loop call and only when the app is STOPPED.
Uppercase J clears/rearms the recorder only in STOP, for the next test.
Existing f fault log and v board-link diagnostics remain unchanged.

Integration: this is a bounded patch for the task assembling V13 or later.
Preserve the target gyro/low-speed/recovery implementation. Add the trace
header, init, serial j/J dispatch, STOP-only Task, and Record at the beginning
of sign_line_telemetry_task BEFORE its 500-ms early return. It consumes common
SignRouteStatus fields; YAW is MPU-derived only in the gyro-enabled target.
Do not replace comprehensive firmware with this main-based BIN. The matching
K210 source is K210/sign_mode34.py, installed as /sd/main.py upon authorization.

Validation: sign/ring regression, trace capacity/wrap/transition/freeze/STOP
output/clear tests, real K210 mocked-loop TX reason/display tests, and formal
ARM build passed (text/data/bss 91384/64/17880). No deployment, ground result,
main merge or shared-state update. Exit thresholds intentionally unchanged.
