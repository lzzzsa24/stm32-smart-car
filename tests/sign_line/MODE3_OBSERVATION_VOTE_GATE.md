# Mode 3 observation and direction gate

Base: comprehensive `55486e6`, source-equivalent to worker `6caa1c6`.
The user reports that observation and OLED directions stopped working after
recent exit changes. The exit-only diff does not alter initial observation;
that alone cannot exclude a control-state interaction on the physical car.

## Reproduced defect

The real application evaluates observation eligibility before feeding each
frame into the route vote. Previously three 20/21-percent arrow frames could
publish a direction without reaching the 22-percent parking gate. The next
22-percent frame then found a committed direction and parking was inhibited.
This skips the stopped-heading reference as well as the observation action.
Later route cancellation can withdraw that prematurely committed direction.
The host reproducer establishes the first defect, not a physical cancellation.

The regression runs the actual follower and DriveBase with mocked inputs.
On the unchanged base it fails with `state=1 direction=-1` before any stop.
Earlier threshold-audit tests explicitly documented this undesirable policy;
there is no evidence that the last exit patch introduced the gate itself.

## Change

Mode 3 still needs three matching votes in the latest five frames, with the
existing 600-ms span, final-two agreement and continuity checks. At least one
matching vote must now meet the shared 22-percent observation constant.
20/21-percent frames still contribute. Storing each vote's score makes the
qualifying evidence expire, shift and clear with that exact vote.

This preserves the main-loop ordering: when the first qualifying frame arrives
in an eligible entry state, its observation is requested before direction
confirmation can inhibit later duplicate stops. Four-white input still seeks
either middle sensor before the full two-second hold. Mode 4 voting and its
26-percent parking threshold are unchanged; no K210, UART, OLED, speed or exit
angle changes are included.

## Validation and limits

- Mirrored real-follower regression: weak frames then 22 stop for the full two
  seconds, expose the confirmed route direction used by OLED, preserve the
  stopped reference, resume and do not repeat the confirmed stop; tick wrap.
- Vote isolation: wrong class, expired vote, sequence gap, object-position jump
  and window eviction cannot supply stale qualifying evidence; mode 4 parity.
- Full sign suite and formal ARM build are required before handoff.

No hardware access or physical verification. This fixes a demonstrated bypass,
but does not establish why every reported missing direction occurred. In
particular, a missing UART arrow or an existing LOCK/CANCEL rearm block remains
a distinct possibility; that cooldown policy is unchanged in this patch.
