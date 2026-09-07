# Lone-inner line-loss direction fix

## Ground observation

With only one inner line sensor active (X1 or X3), the previous tracker could
store that sensor side as a persistent turn-direction hint. At one repeatable
approach angle this made the car keep searching in the wrong direction. The
problem could be hidden by retuning the four sensor thresholds, but the input
pattern is physically possible and must be handled in software.

## New rule

- A lone X1 or X3 sample still produces proportional steering while visible.
- It is not accepted as a persistent geometric direction hint.
- If the line is then lost within 200 ms, recovery starts toward that sensor as
  a probe, not as a locked direction.
- The probe reverses after measured four-wheel encoder travel corresponding to
  30 degrees, then expands by 30 degrees per reversal up to 120 degrees.
- Elapsed time and battery level do not advance or reverse a sweep.
- A fresh unambiguous outer sensor exit overrides the probe immediately.
- If a lone-inner contact is captured and lost again during settling, recovery
  continues in the direction that physically found it.

## Verification boundary

Host tests cover X1/X3 mirror symmetry, encoder-bounded reversal, no timed
reversal without motion, and capture/re-loss direction retention. A successful
host build does not establish ground behavior; the revised firmware still
requires a later user-authorized flash and ground test.
