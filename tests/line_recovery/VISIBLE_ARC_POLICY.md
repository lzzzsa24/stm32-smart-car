# Visible-line forward steering and confirmed-loss spin

## Scope and baseline

Worker branch: `fix/line-visible-arc`, based on canonical main
`98780b834d51acf3c82e217a2adf57ea3081dc46`.
This candidate includes the strong-exit prerequisite from
`074ef6827016a9931d8de7f06ab3a35ae291dba2` and the new steering policy together.
Only line implementation, headers, regression tests and their notes change.
PROJECT_STATE.md, shared drive code and main.c are unchanged.

## Resulting behavior

- During ordinary line control, visible line produces forward wheel targets.
  An unambiguous outer contact commands a forward arc, even if it persists;
  the former 12-ms corner latch and LineRecovery_BeginCorner are removed.
- Outer correction uses equivalent PWM 2200 on the inner side and 3000 on the
  outer side at gain 100. Existing turn gain and caller speed caps still apply.
  Provisional inner contact during recovery uses 2200/2400; both-center contact
  uses 2200/2200. These are inputs to the existing CPS conversion and speed loop,
  not direct fixed motor PWM outputs. Normal centered cruise tuning is retained.
- As explicitly requested, the existing 60-ms narrow-center white-gap window
  and 100-ms crossing-tail window remain. Confirmation timing and eligibility
  are unchanged; a brief eligible white gap can still advance before searching.
- Confirmed all-white loss searches with equal/opposite left/right targets
  (default magnitude 2493 CPS). Repeated visible contact instead receives forward
  correction immediately. STOP, braking, faults and other motion owners retain
  their existing authority. The pre-first-line no_line_forward option is retained.
- A visible command does not itself mean capture has succeeded. Active recovery
  still needs repeated live middle observations across at least 4 ms to capture,
  except the pre-existing broad-mark handling. Provisional outer contact alone
  can continue the search melody; confirmed capture stops it. No new waiting
  state, search timeout stop or fixed search angle limit is introduced.
- Fresh outer direction evidence, ordered overlap evidence and pending outer
  direction across unconfirmed middle contact from 074ef682 remain. Existing
  ambiguous-direction encoder sweeps and evidence expiry rules remain.

## Verification

All commands run in the worker root and completed successfully:

```powershell
cmd /c tests\line_recovery\run.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File .\build_unified_motion.ps1
cmd /c tests\sign_line\run.cmd
cmd /c tests\vision_line_v4\run.cmd
git diff --check
```

The line suite exercises default and reduced search speeds, the real DriveBase
speed loop, ownership/brake/fault behavior, evidence history and RAM fault logs.
New coverage checks all 15 visible masks in 120 mask/gain/smoothing/state cases,
left/right forward differential, confirmed-loss counter-rotation, and the
preserved 60/100-ms gaps. Real speed-loop tests cover both turn directions:
center -> white -> outer contact -> white -> confirmed center, checking target
signs immediately and all four simulated PWM directions after the normal ramp.
Prior corner-spin assertions were updated to forward-arc expectations; tests
that need active recovery now establish it through actual all-white loss.
Direction, no-brake, chatter and capture assertions remain.

Formal build: text 84568, data 64, bss 11536; BIN 84636 bytes.
BIN SHA256: BE9820DCB9F9673652F18636CBECA4B9BAAC344577796327873BA86C9429B6E2
HEX SHA256: F89A8BDF6D6F31A08E60F16263497859BA30A3F4DBB47E814E0F33975018686F

## Integration and physical acceptance

Apply the final combined commit once; do not separately apply 074ef682 again.
If the target already contains it, review the overlap rather than applying both
blindly. During this work canonical main advanced to 00079d56 with a deployment
note and also had independent uncommitted mode-1 speed changes in main.c and
state edits. They were not copied or modified here. The integration owner must
preserve those changes and rebuild/test the actual combined main.

No COM port access, flashing, reset, wheel motion or floor test was performed.
Forward/equal-opposite targets do not establish exact physical turn radius:
acceleration ramps, tire slip and loose track material still affect motion.
The integration task owns merge and deployment. Subsequent authorized ground
acceptance should check both outer probes, narrow-line flicker, acute bends,
confirmed-loss in-place search, live recapture/audio stop and remote STOP on
the actual track. Forward arcs may require more space before a sharp bend
loses the line; current sensor sensitivity and straight speed are unchanged.
