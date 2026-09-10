# Shared project state

Current authority: canonical main at this checkout. Historical journals were
removed from this live file during four-mode consolidation; all prior text
remains in Git (rollback/2026-09-10-before-main-four-mode) and docs/history.
Do not infer the active mode map from older experiment documents.

## Machine-readable snapshot

```text
state_schema_version: 1
state_updated_at: 2026-09-11
integration_branch: main
repository_head_at_update: 2b4fc1f27b9b405be48ce3e2c6d4f9212f5ff85e
latest_code_commit: 2b4fc1f27b9b405be48ce3e2c6d4f9212f5ff85e
candidate_source_commit: 2b4fc1f27b9b405be48ce3e2c6d4f9212f5ff85e
candidate_bin_size_bytes: 132156
candidate_bin_sha256: 8C14B8FB10ED8341DD4A0A3FBBE6AC44EF826AA2482624730BCECA7847D872EA
candidate_hex_sha256: 5E3E41992193398B1444A1939297C41B844BE4974C3EE2178726E09CD5D10C2C
candidate_status: main_2b4fc1f_verified_flash_blocked_USB_absent
formal_bin_path: manual-build-unified-motion/exp7_unified_motion.bin
formal_hex_path: manual-build-unified-motion/exp7_unified_motion.hex
formal_bin_size_bytes: 121068
flashed_source_commit: 62b0de2e121cbe199b29c599f32779b7d6881423
flash_record_commit: 6ce2702ee6b46c98f4aeaf011bc3d192f74ffadc
deployed_tag: deployed/2026-09-10-62b0de2
flashed_bin_sha256: 03B838FC37450E47146746219B0B86D1C65C077BAE971CB856D25530631FD3AB
flashed_hex_sha256: 75F6EEA70FBFF97312F168B5042B52B92BEB84CB5E3687D6EAC5C5C4CE573B3B
ground_test_status: new_four_mode_main_not_flashed_or_physically_tested
user_reported_flash: tool_verified_62b0de2_readback_GO
stm32_runtime_status: COM11_62b0de2_full_readback_GO_no_post_GO_query
comprehensive_candidate_source_commit: 62b0de2e121cbe199b29c599f32779b7d6881423
comprehensive_candidate_status: explicit_five_mode_test_COM11_full_readback_GO_passed
comprehensive_remote_branch: test/comprehensive-v15-sign-horn-20260909
comprehensive_candidate_bin_size_bytes: 121068
comprehensive_candidate_bin_sha256: 03B838FC37450E47146746219B0B86D1C65C077BAE971CB856D25530631FD3AB
comprehensive_candidate_hex_sha256: 75F6EEA70FBFF97312F168B5042B52B92BEB84CB5E3687D6EAC5C5C4CE573B3B
temporary_flash_selector_commit: 62b0de2_five_mode_board_not_four_mode_main
k210_status: unchanged_last_verified_COM14_20e8c72_SIGN34_threshold_0_15
k210_candidate_source_commit: a217df81e807f54a7b95fc8c8541876cdc59bfc8
k210_candidate_status: paired_script_in_K210_sign_mode34_py_for_new_mode3_only
remote_sync_status: four_mode_main_merged_PR19_release_rc6_published
remote_sync_branch: integration/main-four-mode-20260910
github_release_tag: v1.2.0-rc.6
github_release_source_commit: ce503cae2875bacc6fa79a7b9032f41fef35c298
github_release_firmware_commit: c2ed15dc77c65f1cd6ee22833d460565a202ae3e
github_release_status: prerelease_published_7_assets_size_SHA256_verified_not_flashed
```

The repository anchor is the latest firmware commit. Newer documentation-only
commits are expected. The formal/candidate files in main are NOT the older
flashed image: candidate hashes and flashed hashes are intentionally separate.

## Current four-mode mapping

Latest local main2b4fc1f merges6594ff0 (notc9df841) without conflicts.
Mode3 now drives forward immediately along the heading at the configured exit
angle, no intermediate alignment turn. Fresh post-entry selected outer
clear/black alone finishes; no route time/distance limit during this forward
phase. Stale IMU removes trim, not forward motion. Manual STOP remains essential.
Full promoted sign suite, mode1/2 integration, source scope and ARM build passed.
User authorized main flash, but fresh enumeration found only Bluetooth ports;
programmer not opened and no erase/write. Reconnect before retrying main2b4fc1f.
Board remains five-mode62b0de2, K210 unchanged. No push or release update.
Rollback: rollback/2026-09-11-before-6594ff0 ->35a78bb.

Explicit test-only update: user requested62aeda3 into the retained five-mode
comprehensive branch, merged as62b0de2. Mode3 exit completion requires fresh
post-entry outer clear/black evidence. Full sign suite and ARM build passed,
BIN121068 with hashes above, artifacts in comprehensive-v15-sign-horn worktree.
Rollback: rollback/2026-09-10-before-62aeda3 ->a217df8. Subsequently flashed:
COM11 full121068-byte readback and GO passed, reserved pages excluded;
no post-GO query/physical test/push/K210 access.
This increment is NOT promoted to four-mode main or rc.6; board now62b0de2.
Only this status record changed in canonical main, not its firmware.

| Input | Controller source | Behavior |
|---|---|---|
| 0 / power-on | STOP latch | Explicit command required to move |
| 1 | Main before consolidation (36f4aed) mode1 | Legacy line + adaptive bypass |
| 2 | Main before consolidation (36f4aed) mode2 | Legacy four-line only |
| 3 | Comprehensive a217df8 mode3 | Line + K210 sign/IMU route |
| 4 | Comprehensive a217df8 mode5 | Fast four-line + fixed bypass |
| 5 | Unassigned | Keeps current mode; use0 to stop |

Internal AppMode is zero-based0/1/2/3 and STOP4. The former main SL2 mode4
and old visual mode5 are not reachable. Archived modules/tests may retain
historical mode5 names; they do not define another active mode.

Mode3:20-percent observation, white search1800 CPS to either middle sensor,
then2-second observation. Signed exit threshold boot40 degrees; dedicated
remote +/- changes5 degrees, clamped30..90. RAM tuning survives mode reset,
not reset/power loss. Selected outer clear-to-black completes exit. Horn sign
plays once. OLED shows E setting, cached IMU state, H heading and route phase.

Mode4: fast straight and wide-line cap3600 CPS; fixed bypass250/300mm and
45-degree inward return,4000-CPS fixed travel, rolling handoffs. Front sonar
16cm base/22cm dynamic maximum/28cm clear. Side obstacle IR disabled here.
Mode1 retains original IR and sonar configuration via isolated profile restore.

## Source isolation / invariants

- Existing line_tracking/line_recovery and mode1/2 drivers retained.
- Promoted_* / promoted_* modules are mechanical namespace copies of a217df8
  controllers, only selected by modes3/4; one shared hardware sampler/DriveBase.
- tests/check_mode35_scope.py verifies source identity, immutable old driver
  baselines and critical mode1/2 app paths. Its filename predates renumbering.
- Motor direction mapping unchanged. Encoders M1 LF, M2 LR (PA15/PB3), M3 RF,
  M4 RR are repaired AB quadrature. No single-edge fallback.
- Encoder travel is wheel motion, not exact chassis yaw. Slip/load still matter.
- Preserve audio memory0x0807F000 and calibration0x0807F800. No mass erase.
- IR remote direction pad retains play/pause, previous/next and volume controls.
- Default STOP; current request must explicitly authorize any hardware flash.
  No routine post-GO zero-output query and no unrequested movement.

## Verification and deployment boundary

Latest main firmware: ARM build text/data/bss132068/84/27056; BIN132156 bytes.
The full consolidation regression listed below refers to c2ed15d; latest
mode3 increment additionally passed promoted sign, scope and mode1/2 checks.
Passed legacy line suite (both speeds), promoted line suite (both speeds),
real promoted DriveBase, sign route/observation/IMU tests, fixed-bypass/profile
and actual selector tests, DFPlayer/store tests, source scope and K210 runtime.
The selector tests cover1..4, ignored5, STOP precedence and dedicated +/-.
No STM32/K210 serial access, flashing, lifted or ground tests in consolidation.
Board now62b0de2 with the OLD five-mode mapping, not this new main.
Latest board evidence: docs/history/deployments/DEPLOYMENT_62B0DE2_20260910.md.

## K210 / release

K210/sign_mode34.py is the paired script from a217df8, deployed filename
/sd/main.py; in the NEW mapping it serves mode3 only. Model:
 /sd/KPU/road_sign_det/road_sign_det.kmodel (571432 bytes)
SHA256 B472A5C45FBB2060CD794BEC7C972D9F58FB40D7DCA27DFE6545125B8E02B901.
Mode4 needs no K210; do not deploy historical K210/main.py visual-line code
for this four-mode release. This task did not rewrite K210 hardware.

Published v1.2.0-rc.6 from main merge ce503ca (PR19); firmware c2ed15d.
https://github.com/lzzzsa24/test-exp7-unified-motion-v1/releases/tag/v1.2.0-rc.6
All7 assets verified against GitHub sizes/SHA256: BIN/HEX, checksums, paired
script, model/manifest and release notes. Release manifest filename is adapted
to packaged road_sign_det.kmodel, model contents unchanged. Pre-release because
this exact new combination is not ground-tested. See docs/releases/RELEASE_V1.2.0_RC6.md.
The composite source branch and rollback/deployed provenance tags were also
pushed. No hardware action. Follow-up publication bookkeeping is docs-only.

## Future workflow

Start subsequent work from new main, name the NEW mode number and return a
bounded commit. Comprehensive-v15 is now a retained provenance snapshot.
Do not silently merge its old five-mode main.c over the four-mode selector.
Rollback main: rollback/2026-09-10-before-main-four-mode ->36f4aed.
Old worktrees and tags are retained; no deletion authorized.
