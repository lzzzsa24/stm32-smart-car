# GPIO snapshot and interrupt history ordering

Worker base: canonical main `da6971c6c751acd08c4276cada81c65a29ece62a`.
The latest firmware source recorded at this base is `7ce4944`.

## Reproduced defect

The main loop reads GPIO before calling `line_tracking_compute()`. A SysTick
interrupt can sample a new outer edge between these calls. Previously compute
drained the queue through its own start time, replayed the new edge, then applied
the older GPIO snapshot as if it were newer. An old middle/center/wide pattern
could clear the edge; an old opposing edge could replace it. On the next white
reading normal tracking could default left, or active recovery could keep its
previous direction.

The initial regression using real GPIO acquisition and the real tick/queue
code failed 6 of 8 direction cases before the fix, and passed all 8 afterward.
Those cases cover normal/active recovery, both sides and tick wrap. This proves
a software failure path compatible with the user's observation; no current
hardware trace establishes that it is the only cause of the physical symptom.

## Change

`line_tracking_read()` now returns a timestamped snapshot. Timestamp and four
GPIO reads share a short IRQ critical section; the caller's PRIMASK is restored.
There is no control calculation, queue drain or wait inside that section.

Compute uses acquisition time as its observation boundary. Later ISR samples
remain queued until the next snapshot, preserving chronological direction
evidence. Same-tick queued samples precede the live snapshot. A validity flag
allows tick zero and wraparound; zero-initialized synthetic readings retain
compute-time behavior. The synthetic lift-test constructor is initialized too.
Callers must process snapshots in acquisition order. Rebuild all users of
`LineTrackingReading` because its layout changed.

Mode 1/2 use this common path. Sampling stays at 1 ms; direction thresholds,
transverse handling, search speed, wheel mapping and STOP behavior are unchanged.
This cannot recover an electrical pulse that never reaches a software sample.

## Verification

- `tests\line_recovery\run.cmd`: pass at both 2493 and 1870 CPS search targets.
  Expanded regression: 64 timing cases and 2 same-tick/tick-zero cases per build.
  Covers prior inner/center/opposite outer/wide snapshots, normal/active search,
  both sides, wrap, and an ISR injected at snapshot IRQ restoration.
- Existing real DriveBase/load/bypass, alternating-corner, persistent-search,
  STOP/fault ownership and RAM-log host regressions: pass.
- `tests\sign_line\run.cmd` and `tests\vision_line_v4\run.cmd`: pass.
- `build_unified_motion.ps1`: pass, text/data/bss = 82764/64/11384 bytes.
- No serial access, flash, lifted-wheel or ground test. Main and
  `PROJECT_STATE.md` are not modified by this worker.

Integration owner should merge only this new increment, rebuild the complete
firmware and run the same suites. After separately authorized deployment, check
both outer-edge-to-white directions after straight acceleration and after
alternating corners without reset. If the symptom persists, preserve power and
collect the existing LSEARCH/LFAULT records after operator STOP: distinguish a
missing/invalidated right edge from a right command with incorrect physical yaw.
