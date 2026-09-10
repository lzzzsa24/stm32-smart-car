# Mode3 completion evidence begins after exit entry

Base: comprehensivea217df8. The user requires an exit to have started before
the selected outer sensor may complete it. Existing code already limited
completion to EXIT_SELECT/EXIT_CLEAR, so it could not directly complete ARC.
However, enter_phase seeded the clear latch from the ARC-to-EXIT transition
sample; the very next black sample could therefore finish without any new
clear observation during exit.

The route now marks exit_started and its time only on EXIT_SELECT entry, with
the selected-outer clear latch reset. Only subsequent ticks in an exit phase
may record a clear, and only a black observation after that clear may complete.
Pre-exit masks, the transition sample and repeated calls in that same tick
cannot arm completion. EXIT_SELECT->EXIT_CLEAR preserves the latch and start
marker; a direct unrelated EXIT_CLEAR entry cannot create an exit. ARC entry,
cancellation, completion and Reset clear the marker/latch.

The sign direction still selects left bit8 or right bit1; the other three
sensors and angle do not veto a valid post-entry clear/black completion.
No additional angle, distance or delay threshold was introduced. The existing
adjustable30..90deg/default40 start gate, +/- remote, OLED, observation20percent,
two-second hold,1800-CPS observation search, horn and mode4 behavior are retained.

This defines software phase/sample ordering. It does not claim encoder-proven
physical exit motion, nor distinguish an outgoing line from the original ring
if that ring is recontacted after a valid post-entry clear.

Regression evidence: unchanged base fails the new real-follower test after
transition-clear -> next-black (already LOCKED). Fixed code remains in the exit
task until a new clear/black pair. Mirrored tests cover pre-ARC transitions,
same-tick calls, wrap-compatible clocks, passive handoff and normal tracking
after completion. Existing all-mask/initial-held-black, configurable-angle,
mode4 and STOP tests remain in the full sign suite.

No serial, flash or physical-car testing in this worker task.
