"""Bind runtime parity tests to the actual app mode entry/ownership paths."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
main = (root / "Core/Src/main.c").read_text(encoding="utf-8")

def block(text, marker):
    start = text.index("{", text.index(marker))
    depth = 1
    end = start + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]

transition = block(main, "if (requested_mode != app_mode)")
for mode in ("APP_MODE_INTEGRATED", "APP_MODE_LINE_ONLY"):
    entry = block(transition, f"if (app_mode == {mode})")
    assert entry.count("line_tracking_start_following();") == 1
    assert "line_tracking_set_" not in entry, "mode-specific tracking settings drifted"

integrated = block(main, "static void experiment7_integrated_once(void)\n{")
assert "line_tracking_follow_once(line_speed," in integrated
assert "ultrasonic_forward_speed_limit" in integrated
assert "line_tracking_compute(" not in integrated
runtime = main[main.index("sign_line_slowdown_task(app_mode);"):]
pure = block(runtime, "if (app_mode == APP_MODE_LINE_ONLY)")
assert "line_tracking_follow_once(EXP7_LINE_SPEED," in pure
assert "MOTOR_PWM_PERIOD" in pure
assert "line_tracking_compute(" not in pure
assert "LineObstacleBypass_Task" not in pure

bypass = block(runtime, "if (LineObstacleBypass_GetState() != LINE_BYPASS_IDLE)")
assert "LineObstacleBypass_Task(&bypass_input);" in bypass
done = block(bypass, "if (bypass_state == LINE_BYPASS_DONE)")
assert bypass.count("line_tracking_follow_once") == 1 and "line_tracking_follow_once" in done
assert "line_tracking_rejoin_from_bypass(contact);" in done
assert "UltrasonicAvoid_ResumeFollowing();" in done and "configure_ultrasonic_avoid();" not in done
assert bypass.index("LineObstacleBypass_ObserveRawSensors") < bypass.index("LineObstacleBypass_Task")
assert "bypass_input.front_obstacle = bypass_front_obstacle;" in bypass
assert runtime.index("LineObstacleBypass_Task(") < runtime.index("experiment7_integrated_once();")
for marker in ("if (ultrasonic_state != ULTRASONIC_AVOID_FORWARD)",
               "if (ultrasonic_forward_speed_limit <= 0)"):
    assert "continue;" in block(runtime, marker)
    assert runtime.index(marker) < runtime.index("experiment7_integrated_once();")
assert "#define EXP7_VISION_ENABLED                 0U" in main
audio = block(main, "static void update_ultrasonic_buzzer(UltrasonicAvoidState state)\n{")
assert "ULTRASONIC_AVOID_STOPPING" in audio and "ULTRASONIC_AVOID_TURNING" in audio
assert "UltrasonicAvoid_IsNoEchoFallbackActive" not in audio
assert "UltrasonicAvoid_GetLastDistanceCm" not in audio
assert "app_buzzer_safety_write(buzzer, safety_override);" in audio
print("PASS: KEY1/KEY2 share profile and cycle; bypass/ultrasonic gates retain ownership; rejoin clears stale history")
