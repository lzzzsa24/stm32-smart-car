# Comprehensive V11: mode 1 continuity and mode 3/4 steady tracking

Branch: `test/comprehensive-v11-mode134-20260909`.
Functional source: `a8b7e8b`.

Created from current main `72b0208`, then merged `230ea7425b0bd5e0ca8d78f1f493124dc88911ef`
including its V8 `c767baa` ancestry. Cherry-picked `ab27bfbfe151d81e37b79a03a83f11590f5b396c`
with conflict resolution preserving the comprehensive gyro and automatic recovery stack.
The independent V10 bypass-rejoin and K210 TX-selection candidates are not included.

- Mode 1 retains slow continuous tracking through invalid sonar returns instead
  of alternating brake/forward; three fresh valid samples restore normal cruise.
  Confirmed close obstacles retain priority. Invalid/unconfirmed echoes do not
  preempt manual audio with warning beeps.
- Modes 3/4 use the steady slow helper in both ordinary and ARC tracking,
  after route direction hints. Forward commands retain the 1200-CPS proportional
  cap and do not claim turn-load assistance. Counter-rotation search retains it.
- Shared MPU6050, route yaw gates, automatic recovery, mode 2/5, motor mapping,
  DFPlayer implementation and K210 assets remain as in V8 except the two scoped increments.

Verification on the combined source: complete sign_line, gyro_turn (including
noisy-sonar and real-drive recovery tests), and line_recovery suites passed.
ARM build/link passed: text/data/bss 99420/64/11976 bytes; BIN 99488 bytes.
BIN SHA-256: `F18BC8A7C516CCAC9B99CD2F1705B29E727CB2327197699094DADECCF213A84B`.
HEX SHA-256: `C877421C2C097BE9EDED5F11EECECDBB0C8E0F6EF6EBB77BAC696345A5FA50AE`.
Artifacts are in this worktree's `manual-build-unified-motion` directory.

No flash, physical test, main integration or GitHub push performed in this task.
Board remains at the separately recorded V8 deployment. Slower steering still
needs ground validation on the actual curve; host tests do not prove traction.
