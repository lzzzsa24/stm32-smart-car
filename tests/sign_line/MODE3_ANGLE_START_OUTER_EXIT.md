# Mode 3: angle starts exit, selected outer completes it

Later update: MODE3_ADJUSTABLE_EXIT.md makes the40-degree default adjustable
30..90 using the independent remote +/- keys; the completion rule below remains.

Base: comprehensive ec15dda. This supersedes the start conditions in
MODE3_EXIT_DISPATCH.md and MODE3_OUTER_EXIT_EVENT.md; their selected-outer
clear-to-black completion rule remains authoritative.

## Behavior

After entering ARC, a valid reference and signed heading >=40 degrees enter
EXIT_SELECT immediately. This is yaw relative to the completed observation
stop, multiplied by route direction (-1 left, +1 right); lower half is negative,
upper half positive. The existing PROBE reference fallback is unchanged if no
usable stopped reference exists.

There is no start requirement for line mask, 150mm encoder travel, 30ms stable
samples, outer contact at30 degrees, or natural-departure time/distance evidence.
Diagnostic sweep/peak fields do not authorize exit. The obsolete natural-return
shortcut and its now-unreachable verification timeout have been removed.

Completion is NOT angle-only. Within EXIT_SELECT or EXIT_CLEAR, left choice
watches bit8 and right choice bit1. An observed clear followed by black completes
immediately, independent of the other three sensors and heading. If already
black at exit entry, it must clear before it can complete. This latch is retained
across EXIT_SELECT -> EXIT_CLEAR.

Existing motor behavior remains: wide-line handoff, real lost-line/recontact
handoff and +/-25-degree alignment (cross-zero tolerance +/-30) release active
steering into passive EXIT_CLEAR. These events do not by themselves clear L/R.
The outer event clears route direction and motor ownership in the same cycle,
then ordinary mode2-derived line following/search resumes.

20-percent observation, two-second stop, pre-observation middle seeking, speeds,
horn, RGB-off and mode4 behavior are unchanged. Existing IMU-invalid, wrong-way,
ARC timeout/distance and exit timeout/divergence cancellation protections remain;
they withdraw a failed route and are not successful-exit criteria.

## Verification

Replace obsolete tests requiring 30-degree outer gating, minimum travel,
debounce or natural-return shortcuts with mirrored all16-mask tests at39999/40000
mdeg and zero travel, including delayed samples and tick wrap. Exercise biased
stopped headings and distinct entry apexes separately from the real follower.
Keep the outer-event tests covering all other-sensor combinations, an initially
held-black outer, early recontact, alignment without completion, same-cycle
DriveBase handoff, subsequent bends/loss and manual STOP. Full sign suite,
mode1/2 and mode5 scope checks, and formal ARM build are required for handoff.

Only host input replay/build evidence: no serial, flashing or floor test.
