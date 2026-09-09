# Candidate 5cb1bd3

Requested 948c3db2c0753f0558301cc417a2de6ac8e464ab cherry-picked onto
533e3551f161d63d434f4fa0ab8e790088725a9f as
5cb1bd35f90e2f2ab52b73092bc1046cb064bf59.
Checkout: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn.
Branch: test/comprehensive-v15-sign-horn-20260909.

Fixed bypass outward/parallel requests now250/300 mm, not300/360 mm.
4000-CPS fixed travel,16/22-cm approach,45-degree return and slow sign modes
retained. Side IR remains disabled. Header/main historical distance comments
were not updated by the supplied delta; active constants are250/300 mm.

Formal build, complete gyro_turn and line_recovery suites exited0;
git diff --check passed and checkout clean. No sign rerun for this narrow
bypass parameter change. ELF text/data/bss108540/64/18328.
Artifacts under checkout/manual-build-unified-motion:

- exp7_unified_motion.bin:108608 bytes.
- BIN SHA256:2E729D03C0AB00F49EED95A6C02E78B278F6873DA738C005DB2AF0C7676E6D8A
- exp7_unified_motion.hex SHA256:62C0F9C4544AF8C9E51EE717FF8A05DF2A541023DBA0F5F3C40714DB7FF15889

No serial access, flash, physical test or push. Board remains533e355;
K210 and main code unchanged. Distances are requests, not measured travel.
