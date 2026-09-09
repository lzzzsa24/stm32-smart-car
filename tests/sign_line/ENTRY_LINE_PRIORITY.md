# Withdraw blind entry pivot

Baseline: deployed 917df47, with current main 7716af3 documentation merged.
Latest user observation: direction shown after observation, then entry crosses
the line toward the central sign. Source has OBSERVE, not DISCOVERED; exact
display interpretation is unconfirmed without a frame or trace.

Confirmed software hazard: 6bc7d28 made entry pivot active below 60 degrees
even for white, opposite-only, or center after departed was latched. Route
output then overrode the live SimpleLine output. This delta removes that
entry exception. The confirmed direction remains in route state, but a route
entry output requires the selected edge to be visible and not all-black.
Center releases it immediately; white releases it to existing bounded-yaw
search. This is deliberately withdrawing a previous forced-direction fix;
correct branch choice still depends on real sensor evidence and needs testing.

The gyro-qualified exit alignment and bounded straight reacquisition remain.
Pause .25 threshold / 2 seconds / unconfirmed .5 second retry, K210, horn,
RGB-off, other modes and motor/gyro drivers are unchanged.

Verification: mirrored selected-edge -> white/center/opposite entry checks
retain direction without active route output; complete sign suite and ARM
build. No serial access, flash or physical validation. Apply only this delta
to deployed 917df47 or comprehensive equivalent 426dedd.
