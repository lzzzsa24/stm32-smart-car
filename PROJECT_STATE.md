# Shared project state

This is the handoff record for humans, Codex tasks, and delegated models. It is
not a substitute for Git: the checker resolves the live branch and HEAD every
time. Only the integration coordinator updates this file after a merge, flash,
or physical test.

## Machine-readable snapshot

```text
state_schema_version: 1
state_updated_at: 2026-09-08
integration_branch: main
repository_head_at_update: 3eb6889
latest_code_commit: 36551f3
flashed_source_commit: 4947f9c
flash_record_commit: c3200d9
deployed_tag: deployed/2026-09-07-line-direction-evidence-4947f9c
formal_bin_path: manual-build-unified-motion/exp7_unified_motion.bin
formal_hex_path: manual-build-unified-motion/exp7_unified_motion.hex
formal_bin_size_bytes: 83332
flashed_bin_sha256: D17F7ED9926CB33150CFD0681613038AD8425E96272CF1FC94EB74446418E6E1
flashed_hex_sha256: 4ED1A0D75E674997F0764A2F42332E477410DA5591D4772BF829679B65E28C35
ground_test_status: not_tested_after_v1.2.0_rc1_host_build
k210_status: COM14_SIGN34_07f2b73_script_and_model_readback_verified_startup_passed_board_link_not_reverified
candidate_source_commit: 36551f3
candidate_bin_size_bytes: 84412
candidate_bin_sha256: 5C7F43ACCEE850E30F439121254E1FCA4085CFAAC11C6CE10423463070777CCB
candidate_hex_sha256: 2C65AB052DB5E55AEADC4ECADE21B766AE53483CE46F868108C6E21D8693815F
user_reported_flash: tool_verified_STM32_flash_readback_and_GO_no_physical_test
k210_candidate_source_commit: 07f2b73
k210_candidate_status: deployed_readback_verified_7256_bytes_model_verified_SIGN34_startup_passed
remote_sync_status: current_main_and_v1.2.0_rc1_published_via_required_PR
remote_sync_branch: main
stm32_runtime_status: COM11_4947f9c_readback_verified_GO_and_default_STOP_confirmed
k210_requested_deployment: SIGN34_07f2b73_complete
temporary_flash_selector_commit: 4947f9c
github_release_tag: v1.2.0-rc.1
github_release_source_commit: 3eb6889
github_release_firmware_commit: 36551f3
github_release_status: prerelease_published_host_verified_not_reflashed_or_ground_tested
```

`repository_head_at_update` is the source/history anchor present when this
snapshot was written. Documentation-only governance commits may be newer; the
checker requires the anchor to remain an ancestor and prints the live HEAD.

## Canonical repository layout

Since 2026-09-07, `main` is the only canonical integration and deployment
branch, checked out at
`F:/myproject/jidian/project/test-exp7-unified-motion-v1`. Future feature,
fix and test worktrees start from current `main` and return commits here for one
review/build/deployment pass. Existing worktrees and branches remain preserved
as history; they are not alternate definitions of “latest”. The exact workflow
and temporary historical-image exception are documented in
`BRANCH_WORKFLOW.md`.

GitHub PR #4 merged the all-mode integration into protected `main` as merge
commit `3eb6889`. The active ruleset still requires a pull request and protects
against deletion and non-fast-forward updates; its current required approving
review count is zero. No ruleset was bypassed or disabled for this merge.

Pre-release `v1.2.0-rc.1` points to `3eb6889` and publishes the STM32 BIN/HEX,
checksums, both mutually exclusive K210 `/sd/main.py` choices, the mode 3/4
road-sign model and instructions. It contains firmware code `36551f3`; it is
not the independently flashed `4947f9c` image and makes no new physical-test
claim. The previous `v1.1.0-main-20260907` release remains available as history.

## Current flashed temporary test image (`4947f9c`)

The user requested only to flash exact source `4947f9c`, not merge it. It is on
the independent `fix/line-direction-evidence` branch and retains canonical
source `2d1edaa`'s GPIO snapshot/ISR ordering fix. Its parent has the same code
tree as `2d1edaa`; the one source commit adds the newer line-direction evidence
behavior without importing the independent mode 3/4 sign-fork source
`d1d22d9`.

Adjacent three-probe overlaps can now retain a left/right direction hint while
the crossing guard still commands forward travel. A recent real side hint may
survive a short broad mark for at most 400 ms, but repeated broad samples do not
renew its age. Centre or opposite-side evidence invalidates the old hint, and
queued white/outer evidence breaks an interrupted centre confirmation so a
stale sample cannot complete a false recovery handoff. The fault log records
the accepted hint mask and age for diagnosis.

The complete line-recovery/load/bypass suite passed at both 2493 and 1870 CPS,
including all 16 masks, 256 mirrored ordered pairs, crossing-tail direction,
queued-sample ordering, interrupted confirmation, STOP/fault ownership and
four-wheel turn load. The mode 3/4 sign/ring and mode 5 suites also passed. The
formal ARM build passed with text/data/bss 83264/64/11512 and produced an
83332-byte BIN with SHA-256
`D17F7ED9926CB33150CFD0681613038AD8425E96272CF1FC94EB74446418E6E1`;
the HEX SHA-256 is
`4ED1A0D75E674997F0764A2F42332E477410DA5591D4772BF829679B65E28C35`.

COM11 was enumerated as USB-SERIAL CH340K and a STOP command was sent before
deployment. An initial Windows PowerShell 5 invocation failed while parsing the
host script, before opening the programmer or erasing Flash. The PowerShell 7
run then entered the ROM bootloader, selectively erased 41 application pages,
preserved the calibration page, wrote and read back all 83332 bytes with
`VERIFY OK`, and completed `GO OK: 0x08000000`. The startup banner confirmed
`DEFAULT STOP`, followed by another explicit serial STOP command. This is not a
wheel-motion test. K210 was not rewritten.

Rollback tag `rollback/2026-09-07-before-4947f9c-test` points to the replaced
source `d1d22d9`. Deployment tag
`deployed/2026-09-07-line-direction-evidence-4947f9c` identifies the exact
board source. No lifted-wheel or ground test was performed by Codex.

## Canonical main source and current release (not currently flashed)

Functional source `36551f3`, merged by PR #4 as `3eb6889`, updates every active
mode from its latest accepted branch. KEY1/KEY2 include line commit `4947f9c`
plus the new lone-inner ambiguity fix `1b42805`. KEY3/KEY4 include the complete
latest sign-route stack through `36551f3`. KEY5's controller, protocol and K210
script are blob-identical to accepted mode-5 commit `5d761e4`.

A lone X1 or X3 detection is no longer stored as a certain curve direction.
If it is followed by loss, recovery starts toward that sensor as a probe and
uses all four repaired encoder transition counters for expanding 30/60/90/120
degree sweeps. Time and battery state do not advance those sweeps. Fresh outer
evidence remains authoritative, and a re-loss after lone-inner capture retains
the direction that actually found the contact.

Modes 3/4 now keep live line feedback authoritative, continue lost-line search
without a sign-recognition speed cap, retain a confirmed fork direction and
withdraw failed or ambiguous exit selection. Their K210 program and road-sign
model remain the previously verified `07f2b73` pair; they were packaged, not
rewritten to hardware, in this update.

The line-recovery/load/bypass suite passed at both 2493 and 1870 CPS, including
all 16 masks, 256 mirrored ordered pairs, queued-sample ordering, X1/X3 mirrored
encoder probes and real DriveBase four-wheel targets. The sign-line and visual
line v4 suites also passed. The formal ARM build passed with text/data/bss
84344/64/11536 and produced an 84412-byte BIN with SHA-256
`5C7F43ACCEE850E30F439121254E1FCA4085CFAAC11C6CE10423463070777CCB`;
HEX SHA-256 is
`2C65AB052DB5E55AEADC4ECADE21B766AE53483CE46F868108C6E21D8693815F`.
No serial port was opened and no STM32/K210 flash, lifted-wheel or ground test
was performed. The rollback tag
`rollback/2026-09-08-before-all-modes-update` points to `41e5ae6`.

## Previous flashed integrated source (`7ce4944`)

Source `7ce4944` integrates requested worker `5d761e4` and its required
STOP-state UART diagnostic commit `27a05aa` on top of the previously deployed
`f39513c` integrated source. Modes 1--4, their latest line-search corrections,
the reduced KEY1 ultrasonic profile, the mode 3/4 ring route and the optimized
SIGN34 source are retained.

Mode 5 now uses lower PWM commands: 2000 straight, 1700 curve, 1800 sharp turn
and 1800 lost-line search. Steering gain is 8 PWM/pixel with a 1000-PWM cap.
Lost line keeps searching in the predicted direction, and a return to normal
tracking requires two fresh valid frames. The matching K210 source restricts
line candidates to the near field y=80--239, requires them to extend to y>=170,
and gives lower image regions additional selection weight. UART freshness and
visual-obstacle stopping remain fail-safe constraints.

Mode 5, mode 3/4 sign/ring and the complete line-recovery/load/bypass host
suites passed. The formal ARM build passed with text/data/bss
82676/64/11384 and produced an 82744-byte BIN with SHA-256
`3317777CA6E6EDEBCEEFB03CC9E4E54F15C95379A48BA5CC2643C09F1BD8497E`.
The image was written through the enumerated CH340K COM11 at 57600 baud.
Selective erase covered 41 firmware pages, preserved the calibration page,
read back all 82744 bytes with `VERIFY OK`, and completed
`GO OK: 0x08000000`. No lifted-wheel or ground test was performed.

After the later stationary VLINK attempt toggled BOOT/RESET, a final repeat
deployment could no longer enter the ROM bootloader and failed before erase or
write. On the next retry both COM11 and COM14 had disappeared from Windows.
The verified Flash bytes remained unchanged. On the later explicit `07f2b73`
deployment request, COM11 returned and the same 82744-byte integrated image was
again selectively erased, written and read back with the calibration page
preserved, followed by `GO OK: 0x08000000`. Current application execution is
therefore confirmed at the programmer/GO evidence level.

After the user reconnected the K210, COM14 identified CanMV Yahboom 2.1.1 with
GC2145 and mounted TF storage. The previous 5353-byte `/sd/main.py` was backed
up, then the 8545-byte `K210/main.py` from `7ce4944` was written and read back
byte-for-byte with SHA-256
`380CE27C96B95D27652FEA53E03E74D9B1926421ABA80A388C1BD586D34E5711`.
Soft reboot initialized GC2145 and produced mode 5 `s/off/ang/bot/obs`
telemetry at about 8.2 FPS. `/flash/main.py`, the model directory and other TF
files were not changed. Backup and manifest are under
`F:/myproject/jidian/validation/mode5-slow-near-20260907/backup-20260907-145058`.

A STOP-state `VLINK` check was attempted after deployment but did not receive
the USART1 diagnostic reply, so the current K210-to-STM32 board link is not
claimed as reverified. The earlier `27a05aa` worker deployment did verify fresh
v4 frames on the same board-to-board wiring. Rollback tag
`rollback/2026-09-07-before-mode5-slow-near-merge` points to `e52f8f9`, whose
firmware tree is the previous `f39513c` deployment.

For the later explicit `07f2b73` request, Git confirmed that commit is already
an ancestor of canonical main and that its original `K210/main.py` blob exactly
matches current `K210/sign_mode34.py`. On COM14, the active 8545-byte mode 5
script was backed up, then the 7256-byte SIGN34 script was written and read
back byte-for-byte with SHA-256
`2BCFCC5E08671EE0F0E3BD0712A1DD217A3450BFDBD3C3DDA7EFE8807D38A3D8`.
The existing 571432-byte model at
`/sd/KPU/road_sign_det/road_sign_det.kmodel` matched SHA-256
`B472A5C45FBB2060CD794BEC7C972D9F58FB40D7DCA27DFE6545125B8E02B901`
and was not rewritten. Soft reboot reported model load success, `SIGN34 ready`,
threshold 0.20 and camera vflip/hmirror 0/0. Backup and deployment manifest are
under
`F:/myproject/jidian/validation/sign34-07f2b73-20260907/backup-20260907-152226`.
This makes modes 3/4 recognition available; mode 5 visual-line operation is no
longer active on K210 until its separate `K210/main.py` is redeployed.

## Previous flashed integrated source (`f39513c`)

Commit `f39513c` retains the mode 5 integration from `e740644` and adds the
five ordered line/avoid increments ending at requested worker `34223bd`.
Lost-line rotation no longer inserts a whole-car brake, fresh direction hints
survive capture handoffs, centred straight speed is reduced, and every fresh
outer-edge-to-white event can update an active search direction. KEY1's
ultrasonic profile is reduced to 10 cm stop, 18 cm clear, 16 cm dynamic
emergency maximum, 70 ms lookahead and 5 cm critical raw echo. Mode 3/4
ring-exit navigation, recognition slowdown and optimized SIGN34 are preserved.

Remote or diagnostic serial key `5` selects K210 curve visual-line mode. The STM32 accepts the v4
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
5 parser/control, latest-outer-direction replay, alternating corners, and the
complete line-recovery/load/bypass stack. Formal ARM build passed with
text/data/bss 82280/64/11392 and BIN size 82348 bytes; BIN SHA-256 is
`C7686DC0B41DF032BE1770470A5C9A957EB4A28189BC37435CAA0B69A96CDA60`.
The STM32 image was flashed on 2026-09-07 through CH340K COM11 at 57600 baud.
Selective erase covered 41 firmware pages and preserved the calibration page;
all 82348 bytes read back with `VERIFY OK`, followed by
`GO OK: 0x08000000`. Neither rearranged K210 source was deployed in this flash
task. Rollback tag `rollback/2026-09-07-before-latest-line-search-flash` points
to the previous board source `e740644`; source-integration rollback tag
`rollback/2026-09-07-before-latest-line-search-merge` points to `2d86521`.

## Previous STM32 deployed test image

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
older worker copy. It is now the active, readback-verified `/sd/main.py` on
K210 COM14, paired with the current integrated STM32 image.
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

## Historical K210 sign-recognition deployment

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

## Current STM32 mode map (`36551f3`)

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

The latest observation before this integration was that one repeatable approach
angle produced only X1 or X3 and could lock recovery in the wrong direction.
Current `36551f3` retains continuous recovery and adds an explicit ambiguity
path for that physical pattern. KEY1/KEY2 behave as follows:

- On ordinary line loss it rolls directly into continuous rotation in the most
  recent reliable direction; without a hint it defaults left. Search uses equal
  and opposite 2493-CPS wheel groups. It no longer retreats, returns to encoder
  start counts, or stops for distance, attempt or time budgets.
- A lone inner X1/X3 observation is not a reliable curve direction. If loss
  follows within 200 ms, it starts a probe toward that side, then reverses after
  four-wheel encoder travel corresponding to 30 degrees. Each new sweep grows
  by 30 degrees up to 120 degrees; later sweeps alternate at that bound. Time
  and battery level alone cannot reverse the probe.
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
- A later opposite outer edge can replace the hint. A short broad/transverse
  mark can retain a genuinely recent direction for at most 400 ms without
  refreshing its age; stable centered input, expiry, contradictory strong
  evidence and mode reset clear it. A physical LED flash that falls entirely
  between software samples still cannot be recovered.
- While persistent recovery is already active, an unconfirmed middle-line hit
  opens one fresh 200 ms exit-direction window. The next unambiguous single
  outer edge followed by all-white updates the active spin direction. Consuming
  that update closes the window, so later outer-only chatter cannot repeatedly
  reverse the car; another correction requires a new middle hit.
- A corrected recovery direction is handed into low-speed rejoin after capture,
  and the older pre-recovery hint is cleared. A lone-inner capture does not
  replace that direction; if it is lost during settle, search continues in the
  direction that physically found it. Direction reversal uses the existing
  DriveBase ramp and adds no stop/brake timing cycle.
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
- K210 is connected as COM14 and currently runs the exact SIGN34 script from
  `07f2b73`; the mode 5 slow-near script remains preserved and backed up.
  UART1 IO8/TX to STM32 USART2 PD6/RX and common ground are unchanged.
- OLED is the external J12 display and includes battery/status information.
- Encoder distance/angle is a wheel-motion estimate; ground yaw requires
  calibration because slip and battery/load change the result.

## Evidence ledger

| Evidence level | Current result | Scope |
|---|---|---|
| computer build/link | passed | release firmware `36551f3`; ELF text/data/bss = 84344/64/11536 bytes; BIN is 84412 bytes |
| host regression | passed | both line-recovery speed configurations, X1/X3 encoder probes, all direction/ordering/load/bypass cases, latest mode 3/4 sign/ring and mode 5 suites pass |
| STM32 flash/readback/GO | passed | temporary source `4947f9c`; COM11 selectively erased 41 pages, preserved calibration, wrote/read back 83332 bytes with `VERIFY OK`, completed `GO OK`, and emitted the `DEFAULT STOP` startup banner |
| GitHub main candidate release | passed, pre-release | `v1.2.0-rc.1` points to merged main `3eb6889`; firmware code `36551f3` is published as BIN/HEX with checksums and separate mode-3/4 and mode-5 K210 assets; no new flash or physical test |
| K210 deployment/runtime | passed, stationary only | COM14; 7256-byte SIGN34 `/sd/main.py` read back exactly; existing model hash verified without rewrite; model load and `SIGN34 ready` observed |
| board-to-board UART | current SIGN34 script not reverified | K210 startup proves local inference initialization, not receipt of `$D` frames by STM32 USART2 |
| wheels off ground | not performed | programmer success does not establish search reversal, mode 5 steering, UART-loss stop or operator STOP response |
| ground driving | not performed | lone-inner bidirectional probing, mode 3/4 routing and mode 5 curve tracking remain unverified |

## Current open issue and next safe step

The board currently runs temporary source `4947f9c`, not release firmware
`36551f3`. The next safe step is a separately authorized flash that preserves
the last calibration page, followed first by an operator STOP check and then a
ground test of KEY1/KEY2 from both lone-X1 and lone-X3 approach angles. The
first probe should start toward the lit sensor and, only after measured wheel
travel, reverse and expand rather than remaining permanently locked.

Modes 3/4 require `k210-mode34-sign-main.py` as K210 `/sd/main.py` plus the
packaged road-sign model. Mode 5 instead requires
`k210-mode5-visual-line-main.py` as `/sd/main.py`; both cannot be active under
that filename at once. Test mode 3/4 exit withdrawal and mode 5 stale-UART stop
separately. If the release is rejected, rollback tag
`rollback/2026-09-08-before-all-modes-update` identifies the immediately prior
canonical source. No ground behavior is established by host tests, build,
release publication, programmer readback, startup text or GO success.

## Update protocol

After integrating code, flashing, or receiving a physical result, the
coordinator must update the applicable fields and evidence row. Keep old facts
in Git history instead of accumulating a long diary here. Never mark a physical
test passed from a successful compilation, programmer verification, OLED text,
or encoder counts alone.
