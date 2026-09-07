# Mode 3/4 fork selection candidate

Base: canonical main 71f0306. Ported source fixes 1275257 and 1186ba8 as
9827b9d and e7c4f08, retaining main's GPIO snapshot ordering fix. No shared
PROJECT_STATE update, merge, serial access or flashing is performed here.

User reports mode 4 chooses the opposite semicircle, K210 shows the expected
arrow, occasional digit 2 detections, and OLED alternates ARMED/PROBE.

Confirmed source issues and changes:

- PROBE previously required a non-junction sample to select. Both outer
  sensors black with both middle sensors white (mask 9) remained a junction,
  so SL2 could continue straight rather than choose the reserved side.
  Stable mask 9 now permits selection; all-black still remains crossbar evidence.
- A 30 ms centre observation previously cancelled PROBE. Require 120 ms of
  continuous centre evidence instead; intermittent centre/broad chatter resets
  confirmation. This duration is an initial candidate, not ground calibrated.
- Once a direction is reserved, PROBE cannot overwrite it with another arrow.
  Entered PROBE keeps its reservation across UART silence until its existing
  bounded probe phase ends. ARMED retains its existing age/freshness limits.
  Digits still contribute no arrow vote and do not directly steer. Raw VIS can
  display 2 while the route retains L/R; recognition-model confusion is not fixed.

Sign/ring host tests cover both directions, digit and opposite-arrow noise,
centre chatter, clock wrap, stable crossbars, all-white non-forward ownership,
and continuous search. Formal ARM build passed (text/data/bss 82844/64/11376).
Physical sensor timing, sign confirmation before arrival, actual wheel steering
and successful semicircle exit still need ground verification. No claim that
mask 9 uniquely identifies every physical fork: track markings can be ambiguous.

Branch: fix/sign-fork-selection. Rollback/base remains main 71f0306.
