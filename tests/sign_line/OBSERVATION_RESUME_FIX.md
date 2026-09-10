# Mode 3: resume observation on the real line

2026-09-10. Source base: comprehensive
`d01ab4c621cfa16436ada7139805db8224db1123`. Rollback:
`rollback/sign-before-pause-resume-d01ab4c`.

## Reproduced control failure

The user reports an intermittent unexpected pivot after the two-second
recognition stop. A real route -> shared follower -> DriveBase host test
reproduced a specific cause: with confirmed R, PROBE and left-inner contact
(`mask=4`), resumption requested left/right **+2493/-2493 CPS**. Both wheels
counter-rotate even though an inner sensor is still on the approach line.
The mirrored case is also covered by the repair regression.

The entry search guard classified all opposing-side patterns as white,
including a lone inner contact. That classification was stronger than the
live shared follower. Additionally, the route and search helper continued
evaluating stationary line samples during observation: a crossbar could
advance navigation and search/selection timeouts could elapse while the
motor adapter only suppressed the final output. Releasing the pause could
therefore reveal a prepared search command immediately.

The relevant observation/guard/adapter source was identical in the previously
recorded deployed `4eda8fe` and this base. No fresh on-board report was read;
the reproduction identifies an executable failure path, not every possible
cause of physical turning in the user's run.

## Repair

- During observation, camera voting and fresh sensor/gyro acquisition continue,
  but route navigation does not advance or build motion/capture commands.
  Short line-confirmation accumulators are cleared. Existing invalid-gyro
  withdrawal still runs.
- Mode-3 phase/selection deadlines exclude the actual observation interval.
  They retain their remaining driving time, rather than spending it during
  the stop or granting a complete new timeout. Vision freshness stays on the
  real clock.
- The motor adapter applies the same zero targets during the pause without
  advancing the search helper. On resumption it resets that helper and the
  shared recovery once, keeps the freshly supplied gyro sample, and calculates
  from the current GPIO reading. It preserves the route's confirmed direction
  and stopped-road heading reference.
- Lone inner contacts remain real line evidence even when they oppose the
  pending arrow. The shared slow follower supplies normal forward correction.
  Opposing outer-branch rejection, true all-white search and selected-side
  branch capture remain available.

The 26% gate, fixed two seconds, unconfirmed 500-ms retry, confirmed repeat
inhibition, encoder CPS targets, 55/65-degree mode-3 exit and natural completion
are unchanged. Mode 4 still deliberately starts its separate fixed-angle
entry turn after a confirmed pause; it shares the cleaned observation handoff,
not a change to its trajectory. Modes 1/2/5, K210, RGB and horn are unchanged.

## Verification

Passed full `cmd /c tests\sign_line\run.cmd`, including:

- R/L observation on a crossbar and after an existing PROBE; no navigation
  progression while stationary, live inner correction at resumption, preserved
  confirmed branch capture, clock wrap and STOP;
- a nearly expired PROBE retains its remaining driving time across a full
  pause, then still expires at the existing bounded limit;
- manual STOP issued during observation cannot restart motion at the deadline;
- true line-loss search, opposing outer contact, late direction choice, mode-4
  fixed-angle entry/fallback, 55/65-degree exit, natural departure and cleanup;
- shared slow-profile parity, actual observation duration/gate/retry, MPU/bus,
  K210 helpers, mode selection, trace and horn.

Mode1/2, gyro and mode5 integration checks, canonical state checker and
`git diff --check` passed. Formal `build_unified_motion.ps1` passed:
text/data/bss **115728/64/23544**; BIN **115796 bytes**.

BIN SHA256: `487D19DB10CCB17942F0613B59E3561E3F94FB26431DDFE57ECC9AE0F9182CC5`

HEX SHA256: `287F91F3F2655D8964728CFC959E642A11348A974C117242A26BDB1B057E47F6`

No serial access, flash, lifted-wheel/ground test, push, integration merge or
PROJECT_STATE edit was performed by this worker. The physical sensors can
still genuinely lose the line; this change does not disable real search.
Integrate only the new delta onto the comprehensive candidate and recheck
any concurrent changes, preserving main's different controller baseline.
