# Comprehensive V15 single-sign horn candidate

## Five-repeat follow-up

Latest source: `9cea6bcc2b9b943ae79a53040b2517dda3c8a6b5` changes only
the confirmed horn event to five complete phrases (nominally 7.65 seconds).
Continuous visibility still does not retrigger. Nonblocking playback and safety
preemption remain unchanged. Integration assertion, horn latch regression and
formal ARM build passed; no flashing or physical listening test.
Latest BIN SHA256: `997E54D55F3F279EB27FBBBE0F32D93FCF6AFD68A768F485E4D4D2F01441F461`.
Latest HEX SHA256: `3D82C83C4BD1B0F8004947ED040FE19E72ECF54B14F1961FAAC49C34E381F970`.
BIN remains 104200 bytes at the paths below. Earlier hashes below are historical.

## Original one-repeat merge

- Source merge: `91f2e3999762f93c0ecea58999c7a9464a974caf`.
- Branch: `test/comprehensive-v15-sign-horn-20260909`.
- Started from canonical main `7588116`, merged full V14 `8a46fd6`, then `4c24f1f`.
- Preserves V14 STRACE, gyro calibration/navigation and other comprehensive modes.
- K210 now selects the highest-score valid left/right/horn candidate. This deliberately replaces the older opposing-arrow veto. Digits remain inert.
- Modes 3/4: three fresh horn confirmations trigger one PG12 buzzer phrase; a continuously visible sign cannot retrigger, fresh absence rearms. Mode changes reset confirmation. DFPlayer remote controls are unchanged.

## Verification

Sign-line suite (including gyro, trace, horn and K210 runtime), DFPlayer suite and formal ARM build/link passed. State checker OK with expected unflashed-worktree warnings; diff check passed.

- BIN: `manual-build-unified-motion/exp7_unified_motion.bin`, 104200 bytes.
- BIN SHA256: `B29CFA61526E10D2D115727292FE3B82E277A094E4D9C499ADE31370725B3ECB`.
- HEX: `manual-build-unified-motion/exp7_unified_motion.hex`.
- HEX SHA256: `AB09A761EF3D04395269A4A5E74012F6ACEB2127328B9CCB77C53D169D639E56`.

No flashing, serial access, physical test or remote push performed. STM32 still runs V14. End-to-end horn testing requires deploying BOTH this STM32 firmware and its `K210/sign_mode34.py`; the currently deployed K210 script is unchanged. Main firmware is not promoted by this candidate merge.
