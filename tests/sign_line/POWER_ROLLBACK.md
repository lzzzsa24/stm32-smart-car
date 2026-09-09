# Restore the encoder speed controller before the two power experiments

User request: power is excessive and the car rushes off the track; restore
the power from two revisions ago and use the established encoder interface.
The two sign-power revisions are `11a243f` and `2e9a38e`. Their predecessor's
power source is `96b8372`, identical to comprehensive `8b91f49` for DriveBase
and sign slowdown. Current canonical state was checked at `87f2026`; the
recorded board image is `9ba87c3`, including both experiments and independent
mode-1/IR-isolation changes.

## Restored behavior

- Remove the sign-specific low-speed flag, encoder-budget pulse/coast path,
  fixed 3000/3250/3350 powered output, and their API/call sites.
- Restore the original `DriveBase_SetSideCps` / `DriveBase_SetWheelCps` speed
  command chain. Normal valid-encoder operation uses the existing target ramp,
  per-period encoder error, PI correction, output limits and battery compensation.
  Existing startup assistance and degraded-feedback handling are unchanged.
- Restore search power to the pre-experiment behavior by removing the added
  500-CPS counter-rotation cap. Original forward slowdown caps remain.
- Retain the independent fix that removes an artificial reverse-direction hint
  when entering ARC. Route selection, gyro exit rules, recognition, pause,
  horn and RGB logic are not rolled back.

The withdrawn powered phase did bypass the original speed PI and return fixed
PWM. It still had outer encoder movement feedback controlling pulse/coast
timing, so it was not fully open loop; however, it was no longer the original
speed controller. Voltage feedforward alone cannot replace measured-speed
feedback. Both experiments and their obsolete synthetic-plant test executable
are removed from the active program/test suite; their notes are marked withdrawn.

## Verification

- `git diff --quiet 8b91f49 -- Core/Src/drive_base.c Core/Inc/drive_base.h Core/Src/sign_slowdown.c Core/Inc/sign_slowdown.h`
  passed: the complete power source files match the selected baseline.
- Route source and the independent ARC-direction fix are unchanged from
  `2e9a38e`. Integration checks require the normal CPS interface and reject
  the removed sign-specific output path.
- Full `tests/sign_line/run.cmd`, `tests/line_recovery/run.cmd` (both speeds),
  and `tests/gyro_turn/run.cmd` passed, including real DriveBase feedback,
  STOP/fault/position ownership and gyro/route regressions.
- Formal ARM build passed: text/data/bss = 107332/64/18320 bytes.
- Worker HEX SHA256:
  `267F5CB5C65D3C40ADE2F8AE63606A6EA54D169276A88D2C13FEFD28A5A0669F`.
- Canonical state checker and whitespace check passed. The rollback patch
  passed a read-only applicability check against comprehensive `9ba87c3`.

No serial access, flashing, movement or push occurred. This establishes source
restoration and host/build results, not physical speed or successful arc exits.
The original controller's minimum-output and PI limits are also restored;
this rollback does not claim exact target speed under every load/battery state.

## Completion packet

```text
role: sign-power rollback; local branch/commit handoff
start_commit: 2e9a38e4fc34bd2c932438e2659438cd94dfe203
result_commit: commit adding this note; git log -1 --format=%H -- tests/sign_line/POWER_ROLLBACK.md
files_changed: drive_base.[ch], sign_slowdown.[ch], main.c sign option calls;
 sign integration/slowdown tests and runner; remove experimental low-speed test;
 mark two experimental notes withdrawn; add POWER_ROLLBACK.md
verification_completed: exact baseline comparison, sign/line/gyro regressions,
 formal ARM build, state/diff checks, read-only composite patch check
not_verified: flashing, actual speed under battery/load changes, physical arc exit
risks_or_assumptions: restores original power behavior and its existing limits
integration_notes: cherry-pick this delta onto 9ba87c3 or its successor;
 retain independent mode-1 rectangle/IR changes; rebuild integrated firmware
```

Do not use the whole worker branch image as the comprehensive image: it lacks
independent latest mode-1/IR changes. The built HEX above is worker validation
output. No canonical state or other task's checkout was edited.
