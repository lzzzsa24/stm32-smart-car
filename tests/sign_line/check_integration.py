from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[2]
main = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8")
irq = (ROOT / "Core/Src/stm32f1xx_it.c").read_text(encoding="utf-8")
k210 = (ROOT / "K210/sign_mode34.py").read_text(encoding="utf-8")
manifest = json.loads((ROOT / "K210/model-manifest.json").read_text(encoding="utf-8"))
model = ROOT / "K210/road_sign_det_20260906.kmodel"

assert "return APP_MODE_SIGN_LINE;" in main
assert "return APP_MODE_SIGN_GYRO_TANGENT;" in main
assert "APP_MODE_RESERVED4" not in main
assert "Figure8Encoder_Start();" not in main
assert "SquareEncoder_Start();" not in main
assert "sign_line_task(app_mode);" in main
assert main.index("sign_line_detection_task(app_mode);") < main.index("if (service_bounded_line_wait(app_mode))")
assert "SignRoute_ObserveDetection(&detection);" in main
assert "SignSlowdown" not in main
transition = main[main.index("if (requested_mode != app_mode)"):main.index("sign_line_detection_task(app_mode);")]
assert "DriveBase_SetSpeedLimitCps(0L);" in transition
sign_task = main[main.index("static void sign_line_task(AppMode mode)\n{"):main.index("static AppMode read_requested_mode(AppMode current_mode)\n{")]
assert "line_tracking_compute(" not in sign_task
assert "SimpleLine_StepRoute(" not in sign_task
assert "SimpleLine_ResolveRouteOutput(" not in sign_task
assert "SignLineFollow_Step(&sign_line_controller, &line, EXP7_LINE_SPEED," in sign_task
assert "if (mode==APP_MODE_SIGN_LINE)" in sign_task
assert "SignObservation_UpdateLine(sign_line_mask,now);" in sign_task
assert "SignRoute_MarkObservationSearch();" in sign_task
assert sign_task.index("SignObservation_UpdateLine(") < sign_task.index("SignObservation_Paused(now)")
assert "SignObservation_HoldingRoute(now) : observation_paused, now);" in sign_task
assert sign_task.index("SignObservation_Paused(now)") < sign_task.index("SignLineFollow_Step(")
assert "&route_status, &route_command, observation_paused)" in sign_task
assert sign_task.count("SignLineFollow_Step(") == 1
assert sign_task.index("SimpleLine_UpdateYaw(") < sign_task.index("SignLineFollow_Step(")
assert "SimpleLine_SetDirection(" not in sign_task
assert "SignRoute_UpdateEncoders(" in sign_task
assert sign_task.index("SignRoute_Step(") < sign_task.index("SignLineFollow_Step(")
assert "SimpleLine_Stop(" not in sign_task  # don't reset away current line evidence
detection_task = main[main.index("static void sign_line_detection_task(AppMode mode)\n{"):main.index("static void sign_line_task(AppMode mode)\n{")]
# Power rollback: keep the established encoder speed interface as the owner.
assert "DriveBase_SetSignLowSpeedMode" not in main
drive = (ROOT / "Core/Src/drive_base.c").read_text(encoding="utf-8")
assert "sign_low_speed_mode" not in drive
assert "low_speed_budget" not in drive
assert "DRIVE_SIGN_POWERED" not in drive
adapter = (ROOT / "Core/Src/sign_line_follow.c").read_text(encoding="utf-8")
tracking = (ROOT / "Core/Src/line_tracking.c").read_text(encoding="utf-8")
assert "line_tracking_start_following();" in adapter
assert "line_tracking_compute_slow(reading, base_speed, &output)" in adapter
assert "line_tracking_compute_arc(reading, base_speed, &output)" in adapter
assert "line_tracking_apply_command(&output, MOTOR_PWM_PERIOD);" in adapter
assert "line_tracking_make_route_command(" in adapter
assert "line_tracking_make_route_spin_command(steer, base_speed, &output)" in adapter
assert "SignSlowdown" not in adapter
assert "override = route_command->active || guarded_search;" in adapter
assert adapter.index("if (paused)") < adapter.index("SimpleLine_StepRoute(")
assert "center_search=mode3 && SignObservation_SeekingLine()" in adapter
assert "LineRecovery_StepCentering" in adapter
assert "guarded_search = c->guard.mode == SIMPLE_LINE_SEARCH &&\n      c->guard.entry_guard_active;" in adapter
assert "line_tracking_set_middle_guard(mode3);" in adapter
assert "line_action=line_tracking_compute(reading,base_speed,&output);" in adapter
assert "gyro_arc && (mask == 0U || symmetric_arc_contact(mask))" in adapter
assert "SIGN_FOLLOW_OWNER_ARC_FALLBACK" in adapter
assert "line_tracking_compute_arc_fallback(reading,base_speed," in adapter
route = (ROOT / "Core/Src/sign_route.c").read_text(encoding="utf-8")
gyro_arc = route[route.index("if (route.state == SIGN_ROUTE_ARC)"):route.index("if (route.state == SIGN_ROUTE_EXIT_SELECT)")]
assert "tangent_command(command" not in gyro_arc
assert "SignObservation_ObserveDetection(&detection, HAL_GetTick()," in detection_task
assert "mode == APP_MODE_SIGN_LINE ? SIGN_OBSERVATION_MODE3_SCORE_MINIMUM :" in detection_task
assert "SIGN_OBSERVATION_MODE4_SCORE_MINIMUM);" in detection_task
assert "SignObservation_AllowPause(observation_route.direction == 0 &&" in detection_task
assert "SignObservation_Reset();" in transition
assert "forward_limit_cps" not in adapter
assert "pwm_motor" not in adapter and "DriveBase_SetSideCps" not in adapter
assert "DriveBase_PrepareLineTurnAssist(left, right);" in tracking
assert "DriveBase_SetSideCps(left, right);" in tracking
assert transition.count("SignLineFollow_Start(&sign_line_controller);") == 2
assert "pwm_motor" not in sign_task
assert "if (SignHorn_Observe(&detection, HAL_GetTick()))\n      (void)BuzzerPhrase400_Start(5U);" in detection_task
assert "SignHorn_Reset();" in transition
assert not (ROOT / "Core/Src/sign_slowdown.c").exists()
assert not (ROOT / "Core/Inc/sign_slowdown.h").exists()
assert "(uint8_t)route_status.state," in main
assert "(uint8_t)SIGN_ROUTE_SEARCHING" not in main
wait_task = main[main.index("static uint8_t service_bounded_line_wait(AppMode mode)\n{"):main.index("int main(void)")]
enable = wait_task[wait_task.index("uint8_t enabled"):wait_task.index("uint8_t paused;")]
assert "mode != APP_MODE_SIGN_LINE" in enable
assert "mode != APP_MODE_SIGN_GYRO_TANGENT" in enable
assert 'SIGN3 KEY2 RING NAV START' in main
assert 'SignRoute_SetProfile(SIGN_ROUTE_PROFILE_STANDARD);' in main
assert 'SignRoute_SetProfile(SIGN_ROUTE_PROFILE_GYRO_TANGENT);' in main
assert 'SIGN4 GYRO TANGENT ARC START' in main
assert '"M4 GYRO"' in (ROOT / "Core/Src/oled_status.c").read_text(encoding="utf-8")
assert 'route_command->gentle_arc' in adapter
assert 'line_tracking_make_slow_arc_command(steer, base_speed, &output)' in adapter
assert 'DiagnosticUart_WriteUnsigned((uint32_t)route_status->profile);' in main
assert "void USART2_IRQHandler(void)" in irq
assert "vision_uart_irq_handler();" in irq
assert "THRESHOLD      = 0.15" in k210
assert 'KMODEL_PATH    = "/sd/KPU/road_sign_det/road_sign_det.kmodel"' in k210
assert 'print("SIGN34 ready;' in k210
assert model.stat().st_size == manifest["bytes"]
assert hashlib.sha256(model.read_bytes()).hexdigest() == manifest["sha256"]
compile(k210, str(ROOT / "K210/sign_mode34.py"), "exec")
print("PASS: mode 3 line-feedback and mode 4 gyro-tangent bindings, USART2 IRQ, preserved sign model")
