"""Composite mode5 shares mode1 bypass with isolated fast following."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[2]
main=(ROOT/"Core/Src/main.c").read_text(encoding="utf-8")
assert "APP_MODE_VISION_LINE_V4" not in main
assert "vision_line_v4_task" not in main
assert "VisionLineV4Control_Step" not in main
assert "fixed_bypass_mode = (app_mode == APP_MODE_FIXED_BYPASS);" in main
assert "line_tracking_set_fast_follow(fixed_bypass_mode);" in main
assert "line_tracking_set_straight_boost(1U);" in main
assert "if ((app_mode == APP_MODE_INTEGRATED || app_mode == APP_MODE_FIXED_BYPASS))" in main
assert "if ((mode == APP_MODE_INTEGRATED || mode == APP_MODE_FIXED_BYPASS))" in main
assert "APP_MODE_FIXED_BYPASS" in main[main.index("WheelSpeedObserver_Task();")-250:main.index("WheelSpeedObserver_Task();")]
for path in ("Core/Src/line_obstacle_bypass.c","Core/Src/line_bypass_travel.c",
             "Core/Src/line_bypass_range.c","Core/Src/ultrasonic_avoid.c",
             "Core/Src/motorPWM.c","Core/Src/wheel_encoder.c","K210/sign_mode34.py"):
    anchor = "348fa41" if path in ("Core/Src/line_obstacle_bypass.c", "Core/Src/line_bypass_travel.c") else "fd8472b"
    expected=subprocess.check_output(["git","show",anchor+":"+path],cwd=ROOT).replace(b"\r\n",b"\n")
    assert (ROOT/path).read_bytes().replace(b"\r\n",b"\n")==expected,path
print("PASS: composite modes1/5 share rolling fixed bypass, mode5-only fast profile; range/hardware unchanged")
