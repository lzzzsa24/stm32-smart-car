# Comprehensive candidate e8f6fbe

Requested: 078cfdb9125799ca4bb3998dd1809e488ddd67e0.
Base: bd87633eb645344f9dde85ec4b2700ea65e3e5e7.
Result: e8f6fbeb0dc2a55e298aff5de3c3d94da4f6295c.
Checkout: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn.
Branch: test/comprehensive-v15-sign-horn-20260909.

Only fixed bypass length parameters change: outward 360 to 300 mm;
parallel 600 to 360 mm. 45-degree return, earlier ultrasonic thresholds,
old-heading echo rejection and modes3/4 shared KEY2 tracking retained.
Side IR remains disabled; main code and K210 unchanged.

Formal build and full gyro_turn and line_recovery suites exited 0, including
300/360-mm mirrored route tests. git diff --check passed; checkout clean.
ELF text/data/bss: 108680/64/18336. No sign suite rerun for this parameter-only
bypass change. No serial access, flash, physical test or GitHub push.

Artifacts under checkout/manual-build-unified-motion:

- exp7_unified_motion.bin SHA256: 7BD8884439298F8B7483A4CFCEBF1F3A410DC5DB1BB42435C124AB8A56843D4D
- exp7_unified_motion.hex SHA256: 9749ABA86863BBECB28419DA86F8592655F54F667B239501D803B77E727EF162

Board remains previously verified bd87633; K210 remains 20e8c72.
Encoder distances remain requests, not measured chassis travel.
