# Main integration: continuous obstacle-bypass travel (`453cad2`)

Date: 2026-09-08 11:06 +08:00

## Integrated scope

Canonical local `main` now contains exact source commit
`453cad2cffcfbb4b8494b36cbdca8fc48c620476`, cherry-picked with provenance
from worker commit `68c6444f4c4fe63a852ba3a562992570a93d49d1`.

Only the latest obstacle-bypass change was promoted:

- add a four-wheel, encoder-bounded continuous short-travel controller;
- replace bypass forward/reverse endpoint position pulses with same-direction
  continuous travel capped at 1800 CPS;
- retain whole-car completion, STOP/mode ownership, all-wheel stall detection,
  progress timeout and infrared boundary interruption;
- retain the existing continuous bypass-turn controller and 2500-CPS turn
  command.

The separate experimental changes remain on
`test/comprehensive-v2-20260908` and were not integrated:

- `64f489e`: slower/silent line-loss and turn-assist experiment;
- `4c345b8`: ten-second sign-probe-hold experiment.

The integration changed only these six paths:

- `Core/Inc/line_bypass_travel.h`
- `Core/Src/line_bypass_travel.c`
- `Core/Src/line_obstacle_bypass.c`
- `tests/line_recovery/BYPASS_CONTINUOUS_TRAVEL.md`
- `tests/line_recovery/run.cmd`
- `tests/line_recovery/test_line_turn_load.c`

## Verification

- The touched preimage files on local main matched the worker's stated parent
  before cherry-pick; integration completed without conflict.
- Complete line-recovery/load/bypass suite: passed at both configured line
  search speeds, including 20/40-mm forward/reverse continuous travel,
  four-wheel PWM, encoder wrap, STOP/ownership, stall/progress faults, close
  and invalid infrared priority, and both bypass directions.
- Sign-line suite: passed.
- Vision-line-v4 suite: passed.
- Formal ARM build: passed.
- ELF text/data/bss: `83824 / 64 / 11584` bytes.
- BIN size: `83892` bytes.
- BIN SHA-256:
  `5757D92BC4B2741AE23908A0D494B5DD36757281F36AA9CC60766F9E46FB5E63`.
- HEX SHA-256:
  `E5FE872642E0D49DD66271D2778FE624026DBA4EEF3E938F0B7D2762C648B8CE`.

Prepared artifacts are cached under
`manual-build-candidate-main-bypass-453cad2/`.

## Boundaries

- This operation updated local `main`; it did not push GitHub.
- This operation did not flash STM32. The board remains on comprehensive test
  source `2c2ed97` from the preceding deployment.
- K210 was not accessed.
- No lifted-wheel or ground-driving test was performed for the main-only
  integration artifact.
- Host regression and compilation do not establish physical bypass behavior.

Rollback source tag:
`rollback/2026-09-08-before-bypass-continuous-main` -> `c277955`
