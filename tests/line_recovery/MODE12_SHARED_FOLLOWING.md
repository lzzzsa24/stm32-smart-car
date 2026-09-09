# Share the current MODE2 tracking entry with MODE1

Base: canonical main 65e563944b7ad9ab92f5d9d918611f2e0c7edb7c (rc.3).
The main line implementation already includes f34e7dd's persistent outer
correction, silent loss recovery and the earlier direction-history fixes.
MODE1 and MODE2 were already calling the same compute/apply implementation.
There was no separate stale MODE1 copy to replace.

The remaining differences were in the app entry configuration:

| Setting | Previous MODE1 | MODE2 / new shared entry |
|---|---|---|
| Middle steering gain | 200% | 100% |
| Forward before ever seeing a line | enabled | disabled: search |
| Smooth middle tracking | enabled | enabled |

`line_tracking_start_following()` now owns the reset and that common profile.
Both mode-entry branches call it. `line_tracking_follow_once()` owns one
atomic GPIO snapshot, compute and final command application; both modes call
it rather than maintain separate copies of this sequence. The old app-level
apply wrapper is removed.

MODE1 still supplies its ultrasonic forward-speed cap. Obstacle/ultrasonic
owners keep their existing early returns and priority, and the line cycle
runs only after they release ownership. The bypass DONE branch still resets
line history; it retains the common 100%/no-blind-forward profile on reentry.
MODE2 retains its full limit. The disabled legacy MODE1 vision-action layer
is unchanged. Modes 3--5 and their controllers are unchanged.

## Verification

- Real-source host replay compares the new shared entry/cycle against the
  previously deployed MODE2 reset/settings/read/compute/apply sequence.
- 3600 samples match actions and both side targets exactly, including initial
  all-white, right outer persistence, adjacent pair, center, lone inner,
  loss, broad marks and left correction. Full and reduced forward caps are
  covered. Starting from stale MODE1 settings and post-bypass-style reset
  verifies that blind forward and double gain cannot return.
- Source binding checks require both real main branches to use the shared
  profile and cycle, with no extra per-mode line setters. They also check
  bypass ownership, reset on DONE, ultrasonic state/zero-cap early returns,
  and the disabled legacy vision override.
- Complete line-recovery/bypass suite, sign-line and vision-line-v4 suites
  passed. Formal build and git diff --check passed.

Commands: tests/line_recovery/run.cmd, tests/sign_line/run.cmd,
tests/vision_line_v4/run.cmd, build_unified_motion.ps1.
Formal text/data/bss: 84192/64/11592; BIN size 84260 bytes.
BIN SHA256: E2DA40D39ACBEB09A6D692F4ACE0C3FEC358AE877A8FE14BFEF60B6982E154B4
HEX SHA256: BF672C7DC40B32476288711C420457FCF488F2F78ACAFBA7D5B1CFE99878ED29

No serial access, flashing, reset, car motion or ground test occurred. Main's
untracked purchase guide and PROJECT_STATE were untouched. This worker uses
current main, not the board's separate experimental ten-second sign-probe
layer. The integration task owns merge, combined rebuild and deployment.
Different motion near obstacles remains expected because MODE1 still has
obstacle arbitration and speed caps. Host parity is not ground verification.
