# Mode 3: reacquired line overrides an unfinished exit turn

2026-09-10. Base: comprehensive `98a4a8fecd0153977ff768bab3d3cf65958ecfdc`.
Rollback: `rollback/mode3-before-exit-recapture-98a4a8f`.

The previous fix released only aligned EXIT LINE. EXIT TURN still classified
every narrow contact as the original ring until the gyro reached alignment.
A real route -> follower -> DriveBase host regression reproduced the omission:
after white and then right-outer contact, a left exit with 40-degree remaining
heading error stayed in EXIT TURN and requested **0/2200 CPS**, overriding the
new right-side line instead of correcting toward it.

Mode 3 now records a foreground all-white reading specifically during EXIT
TURN. The first subsequent nonzero black pattern moves to passive EXIT LINE
before any forced-turn command is generated. The existing follower uses that
same current reading immediately, regardless of residual gyro heading error.
The event is reset at each new exit; earlier entry/arc losses cannot authorize
an exit handoff. No sampler, UART or ISR change is involved.

Wide/all-black reacquisition releases motor ownership but does not declare
route completion. Later stable middle contact still clears direction and
enters LOCK. A subsequent loss cannot return to the old gyro turn; normal
line recovery owns it. Continuous original ring contact without a white gap
retains the existing angle-qualified exit behavior, and the no-reacquisition
alignment/timeout paths remain available.

This is intentionally first-contact priority. A false white-then-black reading
can end forced alignment early; the code cannot prove that the new black is
the outgoing road rather than the ring. Physical route capture remains to be
verified. The user's latest observation is the symptom, not a measured trace.

Validation passed:

- Complete `cmd /c tests\sign_line\run.cmd`.
- Both directions and all 15 nonzero masks reacquire before angle alignment;
  actual outer-contact CPS correct toward the current side; broad contact is
  not completion; later loss cannot reclaim turn ownership; stable middle
  capture clears L/R; manual STOP and clock wrap pass.
- Existing continuous-ring, 55/65-degree exit, natural departure, aligned-exit
  tracking, observation-resume and shared slow-profile regressions pass.
- Unchanged mode-4 trajectory, mode1/2, gyro and mode5 integration checks pass.
- `git diff --check`, canonical state checker and formal ARM build pass.

ELF text/data/bss: **115928/64/23552**. BIN: **115996 bytes**.

BIN SHA256: `F7A8EED9F861F7E88129669B8D2F6F02951F58C5A4D49BA77DE6C7E8B6C6DE9F`

HEX SHA256: `1333C95B9ED36D49863946509660F824F4EFA00992DA8FF63493AF2D0B5A2F62`

Only the standard mode-3 exit logic changes. Encoder speed, recognition pause,
entry selection, K210, horn, RGB and other modes retain the comprehensive base.
No serial, flash, lifted-wheel/ground run, merge, push or shared-state edit was
performed by this worker. Integrate the delta while preserving the comprehensive
baseline and any newer independent changes.
