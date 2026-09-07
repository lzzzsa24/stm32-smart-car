# Adjacent overlap direction and continuity audit

## Baseline and observed failure

Worker starts at canonical main `9518a678acda7bb7edc89fcd5ab1b8b2c9782bb9`.
Main still has firmware source 2d1edaa; the board deployment record points to
temporary c44d49d. This candidate includes the c44d49d prerequisite plus the
new fixes below, as one increment on current main. Do not separately apply
c44d49d before this combined increment without resolving duplicate changes.

The preserved serial capture at
`validation/read-line-log-20260907/170351-native/serial.txt` in the parent
workspace contains `ms=44017 side=-1 source=6 hint=-1 edge=0 wide=13`.
Mask 13 is X1+X3+X4: middle pair plus right outer, with left outer white.
This is evidence that the algorithm kept a left hint despite seeing this
combination, not proof of the entire preceding GPIO sequence.

A minimal replay supplies an old opposing inner hint, an adjacent triple and
white. Before this fix all 8 initial normal/active and live/queued cases chose
the wrong direction. The final 16-case version adds tick wrap and passes at
both 2493 and 1870 CPS search configurations.

## Fixes

1. Direction evidence is separate from motor permission. Masks 7/13 now
   immediately record left/right hints before broad-pattern handling. Existing
   narrow masks 2/3 and 8/12 retain their meanings. Both outers, all black and
   nonadjacent pairs create no new side hint. Broad patterns still cancel an
   active corner and command forward travel throughout the crossing guard.
2. The same narrow side filter existed in main's mode 1/2 automatic-wait
   recovery branch. It now calls the shared direction classifier. Existing
   mode 1 infrared priority and bounded wait/recovery timing are retained;
   this exceptional recovery was already allowed to rotate after a long pause.
3. The corner's 12-ms timer could bridge a white/opposite/inner sample observed
   only by the ISR. Two separated live outer hits then falsely looked
   continuous. Queued contradictions now invalidate the pending corner.
4. The 4-ms reacquisition timer had the equivalent gap: ISR white/outer samples
   did not cancel pending live middle capture. They now invalidate continuity.
   Historical samples never complete capture; new live confirmation is still
   required. Two initial false-corner and two false-capture cases reproduced
   before the changes and pass afterward, including successful fresh retries.

LSEARCH appends `hint_mask` and `hint_age` to show the accepted hint's origin
and age. Existing `edge` keeps its narrow-outer-only meaning, so `edge=0` can
coexist with a valid `hint_mask=13`. Source 6 remains crossing-held evidence;
the new fields distinguish a held inner hint from a new adjacent-triple hint.
The RAM ring structure changes; rebuild the whole firmware. No Flash logging
or serial output during motion is added.

## Checked related paths

- All 16 masks have an explicit direction-classifier expectation.
- All 256 ordered mask pairs are compared against their left/right mirror
  (512 traces). Unknown direction intentionally shares default-left; known
  decisions and hint masks must mirror.
- Live and queued triples, normal and active recovery, tick wrap, newer side
  overrides, stable center, hint expiry, reset, queue overflow and broad-mark
  command priority are covered by the full line suite.
- Previous GPIO-snapshot/ISR ordering regression (64 cases) remains passing.
- Real DriveBase source with simulated encoders checks 8 alternating broad
  corners and another 8 with adjacent overlaps against the preceding opposite
  outer hint, without resetting between corners. Four-wheel targets and PWM
  signs after the existing acceleration ramp, crossing guard and STOP pass.
- Automatic-wait main binding is inspected and ARM-compiled; its classifier
  has the exhaustive host tests. The full main loop is not host-executed.
- Mode 3/4 sign/ring and mode 5 suites pass. Their independent controllers,
  motor mapping and drive loop are unchanged. No claim is made of a complete
  audit of their navigation policies.

## Validation and limits

Commands: `tests\line_recovery\run.cmd`, `tests\sign_line\run.cmd`,
`tests\vision_line_v4\run.cmd`, `build_unified_motion.ps1`, and
`git diff --check` all pass. ARM text/data/bss = 83264/64/11512; BIN 83332 bytes.

No COM access, flashing, motion, lifted-wheel or ground test in this worker.
The original 100-ms broad-tail and 60-ms middle-gap forward allowances, 400-ms
held hint and physical acceleration/traction can still affect overshoot.
Adjacent triples can also come from an oblique interference mark: accepting
their side improves the demonstrated discarded-evidence case but is not a
universal track/interference classifier. Symmetric readings contain no unique
left/right direction. Continuous line search still requires valid new sensor
evidence to correct or capture, and operator STOP remains authoritative.

After integration and separately authorized deployment, test both real sharp
bends, transverse strips followed by a continuing center line, accelerated
entry, repeated capture/loss and STOP. Preserve power after a failure and use
the native serial reader that retains inherited DTR/RTS configuration; opening
a port is not an independently proven guarantee against a hardware reset.
