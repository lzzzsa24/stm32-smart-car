# Shared project state

This is the handoff record for humans, Codex tasks, and delegated models. It is
not a substitute for Git: the checker resolves the live branch and HEAD every
time. Only the integration coordinator updates this file after a merge, flash,
or physical test.

## Machine-readable snapshot

```text
state_schema_version: 1
state_updated_at: 2026-09-10
comprehensive_candidate_source_commit: e9049bbe5b1d7b51990584dd6e1743c4858587b8
comprehensive_candidate_status: e9049bb_COM11_full_readback_GO_passed_not_pushed
comprehensive_remote_branch: test/comprehensive-v15-sign-horn-20260909
comprehensive_candidate_bin_size_bytes: 120184
comprehensive_candidate_bin_sha256: B9CB6328CB60F73C7BF8844382B959589AD5CA47D00EF34EE694171F831C04B4
comprehensive_candidate_hex_sha256: 75B0F7C32957EF07DFB9EAA5217F510D6521977B5FB1F897C5BDB7AB6438B783
integration_branch: main
repository_head_at_update: b48e9e04a89c995f23f162af332e7022044dad61
latest_code_commit: b48e9e04a89c995f23f162af332e7022044dad61
flashed_source_commit: e9049bbe5b1d7b51990584dd6e1743c4858587b8
flash_record_commit: 3271e81
deployed_tag: deployed/2026-09-10-e9049bb
formal_bin_path: manual-build-unified-motion/exp7_unified_motion.bin
formal_hex_path: manual-build-unified-motion/exp7_unified_motion.hex
formal_bin_size_bytes: 120184
flashed_bin_sha256: B9CB6328CB60F73C7BF8844382B959589AD5CA47D00EF34EE694171F831C04B4
flashed_hex_sha256: 75B0F7C32957EF07DFB9EAA5217F510D6521977B5FB1F897C5BDB7AB6438B783
ground_test_status: not_tested_after_e9049bb_deployment
k210_status: COM14_20e8c72_7527_bytes_readback_model_verified_STARTUP_threshold_0_15
candidate_source_commit: b48e9e04a89c995f23f162af332e7022044dad61
candidate_bin_size_bytes: 137532
candidate_bin_sha256: 1F60ABB0C8595EDF10767F2169163B9D1E217467E16134C31CC9E02A9F96DB50
candidate_hex_sha256: 877DA7A24AE1B71EA869EB63445E4A6702AA2B4483F05FDAAB1BA49503AFCAEC
user_reported_flash: tool_verified_e9049bb_readback_GO
k210_candidate_source_commit: 20e8c726dc24006d20477754f3c7eb9aa71c3609
k210_candidate_status: deployed_7527_bytes_D5263ED9_readback_startup_verified
remote_sync_status: github_PR15_merged_rc5_published_7_assets_verified
remote_sync_branch: integration/mode5-fixed-bypass-rc5
stm32_runtime_status: COM11_e9049bb_full_readback_GO_no_post_GO_query
k210_requested_deployment: SIGN34_modes3_4_complete_20e8c72
temporary_flash_selector_commit: e9049bb_comprehensive_flashed
github_release_tag: v1.2.0-rc.5
github_release_source_commit: 71b2e7f
github_release_firmware_commit: 71b2e7f
github_release_status: prerelease_published_7_assets_digest_verified_main_mode5_not_flashed
```

`repository_head_at_update` is the source/history anchor present when this
snapshot was written. Documentation-only governance commits may be newer; the
checker requires the anchor to remain an ancestor and prints the live HEAD.

## Canonical repository layout

### Latest deployment: e9049bb

COM11 full120184-byte write/readback and GO passed, exit0, before the user
interrupted the turn. No repeat flash performed. Reserved audio/calibration
pages excluded, K210 unchanged; no post-GO query, physical test or push.
Evidence: docs/history/deployments/DEPLOYMENT_E9049BB_20260910.md.
Supersedes older board/candidate deployment claims below; main unchanged.

### Latest comprehensive candidate: e9049bb (not flashed)

05b3599 replayed as c25c8b8 without importing its old main-based history;
f307368 merged as e9049bb without conflicts. Mode5 normal straight targets
now scale144% instead of120% (another20%); existing caps/turn profiles remain.
Mode3 normal/ARC/passive exit uses comprehensive mode2 compute and middle
guard, including1800-CPS search. Observation centering and active navigation
ownership remain separate; stopped-heading exit rules retained. Mode4 retained.
Full line suite at both speeds, full sign suite, mode5 integration and formal
ARM build passed. BIN120184 bytes; hashes above. Artifacts under
worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback: rollback/2026-09-10-before-05b3599-f307368 ->5940727.
No flash, serial access, physical test or push. Board remains5940727.
Main firmware remains selective b48e9e0; these two increments are NOT promoted
to main. K210 unchanged. Supersedes older comprehensive candidate statements.

### Current local main: b48e9e0 selective mode3 / mode5 promotion

Source5940727 mode3 and mode5 promoted; mode1/2/4 retain original main
controllers and configurations. Promoted controllers use isolated symbols
and opt-in shared drive extensions, not wholesale composite replacement.
Mode3 now observation centering/2s hold/stopped-heading-priority exit/horn;
mode5 fast following/rolling capture/pulsed counter-turns and fixed bypass.
Mode2 middle guard and mode4 gyro-tangent profile are NOT selected in main.
Source-equivalence/scope checker, legacy line/sign, promoted line/sign/real
DriveBase, gyro and mode5 bypass suites passed; formal ARM build passed.
BIN137532 bytes; exact candidate hashes above. See
docs/history/candidates/MAIN_MODE35_20260910.md and README mode table.
Rollback: rollback/2026-09-10-before-main-mode35 ->3257b05.
No flash/push/release or physical test. Board remains full comprehensive
5940727; K210 unchanged. Main candidate is NOT the board image. Published
rc.5 remains71b2e7f; older local-main code statements below are superseded.

### Latest deployment: 5940727 after USB reconnect

Fresh COM11 enumeration, exact120112-byte candidate hash checked, selective
59-page erase, full write/readback and GO passed with exit0. Reserved audio
and calibration pages excluded. Earlier interrupted-write warning below is
resolved by this complete retry; board now has verified5940727 application.
Includes mode2 middle guard, mode3 stopped-heading policy, mode4 continuous
ARC control and retained mode5 fast following. K210/main firmware unchanged.
No post-GO query, physical test or push. Evidence:
docs/history/deployments/DEPLOYMENT_5940727_20260910.md.

### Current board warning: 5940727 deployment interrupted after erase

Latest authorized flash targeted exact5940727 BIN120112 bytes, SHA256
031479C70C40B5B7EF57A54D80E67558220BE5150343C49EE61540B8A84B6BAB.
Fresh enumeration identified CH340K COM11. PowerShell7 programmer at57600
with PreserveLastPage reported BOOTLOADER ACK and ERASE OK for59 pages,
then exited1 with device-not-functioning error during Close. No WRITE OK,
VERIFY OK or GO was received. Application contents/execution are unconfirmed;
the old891a7e1 cannot be assumed runnable after this erase. Reserved audio
and calibration pages were excluded. Re-enumeration still showedCOM11,
but one recovery retry failed at Open with the same device error, before
another erase/write. USB replug is required before retrying exact5940727.
flashed_source_commit/deployed_tag retain the LAST SUCCESSFUL verification,
not the current executable state. K210 untouched; no motion or push.

### Latest comprehensive candidate: 5940727 (not flashed)

Merged831fb06 onto52a67b6 without conflicts. Mode4 acquired ARC retains
the follower across white/symmetric contacts, using its remembered forward
curvature rather than switching route/guard owners and resetting tracking.
Trace now records line action, motor owner and route-active state changes.
Mode2 middle guard, mode3 stopped-heading policy and mode5 fast profile retained.
Full line suite (both speeds), full sign suite, mode5 integration and formal
ARM build/link passed; BIN120112 bytes, hashes above. Artifacts under
worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback: rollback/2026-09-10-before-831fb06 ->52a67b6.
No flash, serial access, physical test or push. Board remains891a7e1;
K210/main firmware unchanged. Supersedes older candidate entries below.

### Latest comprehensive candidate: 52a67b6 (not flashed)

Merged 89e78e2 as a2bb8ae; replayed main-based 0c32484 as52a67b6,
adapting its mode2 middle guard to the composite bounded-wait owner and
preserving mode1 boost, mode3/4 route helpers and mode5 fast following.
Mode3 uses stopped observation heading first with wider exit tolerances.
Mode2 opts into1800-CPS middle guard,1412-CPS inner correction and immediate
search when both middle probes disappear; reset prevents profile leakage.
Full line-recovery suite (both speeds), full sign-line suite, mode5 app
profile and formal ARM compile/link passed. BIN119264 bytes; hashes above.
Artifacts: worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback tag: rollback/2026-09-10-before-89e78e2-0c32484 ->891a7e1.
No serial access, flash, physical test or push. Board remains891a7e1;
main firmware and K210 unchanged. This candidate supersedes older candidate
entries below, not the recorded deployment.

### Latest deployment: 891a7e1

50ce17f integrated with mode4 wide-line and mode5 straight-speed update.
Full sign suite, integration checks and ARM build passed. COM11 full118840
bytes readback and GO passed; reserved pages excluded. No post-GO query,
OLED/physical test or push; K210/main firmware unchanged. Evidence:
docs/history/deployments/DEPLOYMENT_891A7E1_20260910.md.
Supersedes older board/candidate statements below.

### Latest comprehensive candidate: 5ee5dbe (not flashed)

b43041e replayed onto4ac67d8. Mode5 equal positive normal-forward CPS
targets scale120%, retaining downstream caps and corner/crossing/rejoin
targets. Real speed gain remains unverified. Mode3/4 fixes retained. Full
line (both speeds) and sign suites, mode5 integration and ARM build passed.
BIN116916 bytes; hashes above. Artifacts under
worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback: rollback/2026-09-10-before-b43041e. No serial/flash/physical test
or push; board remainsaf602cf, K210/main firmware unchanged.

### Latest comprehensive candidate: 4ac67d8 (not flashed)

4940f2f merged ontoaf602cf without conflicts. Shared current-line-priority
ARC profile steers asymmetric wide contacts by weighted side; symmetric
wide input remains forward. Affects mode4 and mode3 phases using that shared
profile; ordinary tracking and mode5 profiles retained. Full sign and line
suites (both speeds), mode5/gyro integration checks and ARM build passed.
BIN116836 bytes, hashes above; artifacts in
worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback: rollback/2026-09-10-before-4940f2f. No serial, flash, physical test
or push; board remainsaf602cf, K210/main firmware unchanged.

### Latest deployment: af602cf

3bd989a integrated with mode4 live-line priority and mode5 changes retained.
Full sign/line suites, integration checks and build passed. COM11 full116700
bytes readback and GO passed, reserved pages excluded. No post-GO query,
OLED/physical test or push; K210/main code unchanged. Evidence:
docs/history/deployments/DEPLOYMENT_AF602CF_20260910.md.
Supersedes older candidate and board-source statements below.

### Latest comprehensive candidate: 29d490b (not flashed)

f278143 merged onto947794d. Mode4 fallback black contact immediately yields
to live ARC following; remove saved-direction override and restrict ARC
search guard to white. Conflicts resolved preserving latest mode3 pause
handling, stopped-heading reference and exit reacquisition. Mode5 retained.
Full sign suite, mode1/2, gyro and mode5 integration checks and formal build
passed. BIN115808 bytes; hashes above. Artifacts under
worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback: rollback/2026-09-10-before-f278143. No serial, flash, physical test
or push; board remains947794d, K210/main firmware unchanged.

### Latest deployment: 947794d

79d4e9d integrated; mode3 yields exit turn on reacquired black after white.
Full sign tests, integration checks and build passed. COM11 full115996-byte
readback and GO passed; reserved pages excluded, K210/main code unchanged.
No post-GO query, OLED/physical test or push. Previous OLED normal report
applies to98a4a8f, not automatically to this new image. Evidence:
docs/history/deployments/DEPLOYMENT_947794D_20260910.md.
Supersedes older board/candidate statements below.

### OLED retest outcome: normal on latest98a4a8f

User reports OLED normal after reflashing the same latest98a4a8f. Earlier
black screen did not reproduce in this comparison; no deterministic firmware
regression or hardware cause is established. Keep latest firmware unchanged.
No motion-test evidence. This supersedes OLED-pending statements below.

### Current board: 98a4a8f OLED retest

User requested latest again after OLED returned on d01ab4c. Reflashed exact
98a4a8f; COM11 full115920-byte readback and GO passed, reserved pages excluded.
OLED result pending user observation; no hardware root-cause claim. K210
unchanged; no post-GO query, motion or push. Evidence:
docs/history/deployments/OLED_RETEST_98A4A8F_20260910.md.
Supersedes the board rollback below, preserving its user-observed OLED result.

### Current board: OLED comparison rollback to d01ab4c

After OLED black-screen report on98a4a8f, exact rebuilt d01ab4c passed COM11
full115456-byte readback and GO. User then reported OLED returned. This
supports investigating98a4a8f software differences, but reset/initialization
is a possible confounder; exact root cause remains unproven. Leave d01ab4c
on hardware; comprehensive branch/candidate remains98a4a8f. K210 unchanged.
No motion test. Evidence: docs/history/deployments/OLED_ROLLBACK_D01AB4C_20260910.md.
Supersedes previous board98a4a8f statements below.

### Latest deployment: 98a4a8f

COM11 full115920-byte readback and GO passed; reserved pages excluded.
Mode3 live exit, mode4 first fallback contact and mode5 changes included.
K210/main code unchanged; no post-GO query, motion/ground test or push.
See docs/history/deployments/DEPLOYMENT_98A4A8F_20260910.md.
Supersedes older unflashed/board-source statements below.

### Latest comprehensive candidate: 98a4a8f (not flashed)

2fb4f35 and876e9b6 merged onto d01ab4c, including dependency c35df1a.
Mode4 accepts first fallback contact with saved-side guidance; mode3 aligned
exit yields immediately to live tracking/search. Observation resume consumes
live inner evidence and excludes paused time from route deadlines. Conflict
resolution retains mode3 stopped-heading reference plus mode4 fallback hint.
Mode5 constrained pulse steering retained. Full sign suite, mode1/2, gyro
and mode5 integration checks and formal build passed. BIN115920 bytes;
hashes above. Artifacts in worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback: rollback/2026-09-10-before-2fb4f35-876e9b6. No serial, flash,
physical test or push; board remains d01ab4c, K210/main firmware unchanged.

### Latest deployment: d01ab4c

COM11 full115456-byte readback and GO passed, reserved pages excluded.
Includes mode3 earlier exit, mode4 heading-based arc exit and mode5 constrained
pulse steering. No post-GO query or physical test; K210/main code unchanged.
See docs/history/deployments/DEPLOYMENT_D01AB4C_20260910.md.
Supersedes older board/unflashed statements. Records local only, no push.

### Latest comprehensive candidate: d01ab4c (not flashed)

4ec5bd5 merged ontoa05d73c, including40f8888 history but its fixed fallback
turn is superseded. Mode4 fallback self-tracks the arc and triggers exit at
signed60 degrees from approach heading. Mode3's latest55/65-degree selection
and natural completion retained when resolving the shared exit-block conflict.
Mode5 constrained pulse steering retained. Full sign suite, mode1/2, gyro and
mode5 integration checks and ARM build passed. BIN115456 bytes; hashes above.
Artifacts: worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback: rollback/2026-09-10-before-4ec5bd5. No serial/flash/physical test/push;
board remains4eda8fe, K210/main firmware unchanged.

### Latest comprehensive candidate: a05d73c (not flashed)

609f29b merged ontoaf34e32. Mode3 exit alignment starts at signed55-degree
outgoing-edge or65-degree narrow-line evidence, preserving independent
natural-completion evidence. Mode4 recovery and mode5 constrained pulse
steering retained. Full sign suite and mode1/2, gyro, mode5 integration checks
plus formal ARM build passed. BIN115356 bytes; comprehensive hashes above.
Artifacts: worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback: rollback/2026-09-10-before-609f29b. No serial, flash, physical test
or push; board remains4eda8fe, K210/main firmware unchanged.

### Latest comprehensive candidate: af34e32 (not flashed)

a051788 replayed onto4eda8fe. Mode5 bounds forward steering and requires
two distinct nearby middle snapshots for rolling capture, preserving its
recovery direction and moving correction until centered. Mode3/4 fixes retained.
Full line suite (both speeds), full sign suite, mode5 integration and formal
build passed. BIN115352 bytes; hashes above. Artifacts under
worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback: rollback/2026-09-10-before-a051788. No serial, flash, physical test
or push. Board remains4eda8fe; K210/main firmware unchanged.

### Latest deployment: 4eda8fe

COM11 full115184-byte readback and GO passed; reserved pages excluded.
Includes mode3 natural exit, mode4 fallback-to-ARC and mode5 pulse steering.
No post-GO query or physical test. K210/main firmware unchanged. See
docs/history/deployments/DEPLOYMENT_4EDA8FE_20260910.md.
Supersedes older unflashed/board statements. Deployment records local only.

### Latest comprehensive candidate: 4eda8fe (not flashed)

b75ba61 merged ontof5a91a6 without conflicts. Mode3 natural departure below
the old angle gate uses lower/upper-half evidence, heading return and stable
forward line travel; adds exit-heading peak diagnostics. Mode4 recovery and
mode5 pulse steering retained. Full sign suite (including mirrored departure,
false-completion rejection and mode4), mode1/2, gyro and mode5 integration
checks and formal build passed. BIN115184 bytes; hashes above. Artifacts in
worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback: rollback/2026-09-10-before-b75ba61. No serial, flash, physical test
or push; board remainse047c13, K210/main firmware unchanged.

### Latest comprehensive candidate: f5a91a6 (not flashed)

4335b19 replayed onto4d5f8c7. Mode5 normal steering uses3300-PWM macro
pulses, max24ms drive per40ms cycle, ratio/encoder cutoff and1ms OFF-only
deadline hook. Straight/search/bypass/ordinary modes retain their ownership.
Composite mode5 runner adapted to its existing gyro/bypass suite; sign host
links include the new pulse module. Full line (both speeds), mode5/gyro and
sign suites and ARM build passed. BIN114448 bytes; hashes above. Artifacts
in worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback: rollback/2026-09-10-before-4335b19. Mode3/4 fixes retained; no serial,
flash, ground test or push. Board remainse047c13; K210/main firmware unchanged.

### Latest comprehensive candidate: 4d5f8c7 (not flashed)

c37ed64 and prerequisite dff6f74 merged ontoe047c13 without conflicts.
Mode4 missed-arc recovery: after8-cm diagonal probe, return to road heading,
advance to reacquire the arc, then keep ARC/gyro-exit navigation instead of
premature LOCK. Mode3 stopped-heading exit and mode5 no-hold follow retained.
Full sign suite, mode1/2, gyro and mode5 integration checks and ARM build
passed. BIN112632 bytes; hashes above. Artifacts in
worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion.
Rollback: rollback/2026-09-10-before-c37ed64. No flash, serial, physical test
or push; board remainse047c13, K210/main code unchanged.

### Latest comprehensive deployment: e047c13

01e8008 integrated with mode4 spin and mode5 no-hold following. Mode3 uses
the two-second stopped heading as exit reference; shared observation gate26%.
Full sign tests, integration checks and build passed; COM11 full112144-byte
readback and GO passed. Reserved pages preserved, K210/main code unchanged.
No post-GO query, motion test or push. See
docs/history/deployments/DEPLOYMENT_E047C13_20260910.md.
Supersedes older board/candidate statements below.

### Latest comprehensive candidate: d7218e1 (not flashed)

768bc53 replayed onto47f4796, retainingc7bbaaa mode4 spin andfd8472b mode3
exit release. Mode5 fast follow removes timed edge/crossing/gap/rejoin holds
and captures current narrow middle evidence immediately; direction memory
and motor ramp/STOP/obstacle priority remain. Ordinary modes retain their
previous rules. Greater response to brief sensor noise remains a ground risk.
Full sign and line (both speeds) suites, mode5 integration and formal build
passed. BIN111792 bytes; comprehensive hashes above. Artifacts are in
worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion. Rollback:
rollback/2026-09-10-before-768bc53. No serial, flash, physical test or push;
board remainsed7eb3b, K210/main firmware unchanged. Supersedes prior candidate.

### Latest comprehensive candidate: 47f4796 (not flashed)

c7bbaaa merged ontoed7eb3b without conflicts. Mode4 initial diagonal entry
turn now counter-rotates both sides instead of a forward pivot. Mode3 exit
release, mode5 fast-follow and existing other modes retained. Full sign suite,
full line suite (both speeds), mode5 integration check and formal build passed.
Artifacts under worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion;
BIN111568 bytes, hashes in comprehensive fields. Rollback tag:
rollback/2026-09-10-before-c7bbaaa. No serial/flash/physical test/push; STM32
remainsed7eb3b, K210 and main firmware unchanged. Remote comprehensive still
ed7eb3b until separately requested synchronization.

### Latest comprehensive deployment: ed7eb3b (2026-09-10)

COM11 wrote and read back all111280 bytes; VERIFY OK and GO OK, exit0.
Only55 application pages erased; audio/calibration reserved pages excluded.
Mode3 exit release, mode4 drawn route and mode5 fixed bypass/fast four-line
follow are active in this image. Main firmware remains rc.5; K210 unchanged.
No post-GO query, mode start or physical test. Evidence:
docs/history/deployments/DEPLOYMENT_ED7EB3B_20260910.md.
Supersedes older unflashed/board58c3744 statements below. This deployment
record is local only; no GitHub push in the flash task.

### GitHub comprehensive synchronization (2026-09-09)

Remote test/comprehensive-v15-sign-horn-20260909 now containsed7eb3b,
includingfd8472b,852ade1 and the adaptedfb311e3 fast mode5. Both local
pre-integration rollback tags were pushed. Main receives shared-state docs
only through its protected-branch PR workflow; comprehensive code is not
promoted to main. No new release, hardware access or deployment.
This supersedes the older not-pushed statements for this candidate below.

### Latest comprehensive candidate: ed7eb3b (not flashed)

On comprehensive-v15-sign-horn,852ade1 merged as7baf411 preservingfd8472b;
fb311e3 adapted ased7eb3b. Mode4 uses turn/straight-entry/live-arc/turn/exit.
Mode5 now fixed bypass plus fast four-line follow, replacing visual control.
Mode1/2 parameters and mode3 exit release retained; K210 unchanged. Main
firmware remains rc.5. Full sign, line (both speeds), gyro/bypass, archived
vision and audio suites, composite mode5 checks and exact ARM build passed.
BIN111280 bytes with hashes in the comprehensive fields above. Worktree:
F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn; artifacts in its
manual-build-unified-motion directory. Rollback tag:
rollback/2026-09-09-before-comprehensive-mode45. Integration notes are in that
candidate's docs/history/candidates/COMPREHENSIVE_MODE45_20260909.md.
No serial access, flash, physical test or GitHub push. Board remains58c3744.
This supersedes the older comprehensive candidate below, not main rc.5.

### Latest comprehensive candidate: fd8472b (not flashed)

Comprehensive-v15-sign-horn fast-forwarded from58c3744 tofd8472b. Mode3
releases route direction and motor ownership after confirmed exit; visible
line feedback takes priority during exit, and OLED/STRACE expose real phase
and requested wheel speeds. Mode4 driving profile, mode1 bypass, K210,
motor mapping and main firmware code remain unchanged. Full sign, line and
gyro host suites and formal build passed. BIN111196 bytes; detailed hashes
and rollback are in docs/history/candidates/COMPREHENSIVE_FD8472B_20260909.md.
No serial access, flash, physical test or GitHub push. Board remains58c3744.
The main rc.5 candidate fields/artifacts above remain separate and unchanged.

### Published main rc.5: mode5 fixed black-line bypass

PR15 merged as71b2e7f; v1.2.0-rc.5 published with7 size/digest-verified assets.
Main mode5 replaces K210 visual line with composite58c3744 mode1 behavior.
Main modes1-4 preserved; main3/4 are NOT the new composite3/4 controllers.
Build and six host suites passed; no flash or physical test. Board remains
58c3744 and K210 unchanged. Composite branch and rollback/deployment tags
also pushed. Current main firmware BIN102264 bytes is NOT the board image.
See docs/history/candidates/MAIN_MODE5_FIXED_RC5_20260909.md.
This supersedes older main-source, release and mode5-visual claims below.

### Current main: mode5 fixed bypass promotion (cd44bb0)

Main mode5 now uses composite58c3744 mode1 behavior, replacing visual-line
dispatch. Fixed250/300-mm route,4000-CPS travel,16/22-cm sonar, side IR off.
Main modes1-4 retained; profile switching restores legacy parameters/IR state.
Main mode1 adaptive return remains1800 CPS; mode5 fallback is2100 CPS.
Build and sign/line/gyro/audio/archived-vision/new-mode5 host tests passed.
No serial access or flash: board remains58c3744, K210 unchanged. Preparing
v1.2.0-rc.5 through protected-main PR. Rollback tag:
rollback/2026-09-09-before-mode5-fixed-bypass. Source main is no longer the
older mode1-only2ebf64e described below. Historical mode5 visual docs are not
current instructions. Release guide: docs/releases/RELEASE_V1.2.0_RC5.md.

### Latest comprehensive deployment: 58c3744

COM11 full109708-byte readback and GO passed, reserved pages preserved.
Mode4 now gyro-tangent trajectory, not STOP; digital0 remains STOP. Mode3
live-line behavior retained. Side IR disabled. K210 and main code unchanged.
No post-GO query or physical test. See
docs/history/deployments/DEPLOYMENT_58C3744_20260909.md.
Supersedes older unflashed-candidate and STM32 deployment statements below.

### Latest unflashed candidate: 58c3744

Comprehensive branch fast-forwarded from a6c61d3. Adds mode4 gyro-tangent
trajectory while retaining mode3 live line feedback and latest mode1 bypass.
Digital4 is a driving mode in this candidate, not STOP; board remains a6c61d3.
Build and full sign/line/gyro suites passed. No serial/flash/physical test/push.
K210 and main code unchanged. Side IR remains disabled. See
docs/history/candidates/COMPREHENSIVE_58C3744_20260909.md.
Supersedes older candidate statements only.

### Latest comprehensive deployment: a6c61d3

Requested candidate now flashed: COM11 full108800-byte readback and GO
passed; reserved pages preserved. Mode3 consolidated sign control,4=STOP;
side IR remains disabled. K210/main code unchanged. No post-GO/physical test.
See docs/history/deployments/DEPLOYMENT_A6C61D3_20260909.md.
Supersedes older unflashed-candidate and STM32 deployment statements below.

### Latest unflashed candidate: a6c61d3

b67ca7b integrated onto5cb1bd3. Sign control now mode3 only,4=STOP/reserved;
ARC live feedback and phase-anchored110-degree entry search updated. Mode1
250/300-mm fast bypass retained; side IR disabled. Full sign/line/gyro suites
and build passed. No serial/flash/physical test/push. Board remains533e355;
K210 and main code unchanged. See
docs/history/candidates/COMPREHENSIVE_A6C61D3_20260909.md.
Supersedes older candidate statements only.

### Latest unflashed candidate: 5cb1bd3

948c3db integrated onto533e355. Active fixed bypass constants now250/300 mm;
4000-CPS fixed travel and slow sign modes retained. Formal build and complete
gyro/line suites passed. No serial/flash/physical test/push. Board remains
533e355, K210 and main code unchanged. Side IR remains disabled. See
docs/history/candidates/COMPREHENSIVE_5CB1BD3_20260909.md.
Supersedes older candidate statements only.

### Latest comprehensive deployment: 533e355

3b3cab4 and 2de30bb integrated onto16b5092. Modes3/4 use KEY2 slow settle
profile; mode1 fixed bypass4000 CPS, approach16 cm/dynamic22 cm. Geometry
300/360 mm and side-IR-disabled policy retained. Full sign/line/gyro suites
and build passed. COM11 full108608-byte readback and GO passed; reserved
pages preserved. K210 and main code unchanged. No post-GO/physical test.
See docs/history/deployments/DEPLOYMENT_533E355_20260909.md.
Supersedes older candidate and STM32 deployment statements below.

### Latest comprehensive deployment: 16b5092

7b56dad integrated onto e8f6fbe. Sign modes remove moving-speed caps but
retain two-second observation stop. Includes 30/36-cm bypass; side IR remains
disabled. Full sign/line/gyro suites and build passed. COM11 full 108344-byte
readback and GO passed, reserved pages preserved. K210 and main code unchanged.
No post-GO query or physical test. See
docs/history/deployments/DEPLOYMENT_16B5092_20260909.md.
Supersedes older candidate and STM32 deployment statements below.

### Latest unflashed candidate: e8f6fbe

078cfdb integrated onto bd87633; fixed bypass now 300/360 mm. Other controls
retained, including disabled side IR. Formal build and full gyro/line suites
passed. No serial, flash, physical test or push; board remains bd87633,
K210 unchanged. Main code unchanged. See
docs/history/candidates/COMPREHENSIVE_E8F6FBE_20260909.md.
Supersedes older candidate statements only.

### Latest comprehensive deployment: bd87633

After USB reconnect, COM11 wrote and read back all 108748 bytes and GO passed.
Exact candidate hash verified before deployment; reserved pages preserved.
Includes c133918 and bc40f79, with side obstacle IR still disabled. K210 and
main code unchanged. No post-GO query or physical test. See
docs/history/deployments/DEPLOYMENT_BD87633_20260909.md.
Supersedes previous USB blocker and older STM32 deployment statements below.

### Latest candidate bd87633; flash blocked before port open

c133918 and bc40f79 integrated onto 6894af1: 360/600-mm bypass and modes3/4
shared KEY2 tracking with retained route constraints. Full sign/line/gyro
tests and build passed. COM11 Open failed with device-not-functioning error
before erase/write; reconnect required. Board Flash remains 6894af1.
K210 unchanged, side IR still disabled. No physical test or push.
See docs/history/candidates/COMPREHENSIVE_BD87633_20260909.md.
Supersedes older candidate statements, not successful deployment records.

### Latest comprehensive deployment: 6894af1

2c182e3 integrated onto 0cdf8ee. Approach threshold 20 cm, dynamic maximum
30 cm, raw critical 10 cm; active bypass 15-cm guard discards old-heading
echoes. Original encoder speed controller and disabled side IR retained.
Full sign/line/gyro suites and build passed. COM11 full 108364-byte readback
and GO passed; reserved pages preserved. K210 and main code unchanged.
No post-GO query or physical test. See
docs/history/deployments/DEPLOYMENT_6894AF1_20260909.md.
Supersedes older candidate and STM32 deployment statements below.

### Latest comprehensive deployment: 0cdf8ee

a90ce13 integrated onto 9ba87c3. Original encoder speed controller restored;
experimental low-speed powered/coast path and 500-CPS search cap removed.
Independent ARC-direction fix and disabled side-IR policy retained. Side IR
protection remains absent. Build and full sign/line/gyro suites passed.
COM11 full 108164-byte readback and GO passed; reserved pages preserved.
K210 and main code unchanged. No post-GO query or physical test. See
docs/history/deployments/DEPLOYMENT_0CDF8EE_20260909.md.
Supersedes older candidate and STM32 deployment statements below.

### Latest comprehensive deployment: 9ba87c3

5bb2abd integrated onto a5b618d. Obstacle IR disabled for fixed-route
isolation; ultrasonic, tracking, remote and low-speed sign power retained.
Side IR protection is absent in this test image. Full sign/line/gyro suites
and build passed. COM11 full 109208-byte readback and GO passed; audio and
calibration pages preserved. K210 and main firmware code unchanged.
No post-GO query or physical test. See
docs/history/deployments/DEPLOYMENT_9BA87C3_20260909.md.
Supersedes older unflashed-candidate and STM32 deployment statements below.

### Latest unflashed comprehensive candidate: a5b618d

2e9a38e integrated onto ec2dd2f. Sign-only low-speed powered phases retain
breakaway power while encoder-accounted coasting preserves target speed.
Sign, line and gyro regression suites and formal build passed. Synthetic
friction tests are not physical validation. Board remains ec2dd2f; K210 and
main code unchanged. No serial access, flashing or push in this merge task.
See docs/history/candidates/COMPREHENSIVE_A5B618D_20260909.md.
This supersedes older candidate statements below, not deployment evidence.

### Latest comprehensive deployment: ec2dd2f

11a243f integrated onto 8b91f49, retaining 24-cm bypass. Sign-only low-speed
feedback regulation and 500-CPS search enabled. Full sign/line/gyro suites
and build passed. COM11 109740-byte full readback and GO passed, reserved
pages preserved. K210 unchanged; no post-GO/physical test. Main code unchanged.
See docs/history/deployments/DEPLOYMENT_EC2DD2F_20260909.md.
Supersedes older unflashed-24cm and STM32 deployment notes below.

### Latest unflashed candidate: 8b91f49

c7f68e6 integrated onto 5fdb84a, fixed offset now 240 mm; parallel 120 mm,
return 45 degrees and latest sign behavior retained. Build, gyro and line
regressions passed. No serial/flash/physical test/push; board stays 5fdb84a.
See docs/history/candidates/COMPREHENSIVE_8B91F49_20260909.md.

### Latest comprehensive deployment: 5fdb84a

0768639 integrated onto a773196, including mode1 fixed rectangle and current
sign handoff. Sign suite/build passed. COM11 full 108908-byte readback and GO
passed; reserved pages preserved. K210 unchanged, no post-GO/physical test.
See docs/history/deployments/DEPLOYMENT_5FDB84A_20260909.md.
This supersedes unflashed-rectangle and older board statements below.

### Latest unflashed comprehensive candidate: a773196

74c0d66 integrated onto 3a00fa8. Mode1 fixed right 180/120-mm bypass and
45-degree diagonal return, with adaptive fallback; latest sign modes retained.
Formal build, line (both speeds), gyro and sign suites passed. No flash,
serial access, physical test or push; board remains 3a00fa8 and K210 unchanged.
See docs/history/candidates/COMPREHENSIVE_A773196_20260909.md for hashes.

### Latest deployment: 3a00fa8

Requested combined speed/handoff candidate is now flashed: COM11 full
106384-byte readback and GO passed; reserved pages preserved. K210 unchanged.
No post-GO or physical test. See docs/history/deployments/DEPLOYMENT_3A00FA8_20260909.md.
This supersedes the unflashed/a56f6c7 board notes below. Main code unchanged.

### Latest unflashed comprehensive candidate: 3a00fa8

93b408b and bf3e8e1 integrated onto a56f6c7 in comprehensive-v15-sign-horn.
Mode1 stable straight/clear return speed increase and modes3/4 verified
entry-line handoff coexist. Line (both speeds), sign and gyro suites passed,
formal build passed. Obsolete gyro test assertion corrected, not controller.
No serial/flash/push/physical test; board stays a56f6c7, K210 stays 20e8c72.
Hashes above identify candidate; see docs/history/candidates/COMPREHENSIVE_3A00FA8_20260909.md.

### Latest comprehensive deployment: a56f6c7

12fd7f2 final delta integrated onto 426dedd; sign suite/build passed.
COM11 full 105536-byte readback and GO passed; reserved pages preserved.
Entry releases route motor ownership on loss/center/opposite-only contact;
exit gyro alignment and other features retained. K210 unchanged at .15.
No post-GO/physical test or push; main code unchanged. See
docs/history/deployments/DEPLOYMENT_A56F6C7_20260909.md.
Supersedes older STM32 deployment notes below.

### Comprehensive branch synchronized with deployed 917df47

Merge 426dedd31459a117256d70299733cf2c249a693f on
test/comprehensive-v15-sign-horn-20260909 includes 917df47. Core, K210, tests,
Drivers, build script and linker script match that previously tested source.
Formal rebuild passed; BIN and HEX SHA256 exactly match the deployed 917df47
hashes above. Worktree is clean. No new flash, physical test or GitHub push.
This supersedes the older statement that this branch still points to 6bd0c89.
Main firmware code is unchanged; only the comprehensive test branch was merged.

### Latest exact test deployment: 917df47

Requested source 917df4763fdfd0a669b3f5366cc2eae2ed508048 rebuilt in
worktrees/sign-gyro-exit-straight; full sign suite passed. COM11 full
105612-byte readback and GO passed, reserved pages preserved. No source merge.
K210 unchanged at 20e8c72 threshold .15; no post-GO or physical test.
Docs: docs/history/deployments/DEPLOYMENT_917DF47_20260909.md.
Supersedes older STM32 deployment entries. Comprehensive-v15 branch still
points to 6bd0c89; do not mistake that checkout's older BIN for this board image.

### Latest STM32 deployment: 6bd0c89

6bc7d28 integrated onto comprehensive 20e8c72. Sign suite/build passed.
COM11 write/full readback 105728 bytes and GO passed; reserved pages preserved.
Confirmed entry choice and gyro-aligned bounded exit travel updated. Pause,
horn and RGB-off retained; K210 unchanged at paired 20e8c72 threshold .15.
No post-GO or physical test. See docs/history/deployments/DEPLOYMENT_6BD0C89_20260909.md.
This supersedes older STM32 deployment statements; main code unchanged.

### Latest paired deployment: 20e8c72

2c3abe0 integrated into comprehensive 37a04b0. Full sign suite/build passed.
STM32 COM11 full 105472-byte readback and GO passed; reserved pages preserved.
K210 COM14 paired 7527-byte script readback and model hash verified, startup
SIGN34 ready with threshold 0.15 observed. First direction observation pause
is 2 seconds, OLED OBSERVE; gyro/RGB-off/five-repeat horn retained.
No wheel/ground/sign-response test. Main code unchanged. Details and backup:
docs/history/deployments/DEPLOYMENT_20E8C72_20260909.md.
This supersedes older board/K210 deployment and pending-threshold notes below.

### Latest STM32 deployment: 37a04b0

4c81dce integrated into comprehensive 771586e; sign suite/build passed.
COM11 write/full readback 104956 bytes and GO passed; reserved pages preserved.
First positive recognition applies 500-CPS forward peak for 1500 ms.
K210 port absent, hardware unchanged at 0987233 threshold 0.20. Candidate
0.15 script still needs deployment. No post-GO or physical test.
See docs/history/deployments/DEPLOYMENT_37A04B0_20260909.md.
Supersedes older STM32 deployment statements below; main code unchanged.

### Latest deployment: 771586e after reconnect

COM11 replug retry succeeded: 104940-byte write/full readback and GO passed,
52 application pages erased with audio/calibration pages excluded. K210
unchanged (paired 0987233 SIGN34 script). No post-GO query or physical test.
See docs/history/deployments/DEPLOYMENT_771586E_20260909.md. This supersedes
the pending/failed bootloader notes below. Main firmware code remains unchanged.

### Latest candidate 771586e; deployment blocked

b357a4d final delta integrated into comprehensive 0987233: proportional
forward steering weights and 700-CPS all-black cap. Build and relevant checks
passed. Two COM11 bootloader-sync attempts failed before erase/write; old
0987233 Flash remains, but execution after reset probing is unconfirmed.
K210 unchanged. Replug required; see
docs/history/candidates/PREPARED_771586E_20260909.md for artifacts/evidence.

### Latest paired deployment: 0987233

Comprehensive test includes 5f2de2d live hint/gyro +/-25-degree search and
excludes sign modes from forced wait-recovery, preserving RGB-off, five-repeat
horn, trace and other modes. Full sign_line suite and ARM build passed.
STM32 COM11: 104804-byte full readback and GO passed; reserved pages preserved.
K210 COM14: paired 7334-byte sign_mode34.py written/read back, model hash
verified unchanged, SIGN34 single-best left/right/horn startup observed.
No wheel/ground/sign-response or board-link test. Main code is not promoted.
See docs/history/deployments/DEPLOYMENT_0987233_20260909.md for hashes/backup.
This supersedes all older board/K210 deployment statements below.

### Latest deployment: c50d3c8 after reconnect

COM11 reappeared in fresh serial enumeration after user replug. Exact
104232-byte candidate passed write, full readback and GO at 57600 baud;
51 application pages erased, audio and calibration pages excluded.
No post-GO zero-output query or physical test. K210 unchanged, new horn
recognition script not yet deployed. See
`docs/history/deployments/DEPLOYMENT_C50D3C8_20260909.md`.
This supersedes all disconnected/V14-board statements below. Main code
remains mode1-only promotion; comprehensive test source is c50d3c8.

### Latest unflashed comprehensive candidate: V15

Latest source is now `c50d3c81a992fc3d5cd7e91d42ceeaa9c3385821`, merging
`6908938` RGB-off in modes 3/4 while retaining five-repeat horn and gyro/trace.
Full sign-line host suite and ARM build passed. Flash requested but blocked:
live enumeration found Bluetooth ports only, no STM32 USB serial device.
No erase/write/GO attempted; both boards unchanged. Prepared artifact hashes
above supersede the older V15 hashes. See
`docs/history/candidates/PREPARED_C50D3C8_RGB_OFF.md`.

Source `9cea6bcc2b9b943ae79a53040b2517dda3c8a6b5`, branch
`test/comprehensive-v15-sign-horn-20260909`, worktree
`F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn`.
Merges full V14 and `4c24f1f`: highest-score single left/right/horn selection,
confirmed PG12 horn event in modes 3/4 now plays five complete phrases
(nominally 7.65 seconds, nonblocking), preserving STRACE and gyro logic.
Five-repeat integration assertion, unchanged horn latch regression and formal
build passed. No new physical or listening test.
Sign-line/K210/gyro/trace/horn and DFPlayer host tests plus ARM build passed.
See that worktree's `COMPREHENSIVE_V15_SIGN_HORN.md` for artifacts. No flash,
physical test or push; deployed STM32 and K210 remain unchanged. Testing the
new horn end-to-end requires both candidate STM32 and K210 script deployment.
Main firmware scope remains mode1-only V13 promotion. Candidate hashes above
now describe V15, not the older main build mentioned below.

### Current local main: mode1-only V13 promotion

Main code 2ebf64e promotes V13 mode1 and required MPU6050/calibration modules.
Includes gyro/fallback bypass, sonar continuity, measured >45-degree inward
continuous return and queued outer-contact rejoin. Modes3/4 and mode5 retain
their previous main controllers and adapters. Mode2 retains legacy recovery;
its normal follow path is an equivalent shared-helper extraction. All five host
suites and ARM build passed (101948-byte BIN, hashes above). See
`docs/history/candidates/MAIN_MODE1_V13_20260909.md`. Rollback tag:
rollback/2026-09-09-before-mode1-v13. This local main image is NOT flashed or
pushed; hardware remains comprehensive V13 b326a6a with its separate evidence.
Older statements that main is still rc.4 are superseded by this paragraph.

### Latest deployment override — 2026-09-09

Current STM32 is V14 8a46fd6: includes 576fdbc route trace and K210-source
prerequisite 624e8b4 on full V13. Build, full 103688-byte readback and GO passed.
K210 hardware unchanged (new TX-selection source has NOT been deployed).
No post-GO runtime or motion test. GitHub PR #13 later published the current
local-main history together with the documentation reorganization; this remote
operation did not flash either board. See
`docs/history/deployments/DEPLOYMENT_V14_8A46FD6_20260909.md`.
Older board-source statements below are history; main retains mode1-only scope.

Current board is V13 b326a6a: rebuilt, 102636 bytes written/read back, GO OK.
Twenty stationary IMU queries all reported READY/CAL=200/F=0, AGE=18..23 ms,
no backlog or restart. Displayed yaw changed +0.129 degree over 19.328 seconds.
ACC_WARN=1 remains (mean AZ=21874); accelerometer magnitude is not calibrated.
One initial unstable acceleration window was rejected before successful bias
calibration. No physical turn/ground accuracy test. K210 unchanged.
See `docs/history/deployments/DEPLOYMENT_B326A6A_20260909.md`. V12/V11 failure
statements below are historical.

Newest: V12 c177c3d supersedes V11 below. Build, 102148-byte readback and GO
passed. Five stationary IMU queries showed CAL=0, REJECT=4: Z acceleration
21806..21940 exceeds calibration maximum 18500. Fresh FIFO data with no backlog.
Gyro Y=-222..-217 also exceeded the old +/-196 threshold; V12 fixed that barrier,
but Z rejection still prevents READY. Actual offset/scale/acquisition cause of
the Z reading is not yet established. K210 unchanged; no motion test.
Evidence: `docs/history/deployments/DEPLOYMENT_C177C3D_20260909.md`. Old
deployment statements are history.

Latest: comprehensive V11 `fda63ce` has replaced V8 on STM32. Exact build,
99488-byte full readback and GO passed; audio and calibration pages preserved.
User-requested STOP-only IMU checks found communication and fresh samples but
repeated calibration failure: `CAL=0`, `LAST_F=5`, restart count 1 then 4.
IMU is NOT READY; zero yaw must not be treated as valid. The failed raw-axis
condition is not exposed by current telemetry, so the physical cause remains
unconfirmed. K210 unchanged; no motion test. Details:
`docs/history/deployments/DEPLOYMENT_FDA63CE_20260909.md`. Older V8 deployment
statements below are history.

Follow-up: user reports flat, component-side-up, stationary mounting. Repeated
STOP-only queries still showed CAL=0 and calibration-timeout restarts (41).
Explicit `c` was accepted (`IMU CAL START: KEEP STILL`); five subsequent `g`
samples again showed CAL=0, fresh AGE=8..13 ms, no backlog, no transport fault.
Thus a stale latched calibration state is not the explanation. Raw acceleration
and gyro samples are not exposed by this firmware, so no exact rejected axis is
known. Inspect raw acceptance limits before changing hardware: absolute gyro
raw >196 (about 3 deg/s before bias removal), accel orientation and sample span
all reset progress. No firmware changes or flash in this follow-up.

Subsequently, the user requested the modes 3/4 K210 program. COM14 identified
CanMV_Yahboom 2.1.1 with GC2145. The existing `/sd/main.py` (7256 bytes) already
matched V8 `c767baa`'s `K210/sign_mode34.py`; it and `/flash/main.py` were backed
up before the requested rewrite. Full script readback matched SHA-256
`2BCFCC5E08671EE0F0E3BD0712A1DD217A3450BFDBD3C3DDA7EFE8807D38A3D8`.
The existing 571432-byte `/sd/KPU/road_sign_det/road_sign_det.kmodel` matched
`B472A5C45FBB2060CD794BEC7C972D9F58FB40D7DCA27DFE6545125B8E02B901`
on-device and was not rewritten. Soft reboot reported SD mount OK, model load
success and `SIGN34 ready`, threshold 0.20, vflip/hmirror 0/0. Backup, manifest
and startup log: `F:/myproject/jidian/validation/k210-sign34-20260909/backup-20260909-092318`.
STM32 was not accessed in this K210 deployment. No board-link or motion test
was performed; the active K210 application supports modes 3/4, not mode 5.

At the user's explicit request, the STM32 now runs temporary comprehensive V8
source `c767baa158a25cbf411aae6c1dadb5b631a2e51e`. Its exact clean worktree
was rebuilt (99460-byte BIN), then COM11 selectively erased 49 application
pages, wrote all bytes, passed full readback `VERIFY OK` and completed
`GO OK: 0x08000000`. Audio memory and calibration pages were outside the
erase/write range. No post-GO zero-output query, wheel test or ground test was
performed. K210 was untouched. Main firmware and its candidate artifacts remain
rc.4; V8 was not merged. See
`docs/history/deployments/DEPLOYMENT_C767BAA_20260909.md`.
All board-source statements about `4589a25` in the older sections below are
historical and superseded by this entry. These deployment records are local;
no GitHub push was requested in this turn.

Since 2026-09-07, `main` is the only canonical integration and deployment
branch, checked out at
`F:/myproject/jidian/project/test-exp7-unified-motion-v1`. Future feature,
fix and test worktrees start from current `main` and return commits here for one
review/build/deployment pass. Existing worktrees and branches remain preserved
as history; they are not alternate definitions of “latest”. The exact workflow
and temporary historical-image exception are documented in
`BRANCH_WORKFLOW.md`.

GitHub PR #10 merged the generic DFPlayer controls and track memory into
protected `main` as `65e4cce`; PR #11 merged the rc.4 release notes as
`ff4bbc9`. The active ruleset still requires a pull request and protects
against deletion and non-fast-forward updates; its required approving review
count was zero. No ruleset was bypassed or disabled.

Pre-release `v1.2.0-rc.4` targets `ff4bbc9` and publishes its exact STM32
BIN/HEX, a SHA-256 list, both mutually exclusive K210 `/sd/main.py` choices,
the mode 3/4 road-sign model and instructions. All nine uploaded asset sizes
and GitHub digests match the staged local files. K210 assets are byte-identical
to rc.3. The exact rc.4 main BIN has not been flashed: the board still runs
comprehensive test source `4589a25`. K210 still runs the readback-verified
SIGN34 mode-3/4 program and matching model. No new listening, lifted-wheel or
ground-test claim is made. Previous releases remain available as history.

## Current protected main source (`ff4bbc9`)

Functional commit `7dbcb84767f2ea290568357c0461ef5b6a57a7a7`, merged by PR #10
as `65e4cce`, replaces filename-specific audio selection with DFPlayer physical
order previous/next commands. Direction-pad centre now toggles play/pause,
left/right select previous/next, and up/down adjust volume by two. The default
volume is 20/30 and remains adjustable to 30/30. The current file continues to
loop; STOP, mode changes and safety alerts retain audio preemption.

The latest physical track number is appended to the dedicated STM32F103ZE page
`0x0807F000`; the final motor/turn calibration page at `0x0807F800` remains
separate and untouched. Same-power-session pause resumes from the exact time
position. After full power loss, the remembered physical track restarts at its
beginning because the serial protocol does not provide reliable millisecond
position persistence. Whole-chip erase clears both reserved pages.

The exact post-release-note main `ff4bbc9` ARM build passed with text/data/bss
`90312/64/11728`, producing a 90380-byte BIN with SHA-256
`56A9FD8D9DFA5CF1903C7608EE1552919B66D5213AC3FABAB3AD0BCC346408D8`;
the HEX SHA-256 is
`3965EE4492E87AB0C90BEDA37BB7287055FF310D6523FC3343EEF35B6F9E6D04`.
DFPlayer protocol/driver/Flash-store tests and all line-recovery, sign-line and
vision-line-v4 regressions passed. PR #11 merged the release note as `ff4bbc9`.
This exact firmware has not been flashed or physically tested. Release notes:
`docs/releases/RELEASE_V1.2.0_RC4.md`.

## Previous protected main source (`f543639`)

Commit `f54363971bc3111acdda33fbed91882a71305bc7` retains the accepted
continuous obstacle-bypass travel from `453cad2` and promotes the current
tested line controller in two dependency-ordered commits. `6a4ed26` is the
main replay of `64f489e`: brief sole-outer evidence uses a 0/2200-CPS forward
pivot, adjacent pairs retain mirrored 1412/2400-CPS forward arcs, all-white
loss searches in the remembered direction, and automatic loss/rejoin remains
silent. `f543639` replays the final `f34e7dd` increment: 120 ms of uninterrupted
sole-outer evidence escalates to mirrored -2200/+2200-CPS powered correction,
which is cleared by any other raw mask, a sampling gap above 30 ms, reset,
queue overwrite or explicit zero cap/STOP.

The line source and tests are byte-equivalent to the line portion of the
currently flashed `f34e7dd` composite. The independent ten-second sign-probe
hold `4c345b8` was deliberately excluded and remains under test. Complete
line-recovery/load/bypass tests passed at both configured search speeds;
unchanged sign-line and vision-line-v4 tests also passed. The formal ARM build
passed with text/data/bss `84208/64/11592`, producing an 84276-byte BIN with
SHA-256
`36F1FB5C78483CA09B0B33A7ACE55C89713CFC6487E52598464D235FC7A384D6`;
the HEX SHA-256 is
`D63E0222BE881FC775BC18A9D2C50D0FBE514F25A3E1586F6ACBD1DF50840B3F`.
PR #8 merged this source and its release preparation as `4a343dd`. This exact
main BIN has not been flashed or physically tested. Release notes:
`docs/releases/RELEASE_V1.2.0_RC3.md`.

## Previous local main source (`453cad2`)

Commit `453cad2cffcfbb4b8494b36cbdca8fc48c620476` promotes only the latest
obstacle-bypass update from worker `68c6444` onto canonical local `main`. Short
forward/reverse bypass segments no longer enter the low-speed endpoint-pulse
region of the shared position controller. They use the new four-wheel
`line_bypass_travel` owner, retain same-direction continuous drive to the
encoder distance target, and cap the segment at 1800 CPS. STOP/mode ownership,
all-wheel stall checking, progress timeout and infrared-boundary interruption
remain active. Existing continuous bypass turns and their 2500-CPS command are
unchanged.

Only the bypass module, its new travel module and focused tests changed. The
experimental line commit `64f489e` and sign commit `4c345b8` remain solely on
the comprehensive test branches. Complete line-recovery/load/bypass tests
passed at both speed configurations; sign-line and vision-line-v4 also passed.
The formal ARM build passed with text/data/bss `83824/64/11584`, producing an
83892-byte BIN with SHA-256
`5757D92BC4B2741AE23908A0D494B5DD36757281F36AA9CC60766F9E46FB5E63`;
the HEX SHA-256 is
`E5FE872642E0D49DD66271D2778FE624026DBA4EEF3E938F0B7D2762C648B8CE`.
This local main integration was not flashed or pushed. Details:
`docs/history/candidates/PREPARED_MAIN_BYPASS_CONTINUOUS_453CAD2_20260908.md`.

## Previous published main source and deployment (`3170221`)

Commit `317022123395016354249c3c7bc9e9b26c02ff7e` integrates the requested
mode-1 obstacle-bypass power profile into canonical `main`. It raises the
ultrasonic slow and reverse commands from 2200 to 2600, and the legacy
ultrasonic turn pair from 2300/2800 to 2800/3300. The active line-bypass
configuration is now explicitly set to 1900 CPS reverse, 2600 CPS forward,
2200 CPS clear probe, 2300 CPS return and 2500 CPS turn. Modes 2--5 and the
motor polarity mapping are unchanged.

The complete line-recovery/bypass suite passed at both configured search
speeds, including four-wheel DriveBase, obstacle-bypass and STOP-ownership
checks. The formal ARM build passed with text/data/bss `84384/64/11536`, BIN
size 84452 bytes, BIN SHA-256
`14B484C5091A3549B24360236F941A61CC12688D34900427D73EFAC2D181E9C0`
and HEX SHA-256
`D30049A841C4F9C43F2925C118CDA70C675E45D0B2774D87C94D734549BF7AA8`.
COM11 programmed this exact 84452-byte BIN with 42-page selective erase,
reserved-last-page preservation, full readback `VERIFY OK` and
`GO OK: 0x08000000`. STOP telemetry before and after programming showed mode 0
and zero target/measured/PWM output on all four wheels. K210 was not opened,
reset or rewritten. No lifted-wheel or ground test was performed. Detailed
evidence:
`docs/history/deployments/DEPLOYMENT_MAIN_MODE1_POWER_3170221_20260908.md`.

## Previous flashed comprehensive test (`0a02d3d`)

Exact source `0a02d3d0533fd93df468d91624acbbf245548714` adds requested worker
`a14aaff` to the previous comprehensive test in branch
`test/comprehensive-v4-20260908`. KEY1 and KEY2 now call one shared tracking
entry and one-snapshot compute/apply cycle. Both start with smooth tracking,
100-percent middle steering gain and search instead of blind forward travel
before the first line. KEY1 still applies ultrasonic speed caps and obstacle
ownership before the shared line cycle; KEY2 retains its full limit. The
ten-second sign hold, persistent outer correction and continuous bypass remain.

The complete line-recovery/load/bypass suite passed at both search speeds. A
real-source replay matched 3600 KEY1/KEY2 samples and source checks retained
bypass/ultrasonic priority. Sign-line including the ten-second hold and
vision-line-v4 also passed. The formal build passed with text/data/bss
`84404/64/11600`, producing an 84472-byte BIN with SHA-256
`BE44622428EC5BB8A216F190773D16DE0FA153AC92E1EF4E051AB7AD346E0E92`;
the HEX SHA-256 is
`3D61C879E7920D8CF37BBF47547E2E6790E773FF1168CC9E0F0BB8D6BF52833E`.

COM11 selectively erased 42 application pages while preserving the final
calibration page, wrote and read back all 84472 bytes with `VERIFY OK`, and
completed `GO OK: 0x08000000`. No routine post-GO serial check was performed.
COM14's prior 3891-byte `/sd/main.py` was backed up and replaced with the
correct 7256-byte SIGN34 script. Its readback SHA-256 is
`2BCFCC5E08671EE0F0E3BD0712A1DD217A3450BFDBD3C3DDA7EFE8807D38A3D8`.
The existing 571432-byte road-sign model already matched SHA-256
`B472A5C45FBB2060CD794BEC7C972D9F58FB40D7DCA27DFE6545125B8E02B901`
and was not rewritten. Soft reboot reported `model load succeed` and
`SIGN34 ready`. No board-link, lifted-wheel or ground-driving test was run.
Detailed evidence:
`docs/history/deployments/DEPLOYMENT_COMPREHENSIVE_V4_0A02D3D_20260908.md`.

## Previous flashed comprehensive test (`f34e7dd`)

Exact source `f34e7dd3fa1f16f7d8abd0255c94a25d199cbb63` extends the previous
comprehensive source in branch `test/comprehensive-v3-20260908`. Its parent
`8c7070b` is byte-equivalent to `2c2ed97` across `Core`, `tests` and `K210`, so
the only new functional increment is persistent sole-outer-sensor correction.
A brief outer-only contact keeps the existing 0/2200-CPS forward pivot. If the
same outer remains continuously valid for 120 ms, it escalates to mirrored
-2200/+2200-CPS powered counter-rotation. A different raw pattern, sample gap
above 30 ms, queue overwrite, reset or explicit zero cap/STOP cancels it.

The complete line-recovery/load/bypass suite passed at both configured speeds,
including the 119/120-ms boundary, 60 mirrored interruptions, ISR/queue/timer
cases and real four-wheel DriveBase signs. Sign-line with the ten-second hold
and vision-line-v4 also passed. The formal build passed with text/data/bss
`84420/64/11600`, producing an 84488-byte BIN with SHA-256
`D44DF291F6A49176E67D76BD16D6B1356B07D919603580005318D74BA47ED54A`;
the HEX SHA-256 is
`0D4F79D56ACB603B9D705F44A4C445C2D519244686135A683E81962ED4138414`.

The first COM11 attempt entered the ROM bootloader and preserved the final
calibration page while erasing 42 application pages, but Windows denied COM11
access before a complete write. After immediate CH340K re-enumeration, the
retry erased the same bounded region, wrote and read back all 84488 bytes with
`VERIFY OK`, and completed `GO OK: 0x08000000`. No routine post-GO serial
check was performed at the user's request. K210 was not accessed, and no
lifted-wheel or ground test was performed. Detailed evidence:
`docs/history/deployments/DEPLOYMENT_COMPREHENSIVE_V3_F34E7DD_20260908.md`.

## Previous flashed comprehensive test (`2c2ed97`)

Exact source `2c2ed973ef5e34705f301c16871aed043e9ad461` was assembled from
current canonical `main` (`f17bffa`) in independent branch
`test/comprehensive-v2-20260908`. It applies candidate `64f489e` once, followed
by `4c345b8` and `68c6444`, producing replay commits `c82c8b3`, `0a6a0f3` and
`2c2ed97`. It therefore combines slower and silent line-loss recovery,
zero-target-side turn assist, a ten-second sign-probe hold and slow continuous
obstacle-bypass travel. Because `64f489e` already contains its line-history
prerequisites, `c5a4c76` and `074ef682` were not replayed separately. Starting
from current main retains mode-1 power source `3170221`; the later continuous
bypass layer intentionally caps its short travel commands at 1800 CPS.

The complete line-recovery suite passed at both speed configurations,
including 120 visible cases, silent recovery, explicit 0/2200 CPS outer
pivots, moving-side assist with the stopped side at zero, strong exits,
overlaps, continuous bypass travel, four-wheel load/fault checks and STOP
ownership. Sign-line passed with the real ten-second hold, and vision-line-v4
also passed. The formal ARM build passed with text/data/bss
`84148/64/11592`, producing an 84216-byte BIN with SHA-256
`D53897200AD5BD1C0913028446D2C4C2A2B667ABE32FBEAA9AC3B6849DF4815E`;
the HEX SHA-256 is
`DDADBDBDAD506AF344834504035875B213F2B70CDDB1597E1189533135BD2F74`.

COM11 programmed and read back all 84216 bytes after selectively erasing 42
application pages while preserving the final calibration page. The programmer
reported `VERIFY OK` and `GO OK: 0x08000000`. A pre-flash STOP showed mode 0
and zero four-wheel output. At the user's request, the routine post-GO serial
STOP/zero-output check was omitted. K210 was not accessed. No lifted-wheel or
ground test was performed. This source remains an unmerged local test branch
and was not pushed during this operation. Detailed evidence:
`docs/history/deployments/DEPLOYMENT_COMPREHENSIVE_V2_2C2ED97_20260908.md`.

## Previous flashed comparison candidate (`c5a4c76`)

Exact source `c5a4c76ab84666f44d03510227bfbeac41083641` was rebuilt in the
isolated `fix/line-visible-arc` worktree after publishing rc.2. Visible line
evidence now commands a four-wheel forward steering arc; counter-rotation is
reserved for confirmed line loss. The branch also contains strong-exit and
ordered-overlap recovery cases.

The full line-recovery suite passed at both speed configurations, including
120 visible-mask/gain/state cases, forward arcs, confirmed-loss rotation,
four-wheel rejoin, bypass and STOP ownership. The ARM build passed with
text/data/bss `84568/64/11536`, BIN size 84636 bytes, BIN SHA-256
`BE9820DCB9F9673652F18636CBECA4B9BAAC344577796327873BA86C9429B6E2`
and HEX SHA-256
`F89A8BDF6D6F31A08E60F16263497859BA30A3F4DBB47E814E0F33975018686F`.
Prepared artifacts are under `manual-build-candidate-c5a4c76/`.

This exact comparison commit is not merged and not included in rc.2. Its parent
predates `3170221`, so it does not contain the new mode-1 power profile. COM11
programmed all 84636 bytes with selective last-page-preserving erase, full
readback `VERIFY OK` and `GO OK`. STOP telemetry before and after programming
showed mode 0 and zero four-wheel outputs. K210 was not accessed. No lifted or
ground test was performed. Details:
`docs/history/candidates/PREPARED_C5A4C76_20260908.md` and
`docs/history/deployments/DEPLOYMENT_C5A4C76_20260908.md`.

## Previous flashed temporary test image (`074ef682`)

The STM32 previously ran exact source
`074ef6827016a9931d8de7f06ab3a35ae291dba2` from
`fix/line-strong-evidence`. This temporary comparison commit is based directly
on `origin/main` at `eee8773`; it is not merged into canonical `main`. It keeps
pending strong outer evidence through an intervening inner-only observation
and recognizes ordered nonadjacent overlaps during line recovery.

The complete line-recovery suite passed at both 2493 and 1870 CPS, including
the new strong-exit and ordered-overlap cases and real four-wheel DriveBase
checks. The formal ARM build passed with text/data/bss `84560/64/11544` and
produced an 84628-byte BIN with SHA-256
`5C33568DC43365B5DFEB319C4D66A7873B1547EA2BFEE1B455EA189690287BE2`;
the HEX SHA-256 is
`BE02E8937D66FB7665AD37974353E7348585969955019D108CD0A105113E8BBF`.

COM11 was enumerated as USB-SERIAL CH340K immediately before programming. A
STOP command first produced mode 0 and zero four-wheel targets, measurements
and PWM outputs. Selective erase covered 42 application pages and excluded the
reserved final calibration page. All 84628 bytes were written and read back
with `VERIFY OK`, followed by `GO OK: 0x08000000`. A second STOP after GO again
showed mode 0 and zero four-wheel outputs. K210 was not opened, reset or
rewritten. No mode start, lifted-wheel test or ground test was performed.
Detailed evidence: `docs/history/deployments/DEPLOYMENT_074EF682_20260908.md`.
Rollback: `rollback/2026-09-08-before-074ef682-test` -> `36551f3`.

## Previous flashed temporary test image (`4947f9c`)

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

## Previous integrated canonical release (`v1.2.0-rc.1`)

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
model remain the previously verified `07f2b73` pair. This deployment rewrote
and read back the script; the already matching model was hashed on-device and
was not rewritten.

The line-recovery/load/bypass suite passed at both 2493 and 1870 CPS, including
all 16 masks, 256 mirrored ordered pairs, queued-sample ordering, X1/X3 mirrored
encoder probes and real DriveBase four-wheel targets. The sign-line and visual
line v4 suites also passed. The formal ARM build passed with text/data/bss
84344/64/11536 and produced an 84412-byte BIN with SHA-256
`5C7F43ACCEE850E30F439121254E1FCA4085CFAAC11C6CE10423463070777CCB`;
HEX SHA-256 is
`2C65AB052DB5E55AEADC4ECADE21B766AE53483CE46F868108C6E21D8693815F`.
On 2026-09-08 COM11 programmed this exact 84412-byte BIN with 42-page
selective erase, last-page preservation, full readback `VERIFY OK` and
`GO OK: 0x08000000`. COM14 backed up and rewrote the exact 7256-byte SIGN34
script, verified the unchanged model on-device and observed `SIGN34 ready`.
A STOP-only board-link query saw 24 new parsed SIGN frames while `DRV M=0` and
all wheel targets, measurements and PWM outputs remained zero. No mode start,
lifted-wheel or ground test was performed. Detailed evidence is in
`docs/history/deployments/DEPLOYMENT_V1.2.0_RC1_MODE34_20260908.md`. Rollback tag
`rollback/2026-09-08-before-v1.2.0-rc1-flash` points to prior board source
`4947f9c`.

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

## Integrated continuous KEY1 bypass travel

- Commit `453cad2`, cherry-picked from `68c6444`, replaces only the short
  forward/reverse translations used by KEY1 obstacle bypass. It does not change
  KEY2 line tracking or modes 3--5.
- All four wheels remain in DriveBase speed mode at the same signed target
  until encoder travel reaches 20 or 40 mm. The command is capped at 1800 CPS
  so the old per-wheel position-pulse tail and reversal correction are never
  entered.
- A close infrared boundary interrupts forward travel immediately. Invalid
  infrared input, a stalled wheel, missing progress, STOP or mode change still
  ends ownership safely or raises the existing bypass fault.
- Encoder distance establishes bounded wheel travel, not guaranteed chassis
  displacement; physical clearance and traction remain ground-test items.

## Current main mode map (`f543639`)

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

The latest integrated controller retains the ambiguity handling for a
repeatable approach angle that produces only X1 or X3, then layers the current
visible-edge and persistent-error behavior through `f543639`. KEY1/KEY2 behave
as follows:

- On ordinary line loss it rolls directly into continuous rotation in the most
  recent reliable direction; without a hint it defaults left. Search uses equal
  and opposite 2493-CPS wheel groups. It no longer retreats, returns to encoder
  start counts, or stops for distance, attempt or time budgets.
- A lone inner X1/X3 observation is not a reliable curve direction. If loss
  follows within 200 ms, it starts a probe toward that side, then reverses after
  four-wheel encoder travel corresponding to 30 degrees. Each new sweep grows
  by 30 degrees up to 120 degrees; later sweeps alternate at that bound. Time
  and battery level alone cannot reverse the probe.
- Automatic search and rejoin remain silent. The infrared-remote or serial
  one-shot preset phrase is independent. A middle X1/X3 line hit, excluding
  the both-outer wide-line case, must confirm for 4 ms before capture.
- Confirmed capture enters at least 500 ms of low-speed line following and
  requires stable middle-line evidence for 80 ms before normal speed resumes.
  Losing the line during capture restarts same-direction rotation and audio.
- Outer-only and all-four-black input cannot directly complete capture. Manual
  STOP, mode change or zero base-speed still stop motion and cancel the phrase;
  power-on still enters STOP.
- During normal tracking or low-speed capture, a brief single outer sensor
  commands a 0/2200-CPS forward pivot, while an adjacent same-side pair uses a
  mirrored 1412/2400-CPS forward arc. If the same sole outer remains valid for
  120 ms with sample gaps no greater than 30 ms, both sides counter-rotate at
  2200 CPS. Any different raw mask immediately withdraws that escalation.
- While that turn is locked, any outer sensor still seeing black keeps the
  counter-rotation active. Only when both outer sensors are white and at least
  one middle sensor remains black for 20 ms does braking and stationary capture
  begin. A failed 80 ms stationary confirmation resumes the same turn.
- The old timed sharp-corner phases and high-speed weak forward arc remain
  removed. Visible persistent-edge correction uses 2200 CPS; actual all-white
  loss uses the configured search target and remembered direction. Ordinary
  shallow-curve steering and normal straight speed are unchanged, and online
  corner entry does not start the buzzer.
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

- External DFPlayer audio uses J8 UART4 at 9600 8N1: PC10/TX drives module RX
  through the documented series resistor and PC11/RX receives module TX.
- Direction commands are centre `0x05` play/pause, left `0x04` previous,
  right `0x06` next, up `0x01` volume +2 and down `0x09` volume -2. Commands
  do not change the driving mode, and NEC repeat frames remain suppressed.
- Previous/next use native physical-file-order commands and do not constrain
  filenames or folders. UART feedback corrects the stored physical index when
  available. Replacing/reordering the card can therefore change which audio a
  remembered index identifies.
- Default volume is 20/30; the remote can adjust it through the full 0..30
  range. Current-file looping remains enabled.
- Track records use an append-only, power-loss-safe log at `0x0807F000`.
  Full-page erase is deferred until vehicle STOP. Linker FLASH length is 508K,
  reserving that page and the separate `0x0807F800` calibration page.
- Same-session pause resumes at the time position. A complete power cycle
  restores the last track from its beginning; exact time offset is not stored.
- Existing stop/fault, encoder, ultrasonic and bypass warning arbitration can
  immediately cancel the lower-priority DFPlayer audio. The board PG12 phrase
  module remains available for safety and diagnostic rhythm output.
- The user reported that the prior no-audio episode likely followed TF-card
  hot insertion without power-cycling the module. That diagnosis is plausible
  but is not promoted to a new physical pass result. After changing the card,
  the DFPlayer itself must be fully power-cycled.

## Confirmed hardware facts

- All four encoders are repaired and usable as AB quadrature inputs.
- M2 is mapped to PA15/PB3; do not restore the obsolete fallback.
- Wheel order is M1 left-front, M2 left-rear, M3 right-front, M4 right-rear.
- Motor direction compensation remains centralized in `Core/Src/motorPWM.c`.
- K210 is connected as COM14 and currently runs the exact 7256-byte SIGN34
  script present in `0a02d3d` (the same optimized blob introduced by
  `07f2b73`). The prior 3891-byte active script and unchanged `/flash/main.py`
  are backed up. The mode 5 slow-near script remains preserved separately.
  UART1 IO8/TX to STM32 USART2 PD6/RX and common ground are unchanged; this
  deployment did not rerun the board-link query.
- OLED is the external J12 display and includes battery/status information.
- Encoder distance/angle is a wheel-motion estimate; ground yaw requires
  calibration because slip and battery/load change the result.

## Evidence ledger

| Evidence level | Current result | Scope |
|---|---|---|
| computer build/link | passed | exact current main `ff4bbc9`; ELF text/data/bss = 90312/64/11728 bytes; BIN is 90380 bytes; HEX hash is `3965EE44...6D04` |
| host regression | passed | current main DFPlayer protocol/queue/RX/play-pause/generic previous-next/Flash-store tests; complete line suite at both speeds; sign-line and vision-line-v4 passed |
| STM32 flash/readback/GO | unchanged, older image passed | board remains on `4589a25`; its 86636-byte BIN was previously written/read back with `VERIFY OK` and `GO OK`; rc.4 was not flashed |
| GitHub main candidate release | passed, pre-release | `v1.2.0-rc.4` targets protected-main `ff4bbc9`; exact main BIN/HEX, checksums and unchanged mode-3/4 and mode-5 K210 assets are published; all nine sizes and GitHub digests were verified |
| K210 deployment/runtime | passed | COM14 backed up the prior script, wrote/read back the 7256-byte SIGN34 `/sd/main.py`, verified the existing model hash and observed `model load succeed` plus `SIGN34 ready` after soft reboot |
| board-to-board UART | unchanged from prior STOP-only result | no new link query was run; the earlier four VLINK samples increased parsed SIGN count 102 to 126, but this deployment's physical IO8/TX to PD6/RX path remains untested |
| wheels off ground | not performed | programmer success does not establish search reversal, mode 5 steering, UART-loss stop or operator STOP response |
| ground driving | not performed | composite line recovery, continuous bypass, mode 3/4 routing and mode 5 curve tracking remain unverified |

## Current open issue and next safe step

The STM32 still runs temporary comprehensive DFPlayer firmware `4589a25`; its
write/readback/GO passed previously, but listening remains unrecorded and no
new wheel or ground result is claimed. K210 COM14 still runs the
readback-verified SIGN34 mode-3/4 script and matching model. This task did not
open either serial port or modify either board.

GitHub PR #10 merged the requested generic audio controls and track memory;
PR #11 merged its release note. Pre-release `v1.2.0-rc.4` is published with
nine size/hash-verified assets from current main `ff4bbc9`. That exact STM32
firmware is compiled but not flashed.

The next audio test, only after a separately authorized flash, is to fully
power-cycle the DFPlayer after inserting its TF card, then verify centre
play/pause/resume, left/right previous/next, volume, single-track loop and a
second full power cycle restoring the last selected track. Exact time-position
resume is expected only within one power session.

Modes 3/4 require `k210-mode34-sign-main.py` as K210 `/sd/main.py` plus the
packaged road-sign model. Mode 5 instead requires
`k210-mode5-visual-line-main.py` as `/sd/main.py`; both cannot be active under
that filename at once. Test mode 3/4 exit withdrawal and mode 5 stale-UART stop
separately. If this comprehensive image is rejected, rollback tag
`rollback/2026-09-08-before-comprehensive-v3-f34e7dd` identifies the replaced
source `2c2ed97`. No ground behavior is established by host tests, build,
programmer readback, startup text, UART counters or GO success.

## Update protocol

After integrating code, flashing, or receiving a physical result, the
coordinator must update the applicable fields and evidence row. Keep old facts
in Git history instead of accumulating a long diary here. Never mark a physical
test passed from a successful compilation, programmer verification, OLED text,
or encoder counts alone.
