# Mode 3: center the line before timed sign observation

Historical record: superseded by `MODE3_DIRECT_OBSERVATION.md`. Mode 3 no
longer turns to find the middle pair before its fixed two-second observation.

Base: `947794dec76d649c55c1d3588fefafd01c6772f5` (the recorded flashed composite).
Rollback: `rollback/mode3-before-observation-center-947794d`.

## Behavior

- Only mode 3 opts into the new observation line check. During a requested
  observation stop, four white sensors suspend the two-second timer and hold
  route progression. OLED shows `SEEK LINE` while moving.
- Search uses the existing recovery direction rules and encoder wheel targets.
  Single-middle or outer contact cannot finish this centering search.
- Both middle sensors seeing black stops the wheels immediately. After 30 ms
  of stable middle-pair contact (sample gaps at most 50 ms), start a full new
  two-second stationary observation (`OBSERVE`). Outer sensors may also be black.
- If the middle pair is lost during confirmation, resume search. If all four
  sensors become white during the new observation, repeat centering, including
  loss at the exact two-second deadline.
- Retain the confirmed sign. Camera frames cannot restart the search timer or
  bypass the mandatory new observation. Rebase pre-entry gyro/encoder geometry
  after centering so search rotation is not counted as an entry turn.
- Manual STOP, mode reset, stale-camera handling, the 26-percent stop gate and
  existing exit-line reacquisition remain authoritative. No new timeout STOP.

Mode 4 does not call this centering API. Other recovery callers keep their
existing capture policy. No K210 changes, PWM bypass or speed retuning.

## Validation

- `cmd /c tests\sign_line\run.cmd`: PASS, including mirrored actual follower /
  route / DriveBase tests, partial contacts, full timer restart, 70-degree
  search-yaw exclusion, manual STOP, timer wrap and prior exit reacquisition.
- `cmd /c tests\line_recovery\run.cmd`: PASS, existing direction, wheel-output,
  recovery and STOP regressions.
- `python tests\gyro_turn\check_integration.py`: PASS.
- `python tests\mode5_bypass\test_app_profile.py`: PASS.
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\build_unified_motion.ps1`:
  PASS; text/data/bss = 116820/64/23560 bytes; BIN = 116888 bytes.
- Canonical `tools\check_project_state.ps1`: PASS; unrelated untracked purchase
  guide preserved. `git diff --check`: PASS.

This is host regression and build evidence only. No serial access, flash,
lifted-wheel or ground validation was performed. Actual middle-pair overlap
depends on sensor spacing and line width and must be checked on the vehicle.

The composite advanced independently to `29d490b` during this work (mode 4
visible-line priority). Deliver this change as a commit; do not replace that
integration tree with this worker tree. Preserve the newer mode 4 changes when
integrating and rerun the full sign suite and formal build on the result.
