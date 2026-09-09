# Two-second observation pause (modes 3/4 only)

Continuation of 4c81dce with main 5116ee4 documentation refreshed.
Recorded deployed composite is 37a04b0; K210 is still 0987233 threshold .20.

On a fresh valid left/right frame (score >=15), before branch selection
(IDLE/ARMED/PROBE/WAIT_SIGN), latch a 2000 ms observation window. Motor output
is zero through the sign adapter regardless of the route or lost-line command.
Keep serial reception, direction votes, sensors, yaw, UI and route service
running. No HAL_Delay and no change to manual STOP/mode ownership. OLED shows
ROUTE:OBSERVE with confirmed direction when available; A=6 is observation.

Frames cannot refresh the deadline. At expiry resume the current controller;
if no direction was confirmed, continue ordinary line tracking, not a guessed
turn. This is a software zero-output interval starting at first received frame,
not a measured two seconds at zero chassis speed. Existing IMU-invalid loss
behavior still applies after expiry.

Rearm only after 1500 ms of fresh consecutive no-target heartbeats (gaps <=350
ms) and after the previous pause expired. Silence, a class flip or duplicate
frames do not rearm. Mode reset cancels the pause. Horn/digit frames do not
trigger it. No new pause is started during SELECTING/ARC/EXIT/LOCK/CANCEL.

PROBE still requires physical junction evidence; recognition alone arms a
direction. The pause adds observation time and does not label a straight line
as an intersection. A missed junction still needs sensor/trace diagnosis.

Integration: apply only this delta onto comprehensive 37a04b0. Preserve its
single-best/horn program, gyro routing, trace and all other modes. Insert the
pause check ahead of BOTH route and SimpleLine motor outputs, without bypassing
its horn/trace/UI tail. Keep modes 3/4 excluded from the old automatic wait
recovery guard, otherwise that guard will override the intended 2-second stop.
Do not replace the comprehensive image with this main-based worktree.

Verification: sign host suite includes fixed deadline, uninterrupted votes,
no repeated stop, rearm, mode reset and tick wrap; integration checks output
priority. ARM build required. No hardware start/flash/physical result.
