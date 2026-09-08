# Slow outer-edge correction and silent line recovery

Base: canonical main f17bffa97c17c119b5c144bf833c3b51b4f90abd.
The recorded board firmware is separate candidate c5a4c76. This change includes
that candidate's prerequisite source changes, plus the corrections below, on
current main. Main's 3170221 mode-1 configuration is retained. This is not a
merge or deployment in the canonical checkout.

## Problem and correction

The user reports that one outer LED makes the car rush forward while multiple
LEDs are much slower, and that outer-edge steering is too weak. The user also
requests no automatic buzzer during line loss.

The old visible helper converted equivalent PWM 2200/3000 into 1412/5273 CPS.
KEY1's gain could raise the outer request further. Mean forward target was
3342.5 CPS versus 1412 CPS for a broad mark. This is an inappropriate speed
profile for the largest lateral displacement, not a reason to speed through
ambiguous interference marks.

Targets now use explicit CPS for outer and same-side adjacent patterns:

| Visible pattern (physical order X2 X1 X3 X4) | Left CPS | Right CPS |
|---|---:|---:|
| X2 only (mask 2) | 0 | 2200 |
| X4 only (mask 8) | 2200 | 0 |
| X2+X1 (mask 3) | 1412 | 2400 |
| X3+X4 (mask 12) | 2400 | 1412 |

The lone-outer mean target is 1100 CPS. Stopping the inside side provides a
tighter forward pivot than driving it forward at the old minimum. Neither side
is commanded backward on visible outer contact. Same-side pairs use a gentler
two-side forward arc. The fixed outer/pair targets are not raised by gain 200;
caller speed caps can still reduce them. Centered cruise and the conservative
broad/nonadjacent interference handling are unchanged. Crossing guard priority
and the user-selected 60/100-ms short-white-gap confirmation remain.

LineRecovery no longer starts, repeats, stops or updates the phrase player.
Confirmed all-white loss still uses equal/opposite search targets and fresh
direction evidence; it is now silent. Repeated live middle observations still
confirm capture across 4 ms. The new read-only IsSearching query lets regression
tests inspect that state without wrongly using audio as the capture indicator.
Manual audio and unrelated obstacle/fault warnings are not muted. Remote STOP
and other motion owners retain priority; there is no new timeout stop.

DriveBase's explicitly prepared turn-assist claim now accepts one zero side.
The stopped wheels still get no supplement because their targets are zero;
moving wheels retain the existing bounded, expiring load assistance. No gains,
PWM limits, motor mapping or generic speed-control implementation were changed.
This uses the user's existing authorization for supporting drive_base changes.

## Verification

All completed successfully in the independent worker:

```powershell
cmd /c tests\line_recovery\run.cmd
cmd /c tests\sign_line\run.cmd
cmd /c tests\vision_line_v4\run.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File .\build_unified_motion.ps1
git diff --check
```

- 120 visible-mask/gain/state cases at each configured search speed, retaining
  narrow gaps, fresh-direction history, ISR ordering and capture tests.
- Real DriveBase old 1412/5273 conversion reproduced. Sixteen mirrored
  gain/smoothing/search-state profiles check exact outer/pair targets, lower
  outer mean speed than pairs/center/broad marks, and four-wheel PWM direction.
- Moving-side load assistance under lag raises PWM 2787 -> 3387 in each of the
  four wheel positions, while the stopped side remains at zero PWM.
- Ninety-second searches produce zero buzzer attacks. False contacts, capture,
  re-loss, STOP/fault and tick wrap are still tested. Manual phrase timing and
  reset independence remain verified. Sample-level silence assertions cover
  the rest of the line suite outside the explicit manual-audio test.
- Existing cap, brake/position/STOP ownership, real bypass and sign/vision
  regressions pass. These are host tests, not physical motor or track evidence.

Formal text/data/bss: 84496/64/11536; BIN size: 84564 bytes.
BIN SHA256: BA3A558FBB5FD9666113EF5CC31E3404F38E383B24FCB05B4B5A4D54B2EB6A61
HEX SHA256: AD98B93B9971B44FF3A81900123381F27443C86C0FEED801411305CFFB273909

## Integration and limits

Apply the final combined commit once; it already includes c5a4c76/074ef682
prerequisites. Do not separately reapply those patches without reviewing the
overlap. Bypass continuous-travel candidate 68c6444 is independent and is not
included; preserve/apply it separately if selected and rebuild the actual
combined main. No PROJECT_STATE edits, COM access, flashing, reset, lifted-wheel
or ground test occurred here. Target speeds/radius changes do not prove actual
chassis motion; acceleration ramps, friction, slip and loose track material
still need ground validation. Integration/deployment belongs to 黑线避障.
