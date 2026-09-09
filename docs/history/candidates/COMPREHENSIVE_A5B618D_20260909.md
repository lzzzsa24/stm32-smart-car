# Comprehensive candidate a5b618d (2026-09-09)

- Request: merge 2e9a38e; no flash requested.
- Base: ec2dd2f6b9ae20df45f461e45669dc32ad8a77ef.
- Worker delta: 2e9a38e4fc34bd2c932438e2659438cd94dfe203.
- Result: a5b618d339edfb89f3b2d2545d2894c665c3663a.
- Branch: test/comprehensive-v15-sign-horn-20260909.
- Checkout: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn.

## Change and scope

Opt-in sign low-speed SPEED control uses stronger powered phases (straight
3000 PWM; turning wheel-specific recovery power, voltage compensated and
capped at 3400). Encoder-accounted coast intervals retain the low average
speed request. Zero requests, STOP, opt-out and POSITION behavior remain
covered by regression. Motor polarity, obstacle geometry, K210 and main
firmware source were not changed.

## Verification

All commands exited 0 in the result checkout:

- cmd /c tests\sign_line\run.cmd
- cmd /c tests\line_recovery\run.cmd
- cmd /c tests\gyro_turn\run.cmd
- powershell -NoProfile -ExecutionPolicy Bypass -File build_unified_motion.ps1
- git diff --check

Loaded low-speed tests use simulated friction, not a measured chassis model.
They do not establish real wheel startup, speed accuracy or floor behavior.

## Artifacts

Directory: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn/manual-build-unified-motion

- BIN: exp7_unified_motion.bin; 109900 bytes.
- BIN SHA-256: 390732B8C9F64AEF2A5B6C2CE186B46106F9C620B6CC11B115E192BFD1F87FEC
- HEX: exp7_unified_motion.hex.
- HEX SHA-256: 79A7392C30C244CEF307267D30D2D0C2A29710267D680A8DDA3AECEF74A2B171
- ELF text/data/bss: 109832/64/18344 bytes.

No serial access, flash, readback, GO, lifted-wheel or ground test occurred.
Last verified board deployment remains ec2dd2f; K210 remains 20e8c72.
No GitHub push was requested or performed.
