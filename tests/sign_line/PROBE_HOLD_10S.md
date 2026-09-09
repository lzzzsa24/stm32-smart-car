# Mode 3/4 PROBE direction hold

Superseded by ARC_HANDOFF_FIX.md: 10 seconds is now a maximum; stable capture
ends entry priority early, and all-black no longer requests a branch turn.

User request: retain PROBE L/R for 10 seconds and prioritize that side's
outermost sensor during this interval. Base: canonical main c3e6bc8, including
mode-1 power source 3170221. Branch: fix/sign-probe-hold.

- First PROBE with a confirmed direction starts one 10000-ms window. If the
  direction is confirmed after entering an unsigned PROBE, the window starts
  on the first control cycle with that confirmed direction.
- Centre/crossbar/white input, navigation geometry limits and further vision
  frames cannot leave or renew this window. Manual STOP/mode reset cancels it.
- The selected outside sensor wins over all other sensors, including 1111:
  left priority requests left wheels zero/right wheels forward; right mirrors
  it. The existing recognition forward-speed cap remains applied.
- Without the selected outside contact, ordinary SL2 tracking applies. On
  raw all-white, SL2 is seeded with the held direction before computing output,
  so an old opposite-side memory cannot cause the first search command to turn
  the other way. No blind forward target is issued on white.
- OLED retains PROBE L/R throughout, including search; SEARCH telemetry still
  reports the raw-white condition. At 10000 ms it enters SELECTING with fresh
  phase time/geometry; later ARC/exit logic resumes and may cancel failed routes.

This intentionally prioritizes the selected outside probe even over a black
crossbar during the hold, as requested. Ten seconds may span more than one
physical junction; wheel/track behavior still requires ground testing.

Validation: dedicated test_probe_hold uses production 10000 ms and all 16
sensor masks, both directions, incorrect digit/arrow frames, rollover, exact
expiry, post-hold capture and STOP. The pre-existing route suite runs with the
hold disabled to isolate downstream route regression. Mode 5 regression and
formal ARM build passed (text/data/bss 84596/64/11544).

No flash, serial access, ground test, main merge or shared-state update.
