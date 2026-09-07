# Shared project state

This is the handoff record for humans, Codex tasks, and delegated models. It is
not a substitute for Git: the checker resolves the live branch and HEAD every
time. Only the integration coordinator updates this file after a merge, flash,
or physical test.

## Machine-readable snapshot

```text
state_schema_version: 1
state_updated_at: 2026-09-07
integration_branch: feature/mode5-visual-line-v4
repository_head_at_update: ad31859
latest_code_commit: 27a05aa
flashed_source_commit: 27a05aa
flash_record_commit: ad31859
deployed_tag: deployed/2026-09-07-mode5-visual-line-v4
formal_bin_path: manual-build-unified-motion/exp7_unified_motion.bin
formal_hex_path: manual-build-unified-motion/exp7_unified_motion.hex
formal_bin_size_bytes: 79172
flashed_bin_sha256: 1F7D39C95EA321A26760012250CABDB3B930370933863D9BC61C5FBFCBB8729E
flashed_hex_sha256: B1EBE492CC64153DF2C3B5732B6ED8BA6617E095DD3BEBFAF2C7A0415C638C57
ground_test_status: mode5_visual_line_v4_deployed_stationary_uart_passed_wheels_ground_not_tested
k210_status: visual_line_v4_runtime_and_STM32_board_uart_verified_script_readback_not_performed
candidate_source_commit: 27a05aa
candidate_bin_size_bytes: 79172
candidate_bin_sha256: 1F7D39C95EA321A26760012250CABDB3B930370933863D9BC61C5FBFCBB8729E
candidate_hex_sha256: B1EBE492CC64153DF2C3B5732B6ED8BA6617E095DD3BEBFAF2C7A0415C638C57
user_reported_flash: tool_verified_STM32_mode5_merge_and_live_K210_v4_runtime
```

`repository_head_at_update` is the source/history anchor present when this
snapshot was written. Documentation-only governance commits may be newer; the
checker requires the anchor to remain an ancestor and prints the live HEAD.

## Active project and deployed firmware

- Repository: `F:\myproject\jidian\project\test-exp7-unified-motion-v1`
- Integration branch: `feature/mode5-visual-line-v4`
- Latest integrated source is `27a05aa`; it starts from the deployed mode 3/4
  integration at `400f1d9`, retains modes 1--4, and adds mode 5 K210 curve
  visual-line control plus a STOP-state `v` link diagnostic.
- The board currently runs `27a05aa`. Deployment evidence is recorded by
  `ad31859` and `MODE5_VISUAL_LINE_V4_DEPLOYMENT_20260907.md`.
- The formal BIN above was rebuilt in the integration worktree, then written
  through the STM32 ROM bootloader on USB-SERIAL CH340K COM11 at 57600 baud.
  Selective erase covered 39 firmware pages, preserved the final calibration
  page, wrote and independently read back 79172 bytes with an exact match, and completed
  `GO OK: 0x08000000`.
- Build products under `manual-build-*` are intentionally ignored by Git. A
  different computer must rebuild the named source commit rather than assume
  the artifact was transferred.
- Rollback tag `rollback/2026-09-07-before-mode5-visual-line-v4` preserves the
  previous mode 3/4 integration. Earlier rollback and deployed tags remain available.

## Current K210 visual-line deployment

- COM14 identified CanMV Yahboom 2.1.1 with GC2145. Runtime output shows the
  v4 curve visual-line script producing `s/off/ang/bot/obs` at about 8--9 FPS.
- The active mode 5 script uses RGB565/QVGA and sends the strict eight-field
  `$status,off,angle,bottom,obs,obs_bottom,obs_left,obs_right#` frame every
  50 ms. It does not load KPU or perform sign recognition.
- STM32 STOP-state diagnostics observed valid v4 counts increasing from 77 to
  229 and later 241 to 276. Frame ages stayed at or below 90 ms in the first
  check and 12/66/50 ms in the final samples. `BAD` remained constant within
  each observation window and `SIGN=0`.
- This verifies the live IO8/UART1_TX -> PD6/USART2_RX path while the car is
  stopped. The exact active `/sd/main.py` bytes were not read back in this run.
- The former mode 3/4 sign script remains in source as `K210/sign_mode34.py`;
  it is not active in or consumed by mode 5.

## Integrated modes 3 and 4 sign-line behavior

- KEY3 / remote or serial `3` selects the existing enhanced four-sensor line
  controller plus sign routing. Remote or serial `4` selects the exact SL2
  table-driven algorithm from `feature/simple-four-line`, adapted only from
  raw PWM output to the current encoder-aware DriveBase.
- SL2 mode keeps the `X2 X1 X3 X4` sensor order, two-equal-sample filter,
  2400/2200/2600/2700 logical PWM table, last-direction memory and unbounded
  same-direction in-place search on all white. Unknown direction defaults left.
- Both modes receive strict `$D,class,score,cx,cy#` frames through an USART2
  RXNE interrupt ring. A left/right route needs at least three agreeing votes
  among five recent frames, the newest two agreeing, score at least 20 and no
  adjacent centre jump above 60 pixels.
- A confirmed sign arms a direction but does not leave the line immediately.
  A stable junction/wide pattern for 20 ms starts a 2700-equivalent-PWM
  in-place selection turn. After leaving the junction and seeing a middle-line
  pattern for 20 ms, the selected controller resumes normal tracking.
- A selected sign remains locked through the turn. Rearming needs at least
  1500 ms after capture plus actual no-target frames spanning 800 ms. Vision
  silence, malformed frames, horn/one/two and missing K210 never stop or steer
  the car; without a confirmed left/right sign both modes continue tracking.
- Infrared and ultrasonic readings do not own motors in modes 3/4. The figure-8
  and square source modules remain for history/reuse but have no key binding.
  Power-on and remote/serial `0` remain STOP.

## Integrated mode 5 K210 visual-line behavior

- Remote number `5` or serial `5` selects mode 5. It consumes only the v4
  eight-field visual-line frame; it does not call the sign-route state machine.
- STM32 uses filtered horizontal offset for differential steering, line angle
  for curve-speed selection, and bottom-centre position for near-horizontal
  sharp-turn direction. DriveBase remains the only motor-output owner.
- A stale UART frame older than 150 ms stops the car. Line loss keeps the last
  known turn for at most 400 ms and then stops. A reported visual obstacle with
  lower edge at y >= 150 stops the car; uncalibrated automatic bypass is not enabled.
- In STOP, serial `v` prints `VLINK` counts and the last v4 frame without
  changing mode or requesting motor motion. Remote/serial `0` remains the
  operator STOP command.

## Integrated bounded automatic-wait behavior

- In KEY1/KEY2 and KEY3 enhanced tracking, a continuously stopped, braking or
  faulted DriveBase remains under its current controller for 800 ms. If still
  paused, the candidate logs
  the reason, releases bypass/position ownership, clears the drive fault and
  performs 1200 ms of four-wheel counter-rotation before stopping and retrying
  the normal controller.
- In KEY1 only, recovery direction first avoids a currently detected infrared
  obstacle. All enhanced line modes then use an unambiguous outer line sensor
  or existing recovery direction; without evidence they default left. Recovery
  commands rotation only, not forward/reverse.
- Operator STOP, mode changes and power-on STOP reset the guard and never start
  timed recovery. The recovery deliberately overrides unresolved automatic
  ultrasonic, bypass or drive stops for its fixed window, so ground testing
  requires the remote `0` immediately available.
- `LSEARCH` source 5 records automatic wait recovery with pause reason and the
  drive/bypass fault masks. This code has passed host tests, formal build and
  programmer readback; its physical motion remains unverified.

## Integrated continuous KEY1 bypass turning

- KEY1 bypass turn segments now use `line_bypass_turn` instead of the shared
  endpoint-position turn controller. Both wheels on one side request `-1800`
  CPS while both wheels on the other side request `+1800` CPS (signs reverse
  for the opposite direction), using the existing DriveBase 20 ms speed loop,
  acceleration ramp, battery compensation and bounded slow-wheel assistance.
- A segment drives all four wheels continuously until average encoder travel
  reaches the requested angle and every wheel reaches at least half the target;
  then the whole car brakes together and accumulates at least 120 ms of settling
  travel. It does not perform per-wheel tail pulses or reversal correction.
- A stable side-infrared boundary may end the turn early, including before a
  nonzero encoder angle is accumulated. Encoder/DriveBase failures and bounded
  action timeouts remain faults; STOP or a mode change cancels the turn owner.
- Encoder-derived angle bounds wheel travel but is not a direct chassis-yaw
  measurement. Continuous-turn direction, obstacle clearance and overshoot are
  still pending lifted-wheel and ground validation at the current battery/load.

## Integrated mode map

| Input | Mode | Motor owner |
|---|---|---|
| power-on / stop command | STOP | stop latch |
| KEY1 / `1` | integrated black-line tracking plus infrared/ultrasonic bypass | line controller or bypass state machine |
| KEY2 / `2` | black-line tracking only; obstacle sensors do not take the motors | line controller |
| KEY3 / `3` | enhanced four-line tracking plus confirmed K210 left/right route selection | enhanced line controller or sign route selector |
| KEY4 / `4` | independent SL2 simplified four-line tracking plus the same sign selection | simple line controller or sign route selector |
| remote / serial `5` | K210 v4 curve visual-line tracking; no sign recognition | visual-line v4 controller |
| remote direction-pad centre (`0x05`) | play the preset buzzer phrase once without changing mode | non-blocking phrase player; safety warnings retain priority |

The infrared remote also supplies the virtual mode keys and a stop command.

## Integrated line-loss behavior

Latest user observations before this deployment (2026-09-05): KEY2 could emit
1-, 5- or 8-beep drive alarms, alternate very small rotations, or stop silently.
The user then explicitly requested continued searching and fault observation
instead of stopping the line mode.

The persistent recovery retained from `ec858dc` in current `dbf61e4` changes
KEY1/KEY2 as follows:

- On line loss it briefly brakes, then continuously rotates in the most recent
  reliable direction; without a hint it defaults left. Search uses equal and
  opposite 2493-CPS wheel groups. It no longer retreats, reverses search side,
  returns to encoder start counts, or stops for distance, attempt or time
  budgets.
- While searching it continuously repeats the existing 1.53-second preset
  buzzer phrase. A middle X1/X3 line hit, excluding the both-outer wide-line
  case, must confirm for 4 ms before the phrase is stopped.
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
- While persistent recovery is already active, an unconfirmed middle-line hit
  opens one fresh 200 ms exit-direction window. The next unambiguous single
  outer edge followed by all-white updates the active spin direction. Consuming
  that update closes the window, so later outer-only chatter cannot repeatedly
  reverse the car; another correction requires a new middle hit.
- A corrected recovery direction is handed into low-speed rejoin after capture,
  and the older pre-recovery hint is cleared. Direction reversal continues
  through the existing DriveBase ramp and adds no stop/brake timing cycle.
- After tracking GPIO initialization, the HAL 1 ms tick records all four sensor
  inputs into a 256-entry static queue. The interrupt path only reads GPIO and
  records timestamped masks; it never drives motors, plays audio, prints or
  waits. This preserves short sensor hits while OLED or serial work delays the
  main loop.
- Before each control calculation, the main loop consumes recent queued samples
  in order. History can update direction, middle-line recency and transverse
  filtering, but it cannot replay old motor commands or falsely complete line
  capture. Samples older than 200 ms are ignored; queue overwrite clears stale
  normal-mode direction candidates and retains the newest bounded history.
- Each control iteration now freezes its queue-consumption time boundary before
  draining. A sensor sample arriving from the tick ISR immediately after a pop
  remains queued for the next iteration, and the observation watermark cannot
  advance past that unconsumed evidence. This closes the previous queue-handoff
  race without raising the sampling frequency or widening the IRQ critical
  section.
- Wide/transverse input still clears older direction hints and keeps the car in
  low-speed straight travel during the 100 ms crossing tail. A later
  unambiguous single-side outer edge inside that window is now retained as the
  exit hint but cannot immediately command a spin; only continued all-white
  after the protection window starts search in that direction. A newer wide
  mark, centered-line clearing or the existing 200 ms expiry still invalidates
  the hint.
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
- The 1 ms sampler cannot guarantee capture of pulses shorter than one tick,
  and long interrupt masking can still delay sampling. It preserves sensor
  evidence but cannot remove the main-loop delay before a motor response.

## Integrated line-search decision log

- A separate 16-entry RAM ring records every initial search, confirmed corner
  entry and ACTIVE direction correction. Each record includes chosen side,
  source, current hint, recent edge/wide masks and ages, plus sensor-queue
  overwrite count. It records during motion without serial output.
- Search records survive STOP and mode reset, but reset, power loss, reflash or
  `LineFaultLog_Init()` clears them. Full rings overwrite the oldest entries.
- After pressing remote `0`, keep power connected and send `f` or `F` at
  115200 8N1. The existing `LFAULT` block is followed by `LSEARCH BEGIN`; keep
  receiving until `LSEARCH END`. Source 0 means default direction, 1 hint,
  2 rejoin, 3 corner and 4 ACTIVE correction.

## Integrated line-turn load assistance

- Commit `63dbfe6`, retained in current `dbf61e4`, keeps the requested four-wheel CPS targets and adds a
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
- KEY1 and KEY2 now use the same final line-command application path. KEY1
  first applies its existing ultrasonic forward-speed cap proportionally to
  both sides, then rebinds bounded turn assistance to those final CPS targets;
  KEY2 supplies the full PWM-period limit. This avoids DriveBase rejecting a
  pre-cap assistance claim whose targets no longer match.
- The ultrasonic speed cap, left/right target ratio, PWM hardware ceiling and
  assistance ceiling are unchanged. A zero KEY1 forward cap remains an
  explicit coast stop; `valid=0` recovery retains motor ownership, and active
  braking, position control and drive faults still take priority.

## Integrated line-drive fault observation

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

## Integrated position-control handoff

- Commit `b424189`, retained in current `dbf61e4`, fixes the shared DriveBase transition from continuous
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

## Integrated infrared-remote audio behavior

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
  current `dbf61e4`.

## Confirmed hardware facts

- All four encoders are repaired and usable as AB quadrature inputs.
- M2 is mapped to PA15/PB3; do not restore the obsolete fallback.
- Wheel order is M1 left-front, M2 left-rear, M3 right-front, M4 right-rear.
- Motor direction compensation remains centralized in `Core/Src/motorPWM.c`.
- K210 is connected as COM14 and currently runs the v4 curve visual-line script.
  Board-to-board UART1 IO8/TX to STM32 USART2 PD6/RX passed a stationary
  receive check; common ground remains required.
- OLED is the external J12 display and includes battery/status information.
- Encoder distance/angle is a wheel-motion estimate; ground yaw requires
  calibration because slip and battery/load change the result.

## Evidence ledger

| Evidence level | Current result | Scope |
|---|---|---|
| computer build/link | passed | integrated `27a05aa`; ELF text/data/bss = 79104/64/11280 bytes; BIN is 79172 bytes; both sign and v4 parsers plus the mode 5 controller are linked |
| host regression | passed | v4 strict parser/control states and preserved mode 3/4 sign parser/routing tests pass |
| STM32 flash/readback/GO | passed | CH340K COM11 at 57600 baud; 39-page selective erase; calibration page preserved; independent 79172-byte exact readback; `GO OK` |
| K210 deployment/runtime | passed, stationary only | COM14; v4 `s/off/ang/bot/obs` output observed at about 8--9 FPS; exact `/sd/main.py` readback not performed |
| board-to-board UART | passed, stationary only | STOP-state `VLINK` v4 count increased 77 -> 229 and 241 -> 276; final frame ages 12/66/50 ms; `BAD` stable and `SIGN=0` |
| wheels off ground | not performed | programmer and UART success do not establish mode 5 motor direction or STOP timing under moving wheels |
| ground driving | not performed | visual line following, curve direction, sharp turns and visual obstacle stop remain unverified |

## Open issue and next safe step

The modified merged firmware and stationary K210->STM32 v4 link are deployed.
The next safe step is a lifted-wheel mode 5 test with remote `0` ready: confirm
offset-left produces a left request, offset-right produces a right request,
UART loss stops within 150 ms, and prolonged line loss stops after the bounded
400 ms hold. Ground tests then need straight, curve, sharp-corner and visual
obstacle-stop runs. No physical driving outcome is established yet.

## Update protocol

After integrating code, flashing, or receiving a physical result, the
coordinator must update the applicable fields and evidence row. Keep old facts
in Git history instead of accumulating a long diary here. Never mark a physical
test passed from a successful compilation, programmer verification, OLED text,
or encoder counts alone.
