# Single-sign recognition with horn event

Base: canonical main cc2e685, branch fix/sign-single-horn. This is an
integration increment; do not replace deployed V13 with this main-based BIN.
Preserve the comprehensive target's gyro, low-speed and route-recovery code.

K210/sign_mode34.py now selects exactly one highest-score valid candidate
among class 0 left, 1 right, 2 horn. Classes 3/4 (digits) remain unused by this
course and cannot suppress these actions. Equal-score classes use lower class
ID for deterministic selection. No opposing-arrow veto remains. Threshold 0.2,
model, orientation, UART1 IO8/IO6 115200 and $D protocol are unchanged.

Only the last UART-submitted candidate is drawn when BOOT enables boxes;
bottom TX:L/R/HORN score (or TX:NONE/WAIT) is always shown. TX is a local
transmit indication, not receipt acknowledgement. Highest confidence is not
proof of correctness; STM32 arrow multi-frame confirmation remains unchanged.

Mode 3/4 horn handling is independent from route direction/state. Three
consecutive class-2 frames with score >=20, delivery age <=350 ms and adjacent
frame gaps <=300 ms trigger BuzzerPhrase400_Start(5) once (five complete
phrases, nominally 7.65 seconds, nonblocking). Sequence gaps,
duplicates, low score and stale frames do not accumulate confirmation.
Continuously visible horn cannot restart the sound. Non-horn observations
spanning 800 ms with fresh continuous sequence rearm it; silence alone does
not. Mode changes/STOP reset the detector and existing STOP stops the phrase.
The existing nonblocking PG12 rhythm is used, not a DFPlayer file. Horn never
requests motor movement, a stop or a navigation direction. Existing safety
audio may interrupt it. OLED VIS displays H for received horn detections.

Integration requires both sides: K210 script as /sd/main.py and STM32
sign_horn.[ch], three main hooks (observe/start, boot reset, mode reset), and
OLED H label. No need to port unrelated main sections. The mode-5 K210/main.py
is separate and unchanged. This supersedes 1cc9e36 conflict selection; do not
reapply its overlap/margin veto after this increment. Optional STRACE from
576fdbc is independent and not required to sound the horn.

Verification passed: mirrored/highest/tie/invalid/digit selection, simulated
K210 loop sending horn with at most one drawn box and TX:HORN indication,
horn debounce/latch/rearm/stale/sequence/tick wrap/reset, existing sign/ring
and mode-5 regression. ARM build text/data/bss 102392/64/12048.
No hardware access, flashing, main merge or ground validation performed.
