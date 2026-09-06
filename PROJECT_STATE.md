# Shared project state

This is the handoff record for humans, Codex tasks, and delegated models. It is
not a substitute for Git: the checker resolves the live branch and HEAD every
time. Only the integration coordinator updates this file after a merge, flash,
or physical test.

## Machine-readable snapshot

```text
state_schema_version: 1
state_updated_at: 2026-09-06
integration_branch: feature/line-reacquire-lock
repository_head_at_update: 6e899fb
latest_code_commit: fedd7ad
flashed_source_commit: fedd7ad
flash_record_commit: 6e899fb
deployed_tag: deployed/2026-09-06-single-edge-direction-memory
formal_bin_path: manual-build-unified-motion/exp7_unified_motion.bin
formal_hex_path: manual-build-unified-motion/exp7_unified_motion.hex
formal_bin_size_bytes: 68012
flashed_bin_sha256: 8482E1EE16858A8A9AAABF9523E551C912D285E5227C5165950251240A65D7A2
flashed_hex_sha256: 0484680ECA1832EB0DB8385F0B045154CDE12FCB83A853BDED40C117896D3EDA
ground_test_status: single_edge_direction_memory_flashed_ground_test_pending_buzzer_passed
k210_status: removed
candidate_source_commit: fedd7ad
candidate_bin_size_bytes: 68012
candidate_bin_sha256: 8482E1EE16858A8A9AAABF9523E551C912D285E5227C5165950251240A65D7A2
candidate_hex_sha256: 0484680ECA1832EB0DB8385F0B045154CDE12FCB83A853BDED40C117896D3EDA
user_reported_flash: tool_verified_current_candidate
```

`repository_head_at_update` is the source/history anchor present when this
snapshot was written. Documentation-only governance commits may be newer; the
checker requires the anchor to remain an ancestor and prints the live HEAD.

## Active project and deployed firmware

- Repository: `F:\myproject\jidian\project\test-exp7-unified-motion-v1`
- Integration branch: `feature/line-reacquire-lock`
- Latest firmware source commit: `fedd7ad` (`fix(line): remember single sampled outer edge for loss search`), now flashed. It integrates the requested `1772dac` on top of the current transverse-priority/persistent-search history, retaining corner continuity, stall-effort assistance, the position handoff fix, buzzer GPIO fix and IR centre-key audio.
- Flash/readback record: `6e899fb` (`Record single-edge direction-memory firmware flash`).
- The formal BIN above was rebuilt from the clean integration checkout, then
  written through the STM32 ROM bootloader on USB-SERIAL CH340K COM11 at
  57600 baud. Selective erase covered 34 firmware pages, preserved the final
  calibration page, wrote and read back 68012 bytes with `VERIFY OK`, and
  completed `GO OK: 0x08000000`.
- Build products under `manual-build-*` are intentionally ignored by Git. A
  different computer must rebuild the named source commit rather than assume
  the artifact was transferred.
- Current formal BIN/HEX were built in this integration checkout from
  `fedd7ad`. The preceding adaptive-search build remains under
  `manual-build-adaptive-line-search`.
  Previous isolated candidates remain under the validation directory.
- Immediate rollback tag: `rollback/2026-09-06-before-single-edge-direction-memory`
  points to `be3e656`. The buzzer GPIO fix and earlier adaptive-search rollback
  points remain available.

## Current mode map

| Input | Mode | Motor owner |
|---|---|---|
| power-on / stop command | STOP | stop latch |
| KEY1 / `1` | integrated black-line tracking plus infrared/ultrasonic bypass | line controller or bypass state machine |
| KEY2 / `2` | black-line tracking only; obstacle sensors do not take the motors | line controller |
| KEY3 / `3` | one encoder-controlled figure eight, then stop | figure-eight controller |
| KEY4 / `4` | one encoder-controlled square, then stop | square controller |
| remote direction-pad centre (`0x05`) | play the preset buzzer phrase once without changing mode | non-blocking phrase player; safety warnings retain priority |

The infrared remote also supplies the virtual mode keys and a stop command.

## Current line-loss behavior

Latest user observations before this deployment (2026-09-05): KEY2 could emit
1-, 5- or 8-beep drive alarms, alternate very small rotations, or stop silently.
The user then explicitly requested continued searching and fault observation
instead of stopping the line mode.

The persistent recovery retained in deployed `fedd7ad` changes KEY1/KEY2 as follows:

- On line loss it briefly brakes, then continuously rotates in the most recent
  reliable direction; without a hint it defaults left. Search uses equal and
  opposite 2493-CPS wheel groups. It no longer retreats, reverses search side,
  returns to encoder start counts, or stops for distance, attempt or time
  budgets.
- While searching it continuously repeats the existing 1.53-second preset
  buzzer phrase. A middle X1/X3 line hit, excluding the both-outer wide-line
  case, brakes and must confirm for 20 ms before the phrase is stopped.
- Confirmed capture enters at least 500 ms of low-speed line following and
  requires stable middle-line evidence for 80 ms before normal speed resumes.
  Losing the line during capture restarts same-direction rotation and audio.
- Outer-only and all-four-black input cannot directly complete capture. Manual
  STOP, mode change or zero base-speed still stop motion and cancel the phrase;
  power-on still enters STOP.
- During normal tracking or low-speed capture, a single outer sensor, or that
  outer sensor together with the adjacent middle sensor, locks the matching
  turn direction and enters continuous counter-rotation. Outer/white chatter
  then cannot restart braking or reverse the search direction.
- While that turn is locked, any outer sensor still seeing black keeps the
  counter-rotation active. Only when both outer sensors are white and at least
  one middle sensor remains black for 20 ms does braking and stationary capture
  begin. A failed 80 ms stationary confirmation resumes the same turn.
- The old 120/280 ms sharp-corner phases and weak forward arc are removed.
  Corner rotation uses the same 2493-CPS search target; ordinary shallow-curve
  steering and normal straight speed are unchanged. Online corner entry does
  not start the buzzer until the sensors become all-white.
- Three/four simultaneous black sensors or non-adjacent multi-black patterns
  now have transverse-line priority in normal tracking, locked turning, initial
  loss braking and low-speed capture. They cancel turning/audio and command
  low-speed straight travel at the 2200-equivalent target instead of inferring
  a left or right corner.
- For 100 ms after transverse evidence disappears, its trailing edge or
  all-white input continues low-speed forward travel. For 60 ms after valid
  middle-line evidence, a short all-white gap also continues low-speed travel;
  neither all-white condition refreshes its own window.
- A single outer edge must remain valid for 12 ms, with no sampling gap above
  30 ms, before continuous corner rotation is locked. This filters a transverse
  line whose first contact briefly appears on only one side.
- Direction memory is now separate from immediate turn locking. One
  unambiguous outer-edge or same-side adjacent pair sampled during normal or
  low-speed tracking immediately records that side, while the 12 ms turn
  debounce still applies. If the line becomes all-white after the short-gap
  windows expire, search follows this remembered side rather than defaulting
  left.
- A later opposite outer edge can replace the hint. Transverse/ambiguous input
  clears it and its following 100 ms tail ignores edge hints; stable centered
  input for 80 ms, 200 ms expiry and mode reset also clear it. A physical LED
  flash that falls entirely between software samples still cannot be recovered.
- During search, middle capture now needs at least two valid observations
  spanning 4 ms with gaps no greater than 30 ms. Confirmation immediately
  enters low-speed rejoin without inserting a stop. After at least 500 ms,
  recent middle evidence within 60 ms is sufficient to restore normal tracking.
- These digital-pattern rules cannot prove whether a physical mark is the main
  line or interference. The 4/12/60/100 ms values remain uncalibrated ground
  candidates and can delay a genuine corner immediately after a transverse line.
- Persistent rotation is intentionally unbounded at the recovery layer and can
  drift or heat a physically stalled motor. Encoder wheel speed does not prove
  chassis rotation or guarantee that the line will be found.

## Current line-turn load assistance

- Commit `63dbfe6`, retained in `fedd7ad`, keeps the requested four-wheel CPS targets and adds a
  bounded PWM supplement only to an accepted line-tracking differential or
  counter-rotation command. Straight travel, wide-line travel, stop, encoder
  position retrace/rollback and non-line modes do not receive this supplement.
- Each wheel is evaluated independently. Assistance begins only after its
  target ramp reaches at least 1412 CPS and measured same-direction speed stays
  below 85 percent of target for 40 ms.
- The extra PWM is half the same-direction speed deficit, capped at 600 and
  ramped upward by at most 5 PWM per millisecond. Total output remains clamped
  to the existing 3599 hardware limit; the existing startup pulse has priority.
- Reaching the 85-percent speed threshold, reverse feedback, a mismatched or
  expired authorization, stop or position control clears the supplement.
  Direction/signal-degraded wheels also lose PI and load assistance and use
  bounded feedforward. A no-motion observation alone now retains the bounded
  PI and turn supplement so high ground resistance does not reduce effort.
  Wheel-speed feedback is not current or torque feedback, so traction remains
  untested.

## Current line-drive fault observation

- Normal line tracking and persistent search enable a line-only observation
  policy. Existing no-motion, wrong-direction and illegal-encoder thresholds
  are retained, but reaching them records an event instead of setting the
  blocking DriveBase fault mask or commanding a fault stop.
- A wheel with wrong-direction or illegal-encoder observations uses the
  existing voltage-compensated CPS-to-PWM mapping, continuous-drive PWM floor
  and 3599 ceiling; its suspect feedback no longer adds PI, load assistance or
  repeated startup boost. Other wheels retain closed-loop control.
- A no-motion observation is still logged but no longer marks the wheel as
  feedback-degraded. Its prior bounded PI and turn assistance remain active;
  tests hold about 3529 PWM before and after the 1600 ms threshold in all four
  wheel positions and both search directions. This can keep a truly stalled or
  encoder-disconnected motor energized and is not torque or current control.
- Other modes and position actions retain blocking drive faults. STOP or mode
  change clears the active degraded-wheel state but does not erase the log.
- A 32-entry RAM ring stores event time, sensor/recovery state, battery value
  and four-wheel requested/controlled/measured speed, PWM, encoder delta,
  illegal transition count and zero-motion time. It survives repeated reads
  and operator STOP but is lost on reset, power loss, reflash or DriveBase
  reinitialization; it is not stored in the calibration Flash page.
- After a test, press remote `0`, keep power connected, open the application
  serial port at 115200 8N1 without resetting DTR/RTS, and send `f` or `F`.
  Preserve the final complete block from `LFAULT BEGIN` through `LFAULT END`.

## Current position-control handoff

- Commit `b424189`, retained in `fedd7ad`, fixes the shared DriveBase transition from continuous
  position-control PWM to the short-pulse region used near a target.
- When an individual wheel enters that low-speed region, its previous
  continuous PWM is first set to zero and a fresh stop-settle window is
  established. The existing 36 ms pulse gap and 25 ms per-wheel staggering are
  then retained instead of issuing a pulse while the wheel is still driven.
- Old pulse-response state is cancelled when switching between pulse and
  continuous control. Four-wheel distance targets, tolerances, synchronization,
  fault thresholds and fault latching are unchanged.
- This still affects other DriveBase position moves, but the current lost-line
  recovery no longer invokes position mode. Host tests reproduce both forward
  and reverse handoff; actual wheel inertia and alarm behavior remain untested.

## Current infrared-remote audio behavior

- Latest physical observation before this fix: pressing the intended sound
  button produced no audible result. Source inspection found that the active
  buzzer's PG12 output setup existed only in the unused legacy
  `MX_Experiment1_GPIO_Init()` path; the integrated startup calls
  `MX_GPIO_Init()` instead.
- Commit `0d31f10` initializes PG12 low as a push-pull output in
  `MX_GPIO_Init()` before the phrase player starts. It does not reconfigure
  the entity keys, change the NEC key map, or change motor/line behavior.
- The direction-pad centre/buzzer command `0x05` maps to a dedicated
  `IR_REMOTE_VIRTUAL_AUDIO_ONCE` event.
- One complete NEC frame starts exactly one 1.53-second preset five-attack
  phrase and does not start, stop, or change a driving mode.
- NEC repeat frames remain suppressed, so holding the key does not queue
  repeated playback. A later fresh press restarts one complete phrase.
- Existing stop/fault, encoder, ultrasonic and bypass warning arbitration can
  immediately cancel this lower-priority audio.
- The serial `b` command remains an equivalent one-shot diagnostic entry.
- After flashing `0d31f10`, the user short-pressed the intended sound button
  and explicitly confirmed audible output (`响了`). The same fix remains in
  deployed `fedd7ad`.

## Confirmed hardware facts

- All four encoders are repaired and usable as AB quadrature inputs.
- M2 is mapped to PA15/PB3; do not restore the obsolete fallback.
- Wheel order is M1 left-front, M2 left-rear, M3 right-front, M4 right-rear.
- Motor direction compensation remains centralized in `Core/Src/motorPWM.c`.
- K210 is physically removed for now.
- OLED is the external J12 display and includes battery/status information.
- Encoder distance/angle is a wheel-motion estimate; ground yaw requires
  calibration because slip and battery/load change the result.

## Evidence ledger

| Evidence level | Current result | Scope |
|---|---|---|
| computer build/link | passed | integrated formal `fedd7ad`; BIN is 68012 bytes; buzzer fix retained |
| host regression | passed | one-sample left/right outer hints select the matching later search direction in four configurations without immediately locking a turn; transverse tail, centered/expired/reset clearing, all 16 masks, 4-ms middle capture, persistent corner/search/audio, no-motion effort, fault fallback, STOP and RAM log tests pass; geometry self-test passes; MSVC /W4 /WX |
| flash/readback/GO | passed | CH340K COM11 at 57600 baud; 34-page selective erase; calibration page preserved; 68012-byte write and readback; `VERIFY OK`; `GO OK` |
| physical buzzer | passed | user explicitly confirmed `响了` after the PG12 initialization fix; fix retained in current firmware |
| wheels off ground | not performed this turn | diagnostic image compilation is not a lifted-wheel test |
| ground driving | current single-edge-direction-memory firmware untested | sampled hint direction, transverse classification, narrow capture, corner continuity and motor heating require controlled observation |

## Open issue and next safe step

The requested single-sample outer-edge direction-memory integration, formal
build, all host regressions, flash, readback verification and GO are complete.
Physical buzzer output was confirmed on an earlier firmware; the new ground
behavior is unverified. Test brief left/right edge contact followed by all-white
with the remote STOP ready, checking that search follows the last sampled edge
without a single sample itself causing an abrupt turn. Also recheck transverse
marks and narrow middle-line capture. After abnormal wheel behavior, press `0`
and keep power connected so the RAM log can be exported with `f`. Do not leave
a stalled motor energized. Use the immediate rollback point if unsafe.

## Update protocol

After integrating code, flashing, or receiving a physical result, the
coordinator must update the applicable fields and evidence row. Keep old facts
in Git history instead of accumulating a long diary here. Never mark a physical
test passed from a successful compilation, programmer verification, OLED text,
or encoder counts alone.
