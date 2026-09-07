# Shared project state

This is the handoff record for humans, Codex tasks, and delegated models. It is
not a substitute for Git: the checker resolves the live branch and HEAD every
time. Only the integration coordinator updates this file after a merge, flash,
or physical test.

## Machine-readable snapshot

```text
state_schema_version: 1
state_updated_at: 2026-09-07
integration_branch: feature/line-reacquire-lock
repository_head_at_update: ee059a3
latest_code_commit: 6f7a0db
flashed_source_commit: 6f7a0db
flash_record_commit: ee059a3
deployed_tag: deployed/2026-09-07-continuous-bypass-turn
formal_bin_path: manual-build-unified-motion/exp7_unified_motion.bin
formal_hex_path: manual-build-unified-motion/exp7_unified_motion.hex
formal_bin_size_bytes: 73704
flashed_bin_sha256: 376482E0A8CA6D01D3C2DBC0A18A873C619145748136451C1B5E5887FDE467E7
flashed_hex_sha256: DCFD3B4863B73EE246E2A9E1A3605111E56F7F43260CC6A322F96B3E15F9F35C
ground_test_status: continuous_bypass_turn_flashed_lifted_and_ground_tests_pending
k210_status: VL1_files_retained_on_K210_but_not_redeployed_or_required_by_current_STM32_image
candidate_source_commit: 6f7a0db
candidate_bin_size_bytes: 73704
candidate_bin_sha256: 376482E0A8CA6D01D3C2DBC0A18A873C619145748136451C1B5E5887FDE467E7
candidate_hex_sha256: DCFD3B4863B73EE246E2A9E1A3605111E56F7F43260CC6A322F96B3E15F9F35C
user_reported_flash: tool_verified_current_integrated_candidate
```

`repository_head_at_update` is the source/history anchor present when this
snapshot was written. Documentation-only governance commits may be newer; the
checker requires the anchor to remain an ancestor and prints the live HEAD.

## Active project and deployed firmware

- Repository: `F:\myproject\jidian\project\test-exp7-unified-motion-v1`
- Integration branch: `feature/line-reacquire-lock`
- Latest integrated source is `6f7a0db` (`fix(line): drive bypass turns continuously and accept IR early finish`). It is the integration-branch cherry-pick of requested worker commit `f58eac619d122672474499c775ef95ca97077eb6`; the visual branch was not merged.
- The board currently runs that integrated source. Flash/readback record:
  `ee059a3` (`Record continuous bypass turn firmware flash`).
- The formal BIN above was rebuilt in the integration worktree, then written
  through the STM32 ROM bootloader on USB-SERIAL CH340K COM11 at 57600 baud.
  Selective erase covered 36 firmware pages, preserved the final calibration
  page, wrote and read back 73704 bytes with `VERIFY OK`, and completed
  `GO OK: 0x08000000`.
- Build products under `manual-build-*` are intentionally ignored by Git. A
  different computer must rebuild the named source commit rather than assume
  the artifact was transferred.
- Rollback tag `rollback/2026-09-07-before-continuous-bypass-turn` preserves the
  integration state immediately before this change. The prior bounded-wait
  deployment tag and the isolated VL1 test tag remain available.

## Retained isolated VL1 test assets (not currently flashed on STM32)

- K210 COM13 identified CanMV Yahboom 2.1.1, mounted `/sd` and detected GC2145.
  Exact `8ee2432` files `/sd/line_core.py` (5509 bytes),
  `/sd/line_config.py` (1380 bytes) and `/sd/main.py` (4220 bytes) were written
  dependency-first and read back byte-for-byte. Soft reboot then printed
  `VL1 ready` and produced changing checksummed `$L` frames.
- The previous K210 `/flash/main.py` and `/sd/main.py` are backed up under
  `F:\myproject\jidian\validation\k210-vl1-deploy-8ee2432\backup-20260906-171712`.
  Existing road-sign model files and photos were not removed.
- During the prior isolated test, STM32 passively reported
  `VL1 STOP USER seq=0 ... L=0 R=0 err=0` throughout five seconds. No VL1 frame
  reached that test image. This remains historical evidence for the isolated
  branch; it does not describe the currently flashed integrated controller.
- This standalone image intentionally omits four-sensor tracking, obstacle
  avoidance, encoders, OLED, figure eight, square and buzzer audio. KEY1/KEY2
  start only after two fresh trusted vision frames; KEY3, remote 0/3, serial
  0/3/s/S/space stop it.

## Integrated bounded automatic-wait behavior

- In KEY1/KEY2, a continuously stopped, braking or faulted DriveBase remains
  under its current controller for 800 ms. If still paused, the candidate logs
  the reason, releases bypass/position ownership, clears the drive fault and
  performs 1200 ms of four-wheel counter-rotation before stopping and retrying
  the normal controller.
- Recovery direction first avoids a currently detected infrared obstacle, then
  uses an unambiguous outer line sensor or existing recovery direction; without
  evidence it defaults left. It commands rotation only, not forward/reverse.
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
| KEY3 / `3` | one encoder-controlled figure eight, then stop | figure-eight controller |
| KEY4 / `4` | one encoder-controlled square, then stop | square controller |
| remote direction-pad centre (`0x05`) | play the preset buzzer phrase once without changing mode | non-blocking phrase player; safety warnings retain priority |

The infrared remote also supplies the virtual mode keys and a stop command.

## Integrated line-loss behavior

Latest user observations before this deployment (2026-09-05): KEY2 could emit
1-, 5- or 8-beep drive alarms, alternate very small rotations, or stop silently.
The user then explicitly requested continued searching and fault observation
instead of stopping the line mode.

The persistent recovery retained from `ec858dc` in current `6f7a0db` changes
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

- Commit `63dbfe6`, retained in current `6f7a0db`, keeps the requested four-wheel CPS targets and adds a
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

- Commit `b424189`, retained in current `6f7a0db`, fixes the shared DriveBase transition from continuous
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
  current `6f7a0db`.

## Confirmed hardware facts

- All four encoders are repaired and usable as AB quadrature inputs.
- M2 is mapped to PA15/PB3; do not restore the obsolete fallback.
- Wheel order is M1 left-front, M2 left-rear, M3 right-front, M4 right-rear.
- Motor direction compensation remains centralized in `Core/Src/motorPWM.c`.
- K210 was reinserted for the prior VL1 test and retains the verified `/sd`
  files. Its USB serial port was not enumerated during this STM32-only flash;
  the current integrated image does not depend on the VL1 program.
- OLED is the external J12 display and includes battery/status information.
- Encoder distance/angle is a wheel-motion estimate; ground yaw requires
  calibration because slip and battery/load change the result.

## Evidence ledger

| Evidence level | Current result | Scope |
|---|---|---|
| computer build/link | passed | integrated `6f7a0db`; ELF text/data/bss = 73636/64/10424 bytes; BIN is 73704 bytes; new `LineBypassTurn_*` symbols are present in the map |
| host regression | passed | complete `tests/line_recovery/run.cmd`, geometry self-test and diff checks pass; includes continuous left/right 15/45-degree turns, slow/stopped wheels, early IR finish, timeout and STOP ownership |
| STM32 flash/readback/GO | passed | CH340K COM11 at 57600 baud; 36-page selective erase; calibration page preserved; 73704-byte write/readback; `VERIFY OK`; `GO OK` |
| K210 deployment/runtime | retained, inactive | prior exact VL1 files remain on K210; K210 was not redeployed and its USB port was not present during this STM32 flash |
| wheels off ground | not performed | programmer success does not establish motor direction, four-wheel continuity or sensor transition behavior |
| ground driving | not performed | continuous bypass turn geometry, clearance and overshoot remain unverified |

## Open issue and next safe step

The requested `f58eac6` bypass-turn increment is integrated, built and
programmer-verified on STM32; K210 was not changed. The next safe check is a
lifted-wheel KEY1 test with remote STOP ready: confirm both left wheels and both
right wheels counter-rotate continuously during each bypass turn and all four
stop together. Ground testing must then check left/right clearance, stable IR
early finish and turn overshoot separately. None of those physical outcomes is
established by this flash record.

## Update protocol

After integrating code, flashing, or receiving a physical result, the
coordinator must update the applicable fields and evidence row. Keep old facts
in Git history instead of accumulating a long diary here. Never mark a physical
test passed from a successful compilation, programmer verification, OLED text,
or encoder counts alone.
