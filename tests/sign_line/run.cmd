@echo off
setlocal
cd /d "%~dp0\..\.."
if not exist "manual-build-sign-line-host-test" mkdir "manual-build-sign-line-host-test"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if not "%errorlevel%"=="0" exit /b 1
set CL=/DSIGN_ROUTE_REQUIRE_IMU=0
cl /nologo /W4 /WX /utf-8 /std:c11 /DSIGN_PROBE_HOLD_MS=0 /ICore\Inc tests\sign_line\test_sign_line.c Core\Src\vision_detection_parser.c Core\Src\simple_line_mode.c Core\Src\sign_route.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_sign_line.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_sign_line.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /DSIGN_PROBE_HOLD_MS=0 /ICore\Inc tests\sign_line\test_ring_exit.c Core\Src\sign_route.c Core\Src\simple_line_mode.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_ring_exit.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_ring_exit.exe
if not "%errorlevel%"=="0" exit /b 1
rem Existing route regressions isolate post-hold navigation with hold disabled.
rem This executable exercises the actual production 10000-ms configuration.
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\sign_line\test_probe_hold.c Core\Src\sign_route.c Core\Src\simple_line_mode.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_probe_hold.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_probe_hold.exe
if not "%errorlevel%"=="0" exit /b 1
set CL=
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\sign_line\test_observation.c Core\Src\sign_observation.c Core\Src\sign_route.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_observation.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_observation.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /Itests\line_recovery\stubs /ICore\Inc tests\sign_line\test_mode2_follow.c Core\Src\sign_line_follow.c Core\Src\sign_observation.c Core\Src\simple_line_mode.c Core\Src\sign_route.c Core\Src\line_tracking.c Core\Src\line_recovery.c Core\Src\line_sensor_sample.c Core\Src\line_fault_log.c Core\Src\drive_base.c Core\Src\line_turn_pulse.c Core\Src\line_turn_load.c tests\line_recovery\tick_stub.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_mode2_follow.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_mode2_follow.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\sign_line\test_entry_handoff.c Core\Src\sign_route.c Core\Src\simple_line_mode.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_entry_handoff.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_entry_handoff.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\sign_line\test_mpu_service.c Core\Src\mpu6050_yaw.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_mpu_service.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_mpu_service.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /Itests\gyro_turn\bus_stubs /ICore\Inc tests\gyro_turn\test_bus.c Core\Src\mpu6050_bus.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_bus.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_bus.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\sign_line\test_gyro_route.c Core\Src\sign_route.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_gyro_route.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_gyro_route.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\sign_line\test_mode4_gyro_tangent.c Core\Src\sign_route.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_mode4_gyro_tangent.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_mode4_gyro_tangent.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\sign_line\test_sign_trace.c Core\Src\sign_trace.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_sign_trace.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_sign_trace.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\sign_line\test_sign_horn.c Core\Src\sign_horn.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_sign_horn.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_sign_horn.exe
if not "%errorlevel%"=="0" exit /b 1

python tests\sign_line\check_integration.py
if not "%errorlevel%"=="0" exit /b 1
python tests\sign_line\test_mode_selection.py
if not "%errorlevel%"=="0" exit /b 1
python tests\sign_line\test_k210_runtime.py
exit /b %errorlevel%
