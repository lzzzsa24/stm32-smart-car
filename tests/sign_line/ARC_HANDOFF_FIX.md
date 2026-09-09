# Separate entry choice from arc following

User observed repeated rotation in the recognized direction after entering
the circle. Current recorded deployment is 0a02d3d, which contains the
ten-second experiment. New branch fix/sign-arc-handoff starts at canonical
main 49288f1 and ports 4c345b8 as f7860ff before this correction. It preserves
main's current line/bypass code; it does not import the deployed composite's
separate shared KEY1/KEY2 entry increment. No unrelated main files changed.

Confirmed control issues:
- A mandatory ten-second PROBE return blocked normal arc entry even after
  the sensors had reacquired a line, continuing to seed the sign direction.
- SL2 interpreted outside-only/adjacent contacts as counter-rotation even
  while route state was ARC. A curved line could therefore request spinning.

New behavior:
- PROBE retains its chosen direction while selecting a branch. Selected-side
  edge evidence (excluding all-black), followed by stable centre-only evidence
  for the existing 30 ms, transitions directly to ARC. A crossbar followed by
  centre alone cannot claim branch completion. The selected sign remains
  reserved for routing but no longer has continuous motor priority.
- Ten seconds is a maximum. Failure to capture cancels route ownership, not
  line tracking/search. No new timed STOP is introduced.
- ARC uses forward differential tracking on every nonzero raw mask. Left/right
  visible curves use mirrored 2200/2600 equivalent-PWM targets; ambiguous wide
  masks use 2200/2200. Existing recognition speed scaling still applies.
- All-white still searches. Arc entry seeds opposite-to-entry curvature once;
  live sensor direction subsequently replaces it. First nonzero raw contact
  immediately restores forward targets, avoiding filtered replay of a spin.
- STOP, mode reset, later exit selection and failed-route cancellation remain.

Validation: sign/ring tests and dedicated production-timing tests passed for
both entry directions, maximum-time cancellation, early capture, ten seconds
of opposite-to-entry arc curvature despite repeated arrows, white search,
immediate reacquisition and STOP. Mode-5 regression and ARM build passed
(text/data/bss 84716/64/11600). No serial, flashing, wheel or ground tests.

Limits: sensor sequence does not prove physical semicircle identity. Forward
arc speed ratio and capture timing need ground validation. This change removes
the identified sustained-rotation commands, not wheel slip or all causes of
recognition/route errors. No main merge or shared PROJECT_STATE update.
