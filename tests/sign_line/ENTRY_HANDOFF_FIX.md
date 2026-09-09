# Branch search and verified line handoff

role: branch implementation and host verification, no hardware access
start_commit: 12fd7f2; refreshed with main 33f15c6 in b497c79
recorded_deployed_source: a56f6c7 (12fd7f2 integrated)
files_changed: sign_route.h/.c, simple_line_mode.h/.c, main.c sign output adapter,
  tests/sign_line/check_integration.py, run.cmd, test_entry_handoff.c, this note

## Problem and behavior

The audited 12fd7f2 path retained PROBE R but handed an opposite-only left
sensor to the ordinary follower, producing 800/2300 weights (417/1200 CPS)
and then -2700/+2700 search on white. Merely clearing route.active therefore
did not retain branch choice at the final motor owner.

Before a chosen branch is captured, confirmed PROBE/SELECTING with valid yaw
classifies opposite-only masks (1/2/3 for left; 4/8/12 for right) as unresolved
line search. These samples cannot overwrite the chosen direction or reset the
trusted yaw anchor. White also cannot generate forward entry travel.
The existing powered search sweeps only between the last trusted heading and
25 degrees on the chosen side. It returns within that same sector at the
boundary; a reversal is a bounded return, not a new opposite branch choice.
New confirmation also clears an already active wrong-direction search sweep.

Seeing the selected sole outer or adjacent pair, followed by mask 6 for 30 ms
with at least 15 degrees of directed entry yaw, marks entry_line_ready. A
sample gap >50 ms or interrupted center sequence restarts center confirmation.
All-black/split/wrong-side/center-alone cannot establish this capture.
After capture, ordinary line curvature regains control, including curvature
opposite the sign. The route no longer reclaims a forward pivot while waiting
for its ARC state transition. A bare direction sign does not cause a forward
turn across white, and no entry movement is forced to reach 60 degrees.

The existing ARC state gate (60 degrees in PROBE), exit angles/distances and
timeouts remain unchanged. Entry_line_ready is a motor handoff condition,
not proof of ARC state or successful circle traversal. If the physical entry
does not meet that existing phase gate, it can still time out to CANCEL.
Manual STOP, fixed observation pause, invalid-yaw withdrawal and route cancel
retain priority. K210 threshold, pause/retry settings, RGB/horn, MPU and public
DriveBase implementation are unchanged. Only modes 3/4 call this adapter.

## Verification

SimpleLine_ResolveRouteOutput now implements the production main-loop output
priority in one pure function; the new host test calls it after the real route
and follower code, rather than asserting only route.active/direction fields.
Full sign_line suite passed, including production IMU configuration, mirrored
audited sequence, 200-sample bounded search, unchanged anchor under opposite
contact, interrupted/gapped capture, early line handoff, ARC continuation,
late confirmation during a prior sweep, pause/manual STOP/cancel and tick wrap.
K210 mocked runtime, gyro service/bus, trace and horn regressions also passed.
Formal ARM build: text/data/bss 106172/64/18304 bytes.
HEX SHA256: 44D1D14A7D638F646AB9D0CF837821758DDB277B5DFDA393BB67227EF3043EC0.

not_verified: physical motion, braking/overshoot, correct half selection and
  circle exit on the floor; no serial, flash, push or main merge performed.
risks_or_assumptions: the 15/25-degree capture/search parameters are ground
  candidates; inertia/slip can exceed commanded angular bounds. Sensor masks
  alone cannot prove which physical branch supplied a line.
integration_notes: apply only the final delta onto a56f6c7 or its successor;
  rebuild all consumers of the extended status/controller structs. Keep the
  shared ResolveRouteOutput call (and tests), STOP/pause priority, horn/trace
  tail and exclusion of modes 3/4 from automatic wait recovery. Do not replay
  the older comprehensive prerequisite commits.
