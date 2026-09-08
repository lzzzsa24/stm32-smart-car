# Persistent lone-outer correction (mode 2 observation)

The user reports that the car can follow the line indefinitely with only one
outer sensor lit, with inadequate or absent heading correction. Mode 2 is
confirmed. Recorded board source 2c2ed973ef5e34705f301c16871aed043e9ad461
already contains the 64f489e slow-pivot change: the source issues 0/2200 CPS for
as long as one outer stays black. The real speed loop outputs zero PWM for a
zero target; it does not hold the inside wheels against chassis motion.
Thus the old request does not guarantee that the inside wheels stop rolling.
The missing persistent-error response was reproduced in the host test:
after 250 ms on the left outer the old output remains 0/2200 CPS.

## New behavior

- A short sole X2/X4 contact keeps the existing 0/2200 CPS slow forward pivot.
- The same sole outer continuously observed for 120 ms upgrades to -2200/+2200
  CPS (left) or its mirror (right). No forward target remains. The existing
  speed ramp and bounded load assistance handle the transition without a
  whole-car stop or a repeating pulse/stop cycle.
- Every raw observation participates, including ISR history. Any other mask
  (middle, pair, opposite outer, broad or white) clears the escalation; the next
  lone outer starts a fresh confirmation. More than 30 ms without evidence also
  restarts it. Queue overwrite/reset clears the old hold; newer complete
  history may independently qualify. Future ISR observations cannot change
  the command for an older frozen GPIO snapshot.
- This is a visible-error correction, not a lost-line direction latch. Strong
  direction memory, normal capture and subsequent all-white search retain
  their own logic. There is no fixed turn-angle stop or new infinite wait.
- Existing broad-pattern priority, its 100-ms tail and the 60-ms eligible
  narrow-white gap remain. Adjacent-pair forward arcs and centered cruise are
  unchanged. Loss remains silent; remote STOP remains authoritative.
- The valid visible command can now contain a negative target, so an explicit
  caller forward cap of zero is checked before sign-dependent scaling. This
  prevents the new correction from bypassing an explicit stop request.

The 120-ms value is a candidate confirmation window. It requires continuously
consistent evidence, not simply elapsed wall time or LED appearance. Chassis
rotation and clearance still depend on friction, tire slip and track movement.

## Baseline and scope

Worker started from canonical main c277955c366256eedaf40a27c3d593f32bab23c5.
That checkout contains the deployment record, but not the deployed composite
source changes. The three already-deployed source commits c82c8b3, 0a6a0f3,
2c2ed97 were replayed into this isolated worktree as 158e798, ebbf0b1, 8c7070b.
Before this fix, Core and tests matched 2c2ed97 exactly. This preserves the
silent line controller, ten-second sign probe and continuous bypass behavior.
The final fix commit is an increment over that prepared baseline; its changes
are only line_tracking.[ch], the line tests and this note. No PROJECT_STATE,
main, DriveBase, sign controller or bypass implementation was changed by the fix.

## Verification

- A regression first failed on the deployed-source baseline with 0/2200 after
  250 ms, then passed with -2200/+2200 on the left and the mirrored right pair.
- The complete line suite passes at both configured search speeds, including
  120 mask/gain/state combinations with the updated persistent-edge policy.
- Sixty mirrored interruption cases, 119/120-ms boundary, timer wrap, >30-ms
  gaps, unseen middle ISR hits, queue overwrite, delayed GPIO snapshot and
  explicit zero cap/STOP are covered.
- Real DriveBase tests check escalation from a rolling/search state, correct
  four-wheel PWM signs after ramping, immediate target withdrawal on middle
  evidence, subsequent loss, load assistance, silence and STOP.
- Sign-line (including the deployed ten-second probe) and vision-line-v4
  suites pass. Formal build passes: text/data/bss 84420/64/11600; BIN 84488 bytes.
- Commands: tests/line_recovery/run.cmd, tests/sign_line/run.cmd,
  tests/vision_line_v4/run.cmd, build_unified_motion.ps1 and git diff --check.

BIN SHA256: D44DF291F6A49176E67D76BD16D6B1356B07D919603580005318D74BA47ED54A
HEX SHA256: 0D4F79D56ACB603B9D705F44A4C445C2D519244686135A683E81962ED4138414

No serial access, reset, flash, wheel motion or ground testing was performed.
The integration task owns the actual merge and deployment. Apply the final
increment on top of 2c2ed97 or equivalent integrated source; do not replace the
composite firmware with an old line-only candidate.
