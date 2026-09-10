# Mode 3: begin exit alignment before the far-junction tangent

2026-09-10. Base: recorded deployed composite
`4eda8fe800e29cd347a8cc2730cccd4d1310873b`. Rollback:
`rollback/sign-before-earlier-exit-4eda8fe`.

The user's drawing shows the car following the right half-circle, reaching
the upper junction facing left, and continuing along that tangent instead of
joining the upward outgoing line. The source still waits for a signed road
heading of 90 degrees without exit-side evidence. This leaves the alignment
turn until the tangent is perpendicular to the desired outgoing heading.
No fresh on-board yaw trace was collected; the drawing does not measure the
exact heading or motor-response delay in that run.

## Narrow timing change

Angles remain relative to the actual two-second stopped heading, normalized
by sign direction so the upper half is positive for either route choice.
These are heading thresholds, not accumulated angles since ARC entry.

| Mode-3 active exit selection | Before | Candidate |
|---|---:|---:|
| Current narrow pattern includes the outgoing outer sensor | 70 degrees | 55 degrees |
| Any current narrow line, even without that outer contact | 90 degrees | 65 degrees |

Both still require at least 150 mm of encoder-estimated ARC travel and 30 ms
of valid continuous selection evidence. White, full black and wide/both-side
patterns cannot start exit selection. Lower-half and midpoint headings cannot
satisfy the positive gates. Existing outward-heading-return selection below
these nominal gates remains available.

The exit-side threshold is now separate from the old 70-degree natural-exit
region flag. Lowering a turn trigger must not also lower that completion
criterion. Natural departure based on lower/upper-half evidence and steady
forward line travel, stopped-heading alignment, first-line release, LOCK and
direction cleanup are retained from `NATURAL_EXIT_COMPLETION.md`.

No changes to the 26% recognition-stop gate, two-second pause, shared slow
CPS targets, encoder feedback, manual STOP, K210, horn or RGB. Mode 4's
separate trajectory and modes 1/2/5 retain their current composite behavior.
This note supersedes the nominal 70/90-degree active-selection thresholds
in `SIGNED_HEADING_EXIT.md` and `NATURAL_EXIT_COMPLETION.md` only.

## Verification

The new real route -> shared follower -> DriveBase regression first failed
against the deployed source: it remained in ARC at 65 degrees. It now passes
for both directions, both sensor cases and clock wrap. Coverage includes
lower-half/midpoint rejection, just-below-threshold behavior, wide/white
rejection, interrupted debounce, unchanged 0/2200-CPS forward pivot, return
to the stopped heading, 1412-CPS normal travel and manual STOP. Existing arc
simulations now follow the earlier exit command instead of continuing to feed
another 25 degrees of ring travel after the command has changed.

Passed: full `cmd /c tests\sign_line\run.cmd`; mode1/2, gyro and mode5
integration checks; formal `build_unified_motion.ps1`; `git diff --check`.
Formal ELF text/data/bss: 115120/64/23544 bytes. BIN: 115188 bytes.

BIN SHA256: `5FFDD4E33BBF0D8A4E731E2826254B31B8B7B9B31E399966E0D2E4CEFD973AE8`

HEX SHA256: `6BF52C8BC72857DE9051039EC59F97C358992BA9E9194E3A4AA6865F3806C8D9`

The 55/65-degree values give turning lead but remain a ground-test candidate:
the appropriate lead depends on circle radius, sensor mounting and actual
chassis turning radius. Host tests verify control decisions and wheel targets,
not physical capture of the outgoing line. No serial, flash, wheel or ground
test, push or integration merge was performed. Integrate the new delta onto
the comprehensive branch; do not replace main's different controller baseline.
