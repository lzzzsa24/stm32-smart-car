# First-frame observation crawl

Continuation of b357a4d, refreshed with main e359ee7 documentation.
Current recorded hardware: STM32 771586e, K210 0987233.

Change only the K210 detection threshold from 0.20 to 0.15. On STM32,
any existing valid positive detection immediately activates a 500 CPS
forward peak limit for the existing 1500 ms freshness hold. New sequences
renew the hold; repeated reads/no-target frames do not. Vision takes priority
over the 700 CPS black-bar cap. Ordinary forward cap remains 1200 CPS.
Keep proportional wheel targets and existing lost-line search behavior.
No new stop/timer/motor owner. Direction still requires the existing >=20
score and multiple-frame confirmation; lowering detection threshold alone
does not authorize weaker steering votes.

Verification: complete local sign_line host suite, K210 mocked runtime and
formal ARM build pass. No flash, serial or physical test in this change.
The 500 CPS setting is an unvalidated ground parameter, not proof of better
recognition. Model weights and camera settings are unchanged.

Integration into comprehensive 771586e: apply only this final delta.
Preserve its newer single-highest left/right/horn K210 selection and runtime
tests. For K210 apply the one THRESHOLD assignment change to that script;
do not copy this older main-based script wholesale. Resolve test context
differences by retaining highest-only/horn checks and updating threshold
expectations to .15. Rebuild/test the comprehensive source before deployment.
Both board updates are necessary for the complete new behavior.
