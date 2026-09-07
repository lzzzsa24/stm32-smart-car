# Preserve a bounded direction hint through a broad mark

Historical prerequisite from c44d49d. The adjacent-triple direction rules and
the additional continuity audit are updated in DIRECTION_EVIDENCE_FIX.md.

Base: canonical main `9205055b7b140c4099ffd7f06303cbdcf49d7e61`, with deployed
source `2d1edaa`. Worker branch: `fix/line-crossing-hint`.

## Trigger and behavior

A recent right outer hit followed by a broad/multiple-black mark, a brief
center reading and all white used to erase the right hint and default left.
The same event during an already active search could lose its newer exit side.
The initial 16-case replay failed 8 cases before the change. Both directions,
normal/active recovery and direct/ISR history are exercised; the final replay
also crosses tick wrap during the broad mark.

Broad/nonadjacent patterns still cancel an active turn and command forward
travel. Their 100-ms forward guard and the 60-ms narrow white-gap allowance are
unchanged. They no longer immediately erase a fresh directional observation:

- A hint at most 200 ms old when entering the broad mark can be held up to
  400 ms from its original directional observation. Repeated broad readings
  never renew this deadline. This candidate value covers a roughly 120-ms
  broad region and its 100-ms exit guard; it is not ground calibrated.
- All narrow evidence is observed during the guard and active recovery too.
  A new outer side wins immediately. Opposing inner evidence invalidates the
  old side and confirms its own side through the existing 4-ms rule.
- Center confirmation spanning 80 ms, expiry, reset/mode handoff and queue
  overwrite invalidate the held hint. New non-wide directional observations
  return to the normal 200-ms lifetime.
- Search selection using the held direction is logged as `source=6:cross_hint`.
  Sources 0--5 keep their meanings. The RAM log remains silent during motion.
- Once search begins, the existing continuous recovery owns movement; hint
  expiry is not a new search timeout or stop. Current opposite-edge correction,
  confirmed reacquisition, buzzer and operator STOP remain active.

Mode 1 and mode 2 share this path. No motor mapping, wheel-speed loop, PWM
limits, speed profile, sampler timing or mode 3/4/5 controller is changed.

## Evidence and limits

The user's 3.97-second, 30-fps video shows multiple blue sensor lights, briefly
two middle lights, then loss and a physical left turn away from the right bend.
It supports investigating broad-mark direction loss but cannot establish the
millisecond GPIO history. The replay explicitly assumes a preceding valid side.

If the input contains only symmetric broad/center/white readings, no direction
can be inferred from those inputs alone; this patch keeps the existing unknown
fallback rather than pretending it detected a right exit. A short transverse
interference mark can itself generate a misleading side hit. The bounded hold
and center reset reduce stale carryover but cannot identify the real track from
every ambiguous digital pattern. Retaining a wrong initial hint for up to
400 ms is a material tradeoff to validate on the floor.

## Verification

- `tests\line_recovery\run.cmd`: pass at 2493 and 1870 CPS search configurations.
  Includes the 16-case regression, bounded expiry/no stale revival, center and
  opposite-side clearing, reset, queue overflow, unknown-side behavior, the
  previous 64-case GPIO/ISR ordering test, and full existing recovery/load/bypass
  and RAM fault-log regressions.
- Real DriveBase source with simulated encoders/PWM: 8 alternating broad bends
  without reset, all four wheel target signs, PWM signs after the existing
  acceleration ramp, crossing guard, audio and operator STOP pass. Immediate
  requested reversal is not instantaneous physical or PWM reversal.
- `tests\sign_line\run.cmd` and `tests\vision_line_v4\run.cmd`: pass.
- `build_unified_motion.ps1`: pass; text/data/bss = 82856/64/11384, BIN 82924 bytes.
- No serial access, flashing, wheel motion, lifted-wheel or floor test performed.

Integration: apply only this increment to canonical main, review any intervening
line changes, rebuild and run the same suites. After separately authorized
deployment, compare both sharp-turn directions and short transverse marks without
reset. If a failure persists, preserve power and collect LSEARCH/LFAULT after
operator STOP; inspect source 0 versus 1 versus 6 and the recent edge/wide ages.
