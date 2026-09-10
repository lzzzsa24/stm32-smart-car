# Mode 3: remove forced straight travel after exit alignment

2026-09-10. Base: `c35df1aeaab9beeda273e20b8412a6a7b63ac6c0`, retaining
the previous observation-resume repair on comprehensive `d01ab4c`.
Rollback: `rollback/sign-before-exit-contact-c35df1a`.

The user explicitly limits this task to mode 3 and identifies the outgoing
control logic, not missing sampled sensor pulses, as the concern. No sampler,
UART, ISR or mode-4 trajectory change is included.

## Changed control ownership

Mode 3 previously entered EXIT LINE after gyro alignment and issued equal
positive wheel targets whenever the foreground reading was white and no
narrow exit contact had been latched. This independent straight segment could
continue until its two-second / 250-mm bound. Broad contact did not latch a
narrow line, so the command could resume on the next white reading.

That branch and its obsolete contact latch have been removed. Reaching the
existing heading alignment criterion now releases motor ownership immediately,
including when no line is visible. EXIT LINE only confirms completion:

- live narrow line: the existing slow tracking controller corrects position;
- actual all-white: the existing line-loss recovery searches;
- a broad mark followed by an outer contact: current contact gets immediate
  correction, without the ordinary crossing-straight tail hiding that input;
- centered contact confirmed for 30 ms: clear route direction and enter LOCK;
- failed confirmation: the existing bounded CANCEL still releases control.

EXIT LINE cannot reassert either a straight or a turning route command.
The same slow feedback table as acquired-arc tracking supplies current-line
priority during this mode-3 phase; wheel speeds and encoder control are unchanged.

The preceding EXIT TURN remains the existing bounded gyro-alignment turn,
started by the 55/65-degree criteria. It is not replaced by ordinary ring
following before alignment. Natural departure, direction cleanup, the two-second
recognition stop and its resume repair are retained. Modes 1/2/4/5 are unchanged
by this delta.

## Verification

Before repair, host output reproduced `1412/1412 CPS` after a full-black mark
changed to a visible single outer edge in EXIT LINE, and route straight control
returned after broad contact followed by white. The final mirrored route ->
follower -> DriveBase test passes: current edge commands `0/2200` or its mirror,
aligned white never restores route ownership, real loss uses shared search,
and manual STOP remains authoritative. No sampled-pulse workaround is included.

The complete sign suite passed, including observation-resume, earlier exit,
natural departure, opposite-bend cleanup, slow-profile parity and unchanged
mode-4 trajectory. Mode1/2, gyro and mode5 integration checks passed, as did
`git diff --check` and the formal `build_unified_motion.ps1` build.

ELF text/data/bss: **115652/64/23536**. BIN: **115720 bytes**.

BIN SHA256: `4075666D425978E43BE4E35F1F25F39F511AC1D3E14BD3200E0A9CC7C9233988`

HEX SHA256: `301BA05228F28C6A890BBA21E79D9F8DB5EA5D51C217923BD8F83A87B185A03C`

This deliberately removes the earlier straight-until-contact behavior: when
alignment finishes off the line, the car now searches instead of driving
straight to it. Physical reacquisition remains unverified. No fresh on-car
report, serial access, flash, physical driving test, merge, push or shared-state
edit was performed. The user did not observe the precise failing OLED phase;
these checks establish code behavior, not every cause of the reported run.
