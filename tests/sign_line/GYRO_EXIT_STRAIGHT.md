# Confirmed branch entry and gyro exit alignment

Started from canonical main 44dcb69 and merged the deployed comprehensive
20e8c72 as the test baseline. Final functional delta is confined to sign_route
and its configuration/tests; no main, drive, K210, MPU, horn or other-mode
control changes relative to the deployed source.

Entry: after junction/edge-or-loss evidence, PROBE/SELECTING uses the confirmed
side's forward pivot until 60 degrees of directed phase yaw. Opposite contact
and white no longer let SimpleLine reverse that entry command. A centered
approach after a crossbar does not initiate the pivot; all-black remains
ambiguous. Existing 120/135 degree and time cancellation limits remain. At
angle-qualified centered capture, ARC resumes ordinary live line following.

Exit: valid gyro ARC phase >=170 degrees plus >=150 mm encoder travel sustained
30 ms starts EXIT_SELECT regardless of sensor mask. It forward-pivots toward
the original approach heading; >=45 degrees exit rotation and heading error
within +/-10 degrees enters EXIT_CLEAR even when all sensors are white.
EXIT_CLEAR commands equal forward targets until stable centered line after
60 mm. At 2000 ms or >250 mm, cancel the route and release ordinary search.
Crossbars/both-outer patterns cannot count as successful centered capture.
Manual STOP and the existing first-frame observation pause retain priority.

The phase angle is not total absolute yaw: entry, semicircle and exit have
different origins. Direct straight travel before heading alignment would
leave along the circle tangent rather than the outgoing stem.

Verification: full sign_line suite including gyro service/bus, mirrored entry,
opposite/white entry, centered approach, angle-triggered exit, white alignment,
bounded straight failure, invalid gyro, trace/horn/K210 and clock wrap; formal
ARM build. No serial, flash or physical driving test. 170 degrees / 250 mm
are uncalibrated track parameters. Missed PROBE/junction remains a separate
limitation; this does not turn solely because a sign appears on a straight.

Integration: cherry-pick only the final delta onto 20e8c72 or its successor;
do not replay historical prerequisite commits. Preserve the composite pause,
gyro readiness checks, trace and exclusion from old automatic wait recovery.
