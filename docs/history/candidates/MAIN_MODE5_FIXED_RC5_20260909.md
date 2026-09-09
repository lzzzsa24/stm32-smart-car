# Main mode5 promotion and rc.5 publication

Request: put latest comprehensive mode1 into main mode5; replace visual line,
sync GitHub and publish. No flash authorized or performed in this task.

Main rollback baseline:e643c398c2fc7107c0fd1c30140d7c3ce8e2936c.
Rollback tag:rollback/2026-09-09-before-mode5-fixed-bypass.
Composite source:58c3744de08796edba355ef3becf8453819aa546.
Functional main commit:cd44bb0ea4f94a90511652cc80672d04f9359f93.
PR15 merged to main:71b2e7f41f52c200e62de0c183f6ab09fd885abd.

Mode5 exclusively receives fixed250/300-mm geometry,4000-CPS fixed legs and
return,16/22-cm approach,10-cm raw critical/15-cm forward guard and mode-specific
straight boost. Side IR disabled only in mode5. Legacy enable state is restored
on mode exit, including preserving calibration failure. Main mode1 retains
1800-CPS adaptive return and5-cm raw critical; legacy sign/driver/K210 sources
are byte-equivalent to baseline. Old visual controller has no app dispatch.

Verification: formal ARM build and sign_line, line_recovery, gyro_turn,
dfplayer, archived vision_line_v4 and new mode5_bypass suites passed.
After narrowing line-tracking changes to boost-only, line and mode5 suites
were rerun and passed. Mode5 suite tests actual app profile/selector extracted
from main, repeated5/1 transitions, preserved encoder/driver/sign files,
fixed/adaptive route tests and old-heading echo rejection.
Exact merged main rebuilt; Core/Drivers/K210/build/linker matched tested source.

Release:v1.2.0-rc.5 (pre-release), target71b2e7f.
URL:https://github.com/lzzzsa24/test-exp7-unified-motion-v1/releases/tag/v1.2.0-rc.5
Seven uploaded assets checked against local size and GitHub SHA256 digest.
No legacy mode5 K210 script included; only unchanged main mode3/4 sign assets.
ELF text/data/bss102184/76/12024. BIN102264 bytes.
BIN SHA256:79776C89C9F26262EA395DFFD451082599FC200BBB94BF459D248356B845F088
HEX SHA256:938034772165C2C0774D4D70DAF682709C9D79205C09EC1DC14CAD359928E81A
Package:manual-build-release-rc5. Source/artifact tests are not physical proof.

GitHub protection retained: PR required, configured approvals0, no bypass.
Comprehensive branch and current rollback/deployment tags also pushed.
Unrelated untracked purchase guide preserved, not committed or published.
Board remains58c3744; K210 remains20e8c72. No serial access, readback, GO,
lifted-wheel or ground test occurred for this main firmware.
