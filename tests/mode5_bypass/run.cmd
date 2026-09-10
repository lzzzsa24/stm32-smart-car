@echo off
setlocal
cd /d "%~dp0\..\.."
if not exist "manual-build-mode5-host-test" mkdir "manual-build-mode5-host-test"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /Itests\line_recovery\stubs /ICore\Inc tests\mode5_bypass\test_real_drive.c Core\Src\mpu6050_yaw.c Core\Src\gyro_turn.c Core\Src\line_bypass_turn.c Core\Src\line_obstacle_bypass.c Core\Src\line_bypass_travel.c Core\Src\drive_base.c tests\irq_restore_stub.c Core\Src\line_turn_pulse.c Core\Src\line_turn_load.c Core\Src\motion_advanced.c Core\Src\line_fault_log.c Core\Src\line_wait_guard.c /Fomanual-build-mode5-host-test\ /Femanual-build-mode5-host-test\test_real_drive.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-mode5-host-test\test_real_drive.exe
if not "%errorlevel%"=="0" exit /b 1
python tests\gyro_turn\check_integration.py
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\sign_line\test_mpu_service.c Core\Src\mpu6050_yaw.c /Fomanual-build-mode5-host-test\ /Femanual-build-mode5-host-test\test_mpu_service.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-mode5-host-test\test_mpu_service.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /Itests\line_recovery\stubs /ICore\Inc tests\mode5_bypass\test_ultrasonic_recovery.c Core\Src\ultrasonic_avoid.c Core\Src\ultrasonic_motion.c /Fomanual-build-mode5-host-test\ /Femanual-build-mode5-host-test\test_ultrasonic_recovery.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-mode5-host-test\test_ultrasonic_recovery.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /Itests\line_recovery\stubs /ICore\Inc tests\mode5_bypass\test_return_gate.c /Fomanual-build-mode5-host-test\ /Femanual-build-mode5-host-test\test_return_gate.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-mode5-host-test\test_return_gate.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /Itests\line_recovery\stubs /ICore\Inc tests\mode5_bypass\test_bypass_range.c Core\Src\line_bypass_range.c /Fomanual-build-mode5-host-test\ /Femanual-build-mode5-host-test\test_bypass_range.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-mode5-host-test\test_bypass_range.exe
if not "%errorlevel%"=="0" exit /b 1
python tests\mode5_bypass\test_app_profile.py
if not "%errorlevel%"=="0" exit /b 1
python tests\mode5_bypass\test_mode_selection.py
exit /b %errorlevel%
