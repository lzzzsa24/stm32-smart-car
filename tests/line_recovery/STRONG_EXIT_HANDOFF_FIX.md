# Strong exit evidence across provisional inner contact

Base: canonical main eee8773396e25d0e0e12fc952eaa80b36a46e222 (all-mode
v1.2.0-rc.1 source). No old worker patches need to be separately reapplied.
Main already contains 4947f9c plus 1b42805's lone-inner ambiguity/encoder probe
changes. The deployment record still identifies 4947f9c on the board.

## What changed

1. Active recovery now preserves its pending strong outer direction while a
   middle contact is still unconfirmed. A subsequent all-white observation
   applies that direction within the existing 200-ms age limit. Middle contact
   cannot renew the timestamp. A newer outer supersedes it; conflicting outer
   patterns, actual live capture, reset and commit invalidate it. Capture still
   requires repeated valid live observations and keeps the actual scan side.
2. Nonadjacent masks remain ambiguous as static snapshots. Ordered transitions
   X1-only (1) -> X1+X4 (9) and X3-only (4) -> X2+X3 (6) record the newly
   appearing outer side when the preceding observation is no more than 30 ms
   old. They update direction memory before crossing handling, not motor
   permission. The existing broad-mark forward priority is retained.
3. The previous raw observation is cleared on mode/reset and queue overwrite.
   An intervening different pattern breaks the transition; repeated static
   overlap does not renew the inferred hint. Existing 200/400-ms hint ages and
   stable-center rules remain in effect. LSEARCH hint_mask=9/6 identifies the
   accepted transition, while edge retains its narrow-outer-only meaning.

The first suggested fix (a weak lone-inner pulse must not immediately erase a
strong hint) was already present in the new main. It is preserved and explicitly
regression-tested here rather than replacing that controller with an old copy.
The existing expanding encoder probe, all-mode integration, motor mapping,
target speeds, acceleration ramp and sensor sampling interval are unchanged.

## Reproduction and validation

Before the new changes, 8/8 active outer -> provisional inner -> white cases
kept the wrong side. Ordered nonadjacent overlap failed 2/4 cases (the old
default-left happened to satisfy the left examples). See strong-before.log.

After the fix, the expanded matrix passes 24/24 active cases (both directions,
X1/X3/both-inner contacts, direct and ISR history, tick wrap) and 4/4 ordered
overlap cases. Additional cases cover weak bounce, stale pending exits, newest
outer priority, real capture and re-loss, reset, >30-ms gaps, intervening broad
patterns, static overlap expiry, queue overwrite and transition across wrap.

The actual DriveBase source with simulated encoders/PWM passes both directions
for provisional inner contact and ordered overlap during an active opposite
search. Four wheel targets and PWM signs after the existing ramp agree; broad
guard and operator STOP still work. These are host simulations, not ground tests.

All existing line-recovery/load/bypass tests pass at 2493 and 1870 CPS, including
the 16-mask/256 mirrored-pair checks, GPIO/ISR ordering and encoder-bounded
ambiguous search. Sign-line and visual-line v4 suites pass. Formal ARM build:
text/data/bss 84560/64/11544 bytes; BIN 84628 bytes. git diff --check passes.

## Limits and integration

The 30-ms observation gap is a candidate boundary consistent with existing
sensor confirmation gaps; it has not been calibrated on the floor. An oblique
interference mark can also produce the ordered transition. Keeping forward
priority avoids immediately spinning on that mark, but no finite digital
pattern rule perfectly distinguishes every track and interference geometry.
Pulses never sampled cannot be recovered. Existing forward guard and physical
slip/ramp can still contribute to overshoot.

Integrate only this incremental commit on current main and rerun the three host
suites and formal build. Keep the current physical sensitivity settings for
comparison. After separately authorized deployment, test both sharp directions,
the special starting angle, transverse interference, provisional contacts and
STOP. Preserve RAM logs after failures. No serial access, flash, motion,
PROJECT_STATE edit, push or main-branch merge was performed by this worker.
