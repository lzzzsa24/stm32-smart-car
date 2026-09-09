# Combined mode1 speed and sign handoff candidate

Result 3a00fa8022a2bf991bbbc3fac878df5e62ebcc79 in
F:/myproject/jidian/worktrees/comprehensive-v15-sign-horn, branch
test/comprehensive-v15-sign-horn-20260909. Started at deployed a56f6c7.
93b408b4080302b3c410319b9ece80e49a8d014a replayed as c3476b7;
bf3e8e1826243446c6d84506e660b7ba74972ecc replayed as c9205a6.
Follow-up 3a00fa8 changes only an obsolete gyro integration test assertion:
sign modes remain excluded from forced wait recovery, as already intended.

Mode1 stable-centre opt-in raises ceiling to 2850 equivalent PWM; clear
continuous bypass return cap to 2100 CPS. Mode2 boost remains disabled.
Modes3/4 protect selected-side search until verified entry-line capture and
share one tested final output resolver preserving STOP/pause priority.
Other controls, K210, motor polarity, obstacle limits remain unchanged.

Verification: complete line_recovery (both search speeds), sign_line (new
production handoff test included), gyro_turn suites passed. Gyro suite was
rerun after fixing the old all-mode assertion. Formal ARM build/link passed
at c9205a6; final commit alters tests only, no firmware source difference.
Text/data/bss 106316/64/18304. BIN 106384 bytes.
BIN SHA256 4E83C0EB990FEF6E44B56F368B2CA8D5A16A5DE25DB586E97523588D692BB36F.
HEX SHA256 A7E7266BC2234AEB5F81E165DB4B41F4BB6CA4B06660BC7C58B4ABC37BF82233.
Artifacts under that worktree's manual-build-unified-motion directory.

No flashing, serial access, physical test, main code promotion or remote push.
Board remains a56f6c7; K210 stays deployed 20e8c72. Higher speed and handoff
parameters are not ground-validated by host tests. Rollback source a56f6c7.
