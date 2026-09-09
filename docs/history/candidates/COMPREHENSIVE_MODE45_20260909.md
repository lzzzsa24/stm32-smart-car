# Composite mode4 route and mode5 fast-follow integration

Base: fd8472beb296f436b1b99c7ea88a152d550f5f7f.
Requested sources: 852ade1bb078297851bb651f2714edddc599417b and
fb311e31de3673f71e865fe808b230f48efa4953.
Rollback: rollback/2026-09-09-before-comprehensive-mode45.

Mode1 retains composite fixed250/300-mm bypass and its normal tracking speed.
Mode2 retains pure line tracking. Mode3 retains fd8472b exit release.
Mode4 now uses852ade1 turn/straight-entry/live-arc/turn/straight-exit route.
Mode5 replaces visual line control with the same fixed bypass as mode1,
enabling fb311e3 fast-follow and fast turn assistance only in normal mode5
tracking. K210 source is unchanged and is not required for mode5.

The mode3/mode4 merge retains mode3 arc-peak reset while mode4 measures ARC
origin at physical line reacquisition. The renamed45-degree capture constant
also supplies mode4's original45-degree minimum exit turn; mode3's new
10/15-degree heading-alignment thresholds are not substituted into mode4.

Unlike the source main rc.5 branch, both composite modes1/5 already share the
same fixed-bypass thresholds, IR-off setting and geometry. No legacy main
mode1 profile or older main sign controllers were imported. Remote0 STOP,
mode switching and bypass ownership remain higher priority. The fast profile
resets on every tracking/mode reset; it is re-enabled only by mode5 following.

Full sign, line-recovery (both speeds), gyro/bypass, archived-vision and audio
host suites, composite mode5 integration checks and formal ARM build are the
verification targets. These do not establish real speed, slip or route accuracy.
No serial access, flashing, K210 deployment or GitHub push in this task.
