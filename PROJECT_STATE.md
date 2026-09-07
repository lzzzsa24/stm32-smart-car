# Shared project state

This is the handoff record for humans, Codex tasks, and delegated models. It is
not a substitute for Git: the checker resolves the live branch and HEAD every
time. Only the integration coordinator updates this file after a merge, flash,
or physical test.

## Machine-readable snapshot

```text
state_schema_version: 1
state_updated_at: 2026-09-07
integration_branch: fix/mode34-recognition-slowdown
repository_head_at_update: e740644
latest_code_commit: e740644
flashed_source_commit: f4099cf
flash_record_commit: 1f70810
deployed_tag: deployed/2026-09-07-lower-straight-speed-f4099cf
formal_bin_path: manual-build-unified-motion/exp7_unified_motion.bin
formal_hex_path: manual-build-unified-motion/exp7_unified_motion.hex
formal_bin_size_bytes: 73640
flashed_bin_sha256: 0354BCF4B7FFEDC9126DDED3A55D2A6669DB7CDDF6F9F5ADD2C8024F686F6B9E
flashed_hex_sha256: C6CAFE4B6107FD917E5A0198CEFE78B9AFFF03EA48E154628D85DCCD756D672B
ground_test_status: not_tested_after_f4099cf_flash
k210_status: SIGN34_ff8cf2e_main_and_model_readback_verified_startup_passed_STM32_link_not_tested
candidate_source_commit: e740644
candidate_bin_size_bytes: 82500
candidate_bin_sha256: 55548D9E03322E3E74864A7B4993D9395BFAFD32DBE8D86DDEB970C985010BDD
candidate_hex_sha256: 57667A63BAB7DEE5ECC417D49608C5E9C1168658469120699D14F99B59E91090
user_reported_flash: tool_verified_STM32_flash_readback_and_GO_no_physical_test
k210_candidate_source_commit: e740644
k210_candidate_status: mode5_main_and_SIGN34_v2_split_host_tested_not_deployed
```

`repository_head_at_update` is the source/history anchor present when this
snapshot was written. Documentation-only governance commits may be newer; the
checker requires the anchor to remain an ancestor and prints the live HEAD.

## Integrated unflashed mode 5 candidate

Commit `e740644` integrates requested worker source `10c8567` onto the live
integration branch while preserving the later mode 3/4 ring-exit state machine,
recognition slowdown and optimized SIGN34 script. Remote or diagnostic serial
key `5` selects K210 curve visual-line mode. The STM32 accepts the v4
`$status,off,angle,bottom,obs,obs_bottom,obs_left,obs_right#` frames through the
existing interrupt-driven USART2 ring, uses offset and angle for bounded
differential steering, and stops after 150 ms without a fresh valid frame.
Visual obstacle detection stops mode 5; automatic visual bypass is not enabled.

The K210 source is intentionally split: `K210/main.py` is the mode 5 no-KPU
visual-line application, while `K210/sign_mode34.py` preserves the optimized
road-sign program for modes 3/4. CanMV still has only one automatic
`/sd/main.py`, so switching between those K210 applications requires deploying
the desired file; this merge does not implement live runtime switching.

Merged host suites passed for mode 3/4 sign routing and K210 v2 behavior, mode
5 parser/control, and the complete existing line-recovery/load/bypass stack.
Formal ARM build passed with text/data/bss 82432/64/11392 and BIN size 82500
bytes; BIN SHA-256 is
`55548D9E03322E3E74864A7B4993D9395BFAFD32DBE8D86DDEB970C985010BDD`.
Neither the STM32 candidate nor either rearranged K210 source was deployed in
this merge task. Rollback tag `rollback/2026-09-07-before-mode5-v4-merge`
points to `f4ae2a8`.

## Current STM32 deployed test image

At the user's explicit request, exact source `f4099cf` from independent branch
`feature/line-search-axle-balance` was rebuilt and flashed without merging it
into the integration branch. It retains `b5c1145`'s direct lost-line rotation
and `9a5d22d`'s fresh direction-hint handoff, while reducing enhanced line
tracking's straight target ceiling from about 5273 to 3815 CPS. Sharp-turn and
lost-line search effort remains 2493 CPS. The full line-recovery/load/bypass
host suite passed. Formal ARM build text/data/bss was 73572/64/10424 and BIN
size was 73640 bytes.

COM11 was enumerated as USB-SERIAL CH340K immediately before programming.
At 57600 baud, selective erase covered 36 firmware pages, preserved the final
calibration page, wrote and read back all 73640 bytes with `VERIFY OK`, then
completed `GO OK: 0x08000000`. Power-on remains STOP. No lifted-wheel or ground
test has been performed for this image.

This exact historical branch predates the mode 3/4 sign-line integration:
KEY3 is encoder figure-eight and KEY4 is encoder square. The separately
deployed K210 SIGN34 program may continue running, but this STM32 image does
not consume it. Rollback tag `rollback/2026-09-07-before-f4099cf-test` points
to the previous STM32 source `9a5d22d`.

## Preserved K210 sign optimization

`07f2b73` updated the K210 sign script and its tests/documentation, based on
the user's `F:/myproject/jidian/sign_detect v2.0(1).zip` reference. Default raw
display, debounced BOOT overlay toggle, bounded LCD/debug refresh, UART before
display, periodic/low-memory GC and clipped valid-frame centres are included.
Threshold 0.2, model, camera orientation, arrows-only routing and frame format
are unchanged. Full sign-line suite and simulated actual Python main-loop
tests passed. During the mode 5 merge this implementation moved from
`K210/main.py` to `K210/sign_mode34.py`; its behavior was not replaced by the
older worker copy. No hardware port was opened and the merged K210 files were
not deployed. They remain independent of the currently flashed `f4099cf`
STM32 image.
Details: `K210/V2_OPTIMIZATION.md`.

## Previous flashed ring-exit build

The user's newest observation is: mode 3 makes an in-place U-turn and is not
usable; mode 4 follows the ring but continues around it at the opposite exit.
Their drawing distinguishes following a circular track from spinning in place.
No contemporary flash readback or runtime trace accompanied that observation.

Source `39b9327` replaces mode 3's enhanced recovery with the same SL2 baseline
as mode 4 and excludes both sign modes from automatic-wait forced rotation.
SignRoute now probes through transverse marks, selects an entry branch with a
forward pivot, tracks ARC, selects the outward exit and clears the exit line.
Route selection/capture depends on sensor evidence and encoder travel bounds;
it no longer completes the whole ring at the first entry-line capture. A real
branch without a confirmed arrow waits stopped. Bounds or persistent loss
latch a navigation fault until STOP/mode reset. Existing slowdown is retained.

Sign and full line-recovery host suites passed. Formal ARM build passed with
text/data/bss 78760/64/11240 and BIN 78828 bytes. Source `39b9327` (the firmware
tree selected by requested documentation commit `ff8cf2e`) was previously flashed on
2026-09-07 through STM32 ROM bootloader COM11 at 57600 baud. Selective erase
covered 39 firmware pages and preserved the final calibration page; all 78828
bytes read back with `VERIFY OK`, followed by `GO OK: 0x08000000`. It has not
been tested lifted or driven on the floor. Entry/exit thresholds and the
wheel-based heading estimate need physical calibration; this does not assert
that either reported physical failure is already resolved on the board.
Details and test cases: `tests/sign_line/RING_EXIT_FIX.md`.
Rollback: `rollback/2026-09-07-before-ring-exit` -> `1d1280e`.

## Retained recognition-slowdown change

Source `6e7aa61` adds a 1200-CPS target ceiling in modes 3/4 upon all-four-black
sensor evidence or one valid nonempty recognition frame. Each source holds for
1500 ms after its last event. No-target frames do not renew it. Mode changes
and STOP clear it. The 1 ms sampler retains short all-black events separately
from the existing line history. All speed owners including enhanced recovery
use the same proportional limit; position/brake/fault ownership is preserved.

The slowdown revision passed the sign-line host suite, full line-recovery/load/bypass
suite, formal build (text/data/bss 76272/64/11160), and diff whitespace check.
It was included in the previously flashed `39b9327` source, but has not been run
lifted or ground-tested. Minimum PWM can
prevent physical speed from reaching the requested low target; this is not a
physical speed guarantee. Track-image findings, behavior and tuning limits:
`tests/sign_line/RECOGNITION_SLOWDOWN.md`.

## Historical baseline integration and deployment

This isolated fix worktree was based on integration HEAD `400f1d9` (not the
older remote `main` at `3bd3748`). The deployment record below belongs to that
historical baseline. Rollback tag:
`rollback/2026-09-07-before-mode34-slowdown`.

- Repository: `F:\myproject\jidian\project\test-exp7-unified-motion-v1`
- Integration branch: `feature/line-reacquire-lock`
- Latest integrated source is `dbf61e4`; its parent feature commit `31026c3`
  replaces modes 3/4 with sign-line tracking, and `dbf61e4` ensures the mode 3
  recovery direction cannot be influenced by infrared. It retains the
  `f58eac6` bypass fix and imports the
  K210 model/script assets prepared by requested commit `f991301`, while
  implementing both new STM32 modes on the live integration branch.
- The previous recorded board source was `dbf61e4`. Its final dual-device deployment record was:
  `6ec6f22` (`Record final mode 3 and 4 sign-line firmware flash`).
- That previous 75176-byte formal BIN was rebuilt in the integration worktree, then written
  through the STM32 ROM bootloader on USB-SERIAL CH340K COM11 at 57600 baud.
  Selective erase covered 37 firmware pages, preserved the final calibration
  page, wrote and read back 75176 bytes with `VERIFY OK`, and completed
  `GO OK: 0x08000000`.
- Build products under `manual-build-*` are intentionally ignored by Git. A
  different computer must rebuild the named source commit rather than assume
  the artifact was transferred.
- Rollback tag `rollback/2026-09-07-before-mode4-sign-line` preserves the prior
  integrated deployment. Earlier continuous-bypass, bounded-wait and isolated
  VL1 tags remain available.

## Current K210 sign-recognition deployment

- On 2026-09-07 COM13 identified CanMV Yahboom 2.1.1 with GC2145 and mounted TF
  card. The actual model remains at
  `/sd/KPU/road_sign_det/road_sign_det.kmodel`, not the path originally written
  in `f991301`.
- Device-side SHA-256 is
  `B472A5C45FBB2060CD794BEC7C972D9F58FB40D7DCA27DFE6545125B8E02B901`,
  exactly matching the 571432-byte source model, so this deployment did not
  rewrite the model.
- The `ff8cf2e` tree's `/sd/main.py` was written and read back byte-for-byte:
  4911 bytes, SHA-256
  `E6ADB2BD616F84E7247E39BD487F458F4B4E34DB79F96E41DFD84876E9C2ED7E`.
  Soft reboot reported `model load succeed`, `SIGN34 ready`, the real model
  path, threshold 0.20 and vflip/hmirror 0/0.
- At the start of this deployment, `/sd/main.py` was 2491 bytes with SHA-256
  `AE9722E3360FA29BBA4B6867678D31671B1927FF002F43B5FF81660CF05045A1`,
  which differed from the older shared-state record. That file and the
  unchanged 289-byte `/flash/main.py`, plus deployment metadata and startup
  log, are backed up at
  `F:\myproject\jidian\validation\mode34-ring-exit-k210\backup-20260907-111542`.
  Older VL1 files, models and captured images were not deleted.
- The K210-to-STM32 UART result has not yet been observed after this flash.
  K210 runtime output and STM32 parser host tests do not prove the physical
  IO8/TX -> PD6/RX path is delivering frames.

## Baseline modes 3 and 4 behavior (superseded by the candidate above)

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

## Integrated source mode map (`e740644`, unflashed)

| Input | Mode | Motor owner |
|---|---|---|
| power-on / stop command | STOP | stop latch |
| KEY1 / `1` | integrated black-line tracking plus infrared/ultrasonic bypass | line controller or bypass state machine |
| KEY2 / `2` | black-line tracking only; obstacle sensors do not take the motors | line controller |
| KEY3 / `3` | enhanced four-line tracking plus confirmed K210 left/right route selection | enhanced line controller or sign route selector |
| KEY4 / `4` | independent SL2 simplified four-line tracking plus the same sign selection | simple line controller or sign route selector |
| remote or serial `5` | K210 v4 whole-line curve following; no road-sign recognition | visual-line v4 controller; stale/invalid UART or visual obstacle commands stop |
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
- K210 is connected as COM13 and currently runs the `SIGN34` road-sign script.
  Board-to-board UART1 IO8/TX to STM32 USART2 PD6/RX still needs a stationary
  receive check; common ground remains required.
- OLED is the external J12 display and includes battery/status information.
- Encoder distance/angle is a wheel-motion estimate; ground yaw requires
  calibration because slip and battery/load change the result.

## Evidence ledger

| Evidence level | Current result | Scope |
|---|---|---|
| computer build/link | passed | merged candidate `e740644`; ELF text/data/bss = 82432/64/11392 bytes; BIN is 82500 bytes |
| host regression | passed | mode 3/4 ring/sign and K210 v2, mode 5 parser/control/binding, plus complete line-recovery/load/bypass suites pass |
| STM32 flash/readback/GO | passed | CH340K COM11 at 57600 baud; 36-page selective erase; calibration page preserved; final 73640-byte write/readback; `VERIFY OK`; `GO OK` |
| K210 deployment/runtime | passed, stationary only | COM13; existing 571432-byte model hash matched and was not rewritten; 4911-byte `/sd/main.py` read back; model load and `SIGN34 ready` with threshold 0.20/path confirmed |
| board-to-board UART | not performed | K210 inference output is visible on USB, but receipt by the new STM32 USART2 parser was not observed without starting a drive mode |
| wheels off ground | not performed | programmer success does not establish rolling search direction or STOP response |
| ground driving | not performed | reduced straight speed, corner direction retention and line reacquisition remain unverified |

## Current open issue and next safe step

The exact `f4099cf` test image remains on the STM32; merged candidate `e740644`
has not been flashed. The next integration step, only after explicit flash
authorization, is to deploy the STM32 candidate while preserving the calibration
page and separately choose either K210 mode 5 `main.py` or mode 3/4
`sign_mode34.py`. Mode 5 then needs a stationary UART freshness/STOP check,
followed by lifted-wheel steering and only then ground curve tracking. No
physical behavior is established by the merged build or host tests.

## Update protocol

After integrating code, flashing, or receiving a physical result, the
coordinator must update the applicable fields and evidence row. Keep old facts
in Git history instead of accumulating a long diary here. Never mark a physical
test passed from a successful compilation, programmer verification, OLED text,
or encoder counts alone.
