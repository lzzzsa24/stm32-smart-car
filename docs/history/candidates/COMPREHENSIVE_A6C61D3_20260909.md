# Candidate a6c61d3

Requested b67ca7bf0a9827ea8e477550cac9fe6ac9c9f921 replayed onto
5cb1bd35f90e2f2ab52b73092bc1046cb064bf59 as
a6c61d30f4936aabf25ecc70ff9ae2bd304c088a.
Checkout: F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn.
Branch: test/comprehensive-v15-sign-horn-20260909.

Sign control consolidated into mode3; digital4 maps to STOP/reserved.
ARC narrow-line feedback overrides old crossing tail; valid measured curve
trend updates search direction; selected-side entry search is phase-anchored
0..110 degrees, acquired-arc search retains +/-25-degree bounds.
Mode1 250/300-mm bypass,4000-CPS fixed travel,16/22-cm sonar and disabled
side IR remain. Slow sign profile and two-second observation stop retained.
K210 filename SIGN34 remains compatible; no camera change required.

Formal build and full sign_line, line_recovery, gyro_turn suites exited0;
includes actual mode-selector tests for3,4/STOP priority and1/2/5 retention.
git diff --check passed; checkout clean. ELF text/data/bss108732/64/18336.
Artifacts under checkout/manual-build-unified-motion:

- exp7_unified_motion.bin:108800 bytes.
- BIN SHA256:8D35B9376F602C68DD8B05BF62D7360DB7DD3973C89F9EB30CAF131EEBD500CB
- exp7_unified_motion.hex SHA256:FFD47049FE1210E9B0ED9521EBB25B0F08A530C33407A3ACB7FEAB2F1331E0E8

No serial access, flash, physical test or push. Board remains533e355;
K210 and main firmware code unchanged. Entry/exit behavior needs ground testing.
