# Mode3 adjustable exit-start angle

Base: comprehensive6da3ac8. Mode3 uses the independent physical plus/minus keys
beside0, not the direction-pad volume keys. Their mapping was checked against
F:/myproject/jidian/tmp/yahboom_remote_codes.jpg and the older grid-bypass
driver: printed30/70 reverse into NEC command0x0C/0x0E. Existing virtual-key
values are retained, with new ADD12/SUBTRACT13 events.

## Controls and display

- In mode3, each independent '+' press increases the exit-start threshold5deg;
  '-' decreases5deg. Default40deg, clamped30..90deg. Normally smaller starts
  earlier and larger starts later on the increasing upper-half heading.
- Direction-pad up/down continue to adjust audio volume in every mode.
  Plus/minus do nothing in other modes or STOP and never select a driving mode.
  STOP still wins over simultaneous adjustment. Existing NEC repeat suppression
  is retained; use individual presses rather than expecting hold-to-repeat.
- Parameter is RAM-only and survives route resets, STOP and mode changes.
  Restart/power cycle restores40deg. No calibration/Flash write is introduced.
- Changes apply on the next ARC check. An already-started exit is not restarted
  or cancelled when its setting changes.
- OLED row1 shows M3 E40 plus battery: E is the configured threshold in degrees.
  Row2 retains IMU calibration status. Row3 retains VIS and adds H:+35, the
  current signed stopped-reference heading used by the mode3 gate. H:-- means
  no current direction/reference; this is not raw global IMU yaw. Row4 retains
  ROUTE. All fields fit the existing21-character line limit.

## Other reasons exit may not start or steer

The adjustable comparison is inside ARC only. Entry still requires the existing
line/heading evidence; physical entry is not proof that software reached ARC.
An observation hold returns before phase progression. Direction and reference
must remain valid; CAL OK alone does not establish either or prove the stopped
pose aligned with the road. Signed direction must match the actual selected arc.

The existing ARC20s/3000mm and signed-heading +/-120deg cancellation checks run
before the threshold comparison. These are failure protections, not minimum
travel/line/debounce initiation gates. They are unchanged.

After EXIT_SELECT starts, the existing wide/both-side line guard in
select_command can withhold active turning and leave tracking in control.
Thus EXIT TURN on OLED proves phase entry, not an active motor turn on every
mask. This patch audits that behavior but does not silently remove it.

Exit success still requires the selected outer to clear then see black;
alignment/recontact alone only releases steering into EXIT_CLEAR. No change to
that rule, observation20percent/2s,1800-CPS observation search, horn or mode4.

## Verification

Full sign regression includes the actual NEC keymap and extracted application
selector linked to the real route module: physical +/- change only mode3 by5,
clamp both limits, survive reset, retain up/down volume and STOP precedence.
All13 settings30..90 are tested at threshold-minus1mdeg and exact threshold,
both directions and all16 line masks, with zero travel and outer-only completion.
Live lowering in ARC starts exit; further changes do not restart it. Formal ARM
build, mode1/2 and mode5 scope checks are required for handoff.

No serial, remote hardware, OLED, flash or floor verification in this worker task.
