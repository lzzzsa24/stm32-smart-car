# Pause threshold and unconfirmed retry

Continuation of 6bc7d28, refreshed with main 5ab0837. Recorded hardware source
6bd0c89 includes that previous delta. Apply only this new delta to it.

Mode 3/4 pause requires a fresh valid left/right frame with score >=25 rather
than >=15. K210 detection threshold .15 and STM32 direction vote minimum .20
are unchanged. Confidence is not distance: this reduces weak early triggers
but is not a geometric stopping-distance guarantee.

The zero-output observation remains 2000 ms. Subsequent frames cannot extend
it. When direction is unconfirmed, a fresh qualifying frame can start the next
pause 500 ms after the previous window ended (2500 ms from its start). No
no-target interval is required. No fresh frame means no automatic new pause.
When route.direction is nonzero, the adapter inhibits any new pause, while an
already active window completes normally. Existing pre-entry route eligibility
and manual STOP/mode reset priority remain. ARC/EXIT/CANCEL cannot start pauses.

Tests cover confidence 24/25, confirmed inhibition, uninterrupted voting,
fixed deadline, retry at 499/500 ms, duplicate frames, reset and timer wrap.
Full sign suite and ARM build required. No flash or physical validation.
