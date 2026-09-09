# Comprehensive candidate fd8472b

- Source: fd8472beb296f436b1b99c7ea88a152d550f5f7f.
- Branch: test/comprehensive-v15-sign-horn-20260909.
- Worktree: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn.
- Fast-forward from58c3744; rollback tag: rollback/2026-09-09-before-comprehensive-fd8472b.
- Mode3 confirmed exit clears route L/R and motor ownership; ordinary line
  following/search resumes. White/wide masks cannot independently trigger
  arc exit; heading alignment no longer waits for an extra45-degree turn.
- OLED reports the real route phase; STRACE appends heading/arc peak and
  requested left/right CPS. Mode4 driving parameters remain unchanged.
- Main rc.5 firmware, mode1 bypass, K210 and motor polarity are unchanged.

## Verification

Full tests/sign_line/run.cmd, tests/line_recovery/run.cmd (both speeds),
tests/gyro_turn/run.cmd and build_unified_motion.ps1 passed in this worktree.
git diff --check passed. Host tests are not evidence of ground performance.

Artifacts under that worktree's manual-build-unified-motion:

- exp7_unified_motion.bin:111196 bytes,
  SHA256 682EC10EAF177126609169CFE3FFA28258F967BDA0A9950774E1F0C62B51FC4C.
- exp7_unified_motion.hex:
  SHA256 5E46B4B72941851C8C9E1B1FE8D1551A0420D0369D6BCA677247CC5DF34136FF.

No serial access, flashing, physical test or GitHub push was requested or
performed. STM32 remains the previously verified58c3744; K210 unchanged.
Do not use the different main rc.5 BIN when selecting this test candidate.
