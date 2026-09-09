"""Verify mode1-only promotion retains legacy sign/visual modes."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
main = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8")
bypass = (ROOT / "Core/Src/line_bypass_turn.c").read_text(encoding="utf-8")
mpu_header = (ROOT / "Core/Inc/mpu6050_yaw.h").read_text(encoding="utf-8")
route_config = (ROOT / "Core/Inc/sign_route_config.h").read_text(encoding="utf-8")

assert main.count('#include "mpu6050_yaw.h"') == 1
assert main.count("MpuYaw_Init(HAL_GetTick());") == 2  # boot plus explicit c
assert main.count("MpuYaw_Task(HAL_GetTick(), stationary);") == 1
assert "GyroTurn_ClearFault()" in main
assert "if (stationary) (void)GyroTurn_ClearTransientFault();" in main
assert "case 'g':" in main and "case 'c':" in main

runtime = main[main.index("while (1)"):]
assert runtime.index("MpuYaw_Task(HAL_GetTick(), stationary);") < runtime.index(
    "if (requested_mode != app_mode)"
)
assert "requested_mode = APP_MODE_STOPPED;" not in runtime
assert "imu_generation" not in runtime
assert "uint8_t enabled = mode == APP_MODE_INTEGRATED || mode == APP_MODE_FIXED_BYPASS;" in main
assert "if (mode != APP_MODE_INTEGRATED && mode != APP_MODE_FIXED_BYPASS) return service_legacy_line_wait(mode);" in main
assert "LineBypassTurn_Recover();" in main
assert main.index("if (service_bounded_line_wait(app_mode))") < main.index("if (DriveBase_GetFaultMask() != 0U &&")

sign_task = main[
    main.index("static void sign_line_task(AppMode mode)\n{"):
    main.index("static AppMode read_requested_mode(AppMode current_mode)\n{")
]
assert "SignRoute_UpdateYaw(" not in sign_task
assert "MpuYaw_Refresh(" not in sign_task
assert "SimpleLine_StepSlow(" not in main
assert "vision_line_fallback" not in main
gyro = (ROOT / "Core/Src/gyro_turn.c").read_text(encoding="utf-8")
assert gyro.count("MpuYaw_Refresh(now); now = HAL_GetTick();") == 2

assert "#define MPU6050_BYPASS_ENABLED 1" in mpu_header
assert "#define SIGN_ROUTE_REQUIRE_IMU 1" not in route_config
assert "return GyroTurn_Start(angle_mdeg, cps);" in bypass
assert "if (using_gyro) { GyroTurn_Task(); return; }" in bypass
assert "return encoder_Start(angle_mdeg, cps);" in bypass

print("PASS: mode1 gyro/recovery, legacy other-mode dispatch, no sign/visual candidate leakage")
