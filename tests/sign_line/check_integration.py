from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[2]
main = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8")
irq = (ROOT / "Core/Src/stm32f1xx_it.c").read_text(encoding="utf-8")
k210 = (ROOT / "K210/sign_mode34.py").read_text(encoding="utf-8")
manifest = json.loads((ROOT / "K210/model-manifest.json").read_text(encoding="utf-8"))
model = ROOT / "K210/road_sign_det_20260906.kmodel"

assert "return APP_MODE_SIGN_LINE_ADVANCED;" in main
assert "return APP_MODE_SIGN_LINE_SIMPLE;" in main
assert "Figure8Encoder_Start();" not in main
assert "SquareEncoder_Start();" not in main
assert "sign_line_task(app_mode);" in main
assert main.index("sign_line_slowdown_task(app_mode);") < main.index("if (service_bounded_line_wait(app_mode))")
assert "SignSlowdown_ObserveDetection(&detection, HAL_GetTick());" in main
assert "line_reading_mask(&line) == 15U" in main
assert "LineSensorSample_TakeAllBlack" in main
transition = main[main.index("if (requested_mode != app_mode)"):main.index("sign_line_slowdown_task(app_mode);")]
assert "SignSlowdown_Reset();" in transition
assert "DriveBase_SetSpeedLimitCps(0L);" in transition
sign_task = main[main.index("static void sign_line_task(AppMode mode)\n{"):main.index("static AppMode read_requested_mode(AppMode current_mode)\n{")]
assert "line_tracking_compute(" not in sign_task
assert "SimpleLine_StepSlow(" in sign_task
assert "SignRoute_UpdateEncoders(" in sign_task
assert sign_task.index("SignRoute_Step(") < sign_task.index("SimpleLine_StepSlow(")
assert sign_task.index("SimpleLine_SetDirection(") < sign_task.index("SimpleLine_StepSlow(")
assert sign_task.count("SimpleLine_StepSlow(") == 1
assert "SimpleLine_StepArc(" not in sign_task
assert "SimpleLine_Step(" not in sign_task
assert "SimpleLine_Stop(" not in sign_task  # don't reset away current line evidence
slow_task = main[main.index("static void sign_line_slowdown_task(AppMode mode)\n{"):main.index("static void sign_line_task(AppMode mode)\n{")]
assert "if (SignHorn_Observe(&detection, HAL_GetTick()))\n      (void)BuzzerPhrase400_Start(5U);" in slow_task
assert "SignHorn_Reset();" in transition
assert "DriveBase_SetSpeedLimitCps" not in slow_task[slow_task.index("sign_slow_reasons = SignSlowdown_Reasons"):]
assert "SignSlowdown_TargetLimit(sign_slow_reasons, left_pwm, right_pwm)" in main
assert "route_status.searching && route_status.state != SIGN_ROUTE_PROBE" in main
wait_task = main[main.index("static uint8_t service_bounded_line_wait(AppMode mode)\n{"):main.index("int main(void)")]
enable = wait_task[wait_task.index("uint8_t enabled"):wait_task.index("uint8_t paused;")]
assert "SIGN_LINE" not in enable
assert 'SIGN3 SL2 RING NAV START' in main and 'SIGN4 SL2 RING NAV START' in main
assert "void USART2_IRQHandler(void)" in irq
assert "vision_uart_irq_handler();" in irq
assert "THRESHOLD      = 0.2" in k210
assert 'KMODEL_PATH    = "/sd/KPU/road_sign_det/road_sign_det.kmodel"' in k210
assert 'print("SIGN34 ready;' in k210
assert model.stat().st_size == manifest["bytes"]
assert hashlib.sha256(model.read_bytes()).hexdigest() == manifest["sha256"]
compile(k210, str(ROOT / "K210/sign_mode34.py"), "exec")
print("PASS: mode 3/4 bindings, USART2 IRQ, preserved sign script and model identity")
