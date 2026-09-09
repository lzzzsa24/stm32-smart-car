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
assert "line_tracking_set_straight_boost(1U);" in integrated
assert integrated.index("line_tracking_set_straight_boost(1U);") < integrated.index("line_tracking_follow_once(")
assert main.count("line_tracking_set_straight_boost(") == 1, "boost must be mode-1-only"
assert "ultrasonic_forward_speed_limit" in integrated
assert "line_tracking_compute(" not in integrated
runtime = main[main.index("sign_line_detection_task(app_mode);"):]
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
print("PASS: KEY1/KEY2 share tracking cycle; only KEY1 opts into straight boost; bypass/ultrasonic ownership and rejoin retained")
assert main.count("bypass_config.fixed_route_direction = 1;") == 1
assert 'DiagnosticUart_WriteUnsigned(telemetry.fixed_route_phase)' in main
print("PASS: only mode-1 bypass enables the fixed right-hand rectangle and exposes phase telemetry")
assert "#define EXP7_IR_AVOID_ENABLED              0U" in main
assert "bypass_config.infrared_enabled = EXP7_IR_AVOID_ENABLED;" in main
assert "if (EXP7_IR_AVOID_ENABLED && !ir_avoid_calibrate())" in main
assert "if (EXP7_IR_AVOID_ENABLED && confirmed_ir_bypass_direction" in main
ir_off = block(main, "if (!EXP7_IR_AVOID_ENABLED)")
assert "ir_avoid_set_enabled(false);" in ir_off
for side in ("LEFT", "RIGHT"):
    assert f"HAL_GPIO_WritePin(IR_{side}_ENABLE_GPIO_Port, IR_{side}_ENABLE_Pin, GPIO_PIN_SET);" in ir_off
assert "ir_avoid_init();" in main and "BatteryMonitor_Init();" in main
assert "IrRemote_Init();" in main and "IrRemote_EXTI_Callback(GPIO_Pin);" in main
status = main[main.index("/* KEY1/KEY2 保留红外状态灯") - 180:]
assert "EXP7_IR_AVOID_ENABLED" in status[:180]
print("PASS: obstacle IR disabled at emitters/calibration/trigger/display; shared battery ADC and remote STOP retained")
import re
for name,value in (("STOP_CM",20),("CLEAR_CM",35),("EMERGENCY_MAX_CM",30),("LOOKAHEAD_MS",160),("BYPASS_STOP_CM",15)):
    assert re.search(r"#define EXP7_ULTRASONIC_"+name+r"\s+"+str(value)+r"U\b",main)
assert "LineBypassRange_Task(" in bypass
assert "LineObstacleBypass_GetState() == LINE_BYPASS_DRIVING" in bypass
assert "EXP7_ULTRASONIC_BYPASS_STOP_CM" in bypass
assert "LineBypassRange_Reset();" in transition
assert main.count("LineBypassRange_Reset();") == 3
print("PASS: earlier approach thresholds and reset/forward-only bypass range ownership")
