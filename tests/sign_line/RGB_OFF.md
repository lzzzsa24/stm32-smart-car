# Mode 3/4 RGB off

Base main da90658, branch fix/sign-rgb-off. User requests both RGB lamps off
in sign modes to avoid illuminating signs.

All six channels are driven low on entry and every mode 3/4 main-loop cycle,
before automatic recovery/fault early returns. Route-direction green outputs
are removed; global fault red output is excluded in mode 3/4. Infrared status
lights remain scoped to mode 1/2. Mode-5 entry still clears both lamps.

Confirmed mapping bug also fixed: left red/blue are PG1/PG2 while left green
is PE7. The old combined GPIOG mask did not clear PE7 and wrote unrelated PG7.
The new helper uses each channel's declared GPIO port and pin.

Only main.c changed. Integrating into comprehensive V14/V15 or later requires
the helper, removal of sign-task green writes, mode-entry/loop off calls and
fault-red exclusion; preserve all unrelated gyro, horn and motor logic. Inspect
any additional target RGB writers before deployment. Do not replace the whole
comprehensive main.c with this main-based file.

No serial or hardware actions, no main merge/state update. Formal ARM build
and diff checks are the software validation; physical lamp state is untested.
