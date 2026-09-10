@echo off
setlocal
cd /d "%~dp0\..\.."
if not exist "manual-build-gyro-host-test" mkdir "manual-build-gyro-host-test"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if not "%errorlevel%"=="0" exit /b 1
call tests\gyro_turn\run_module.cmd
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /Itests\line_recovery\stubs /ICore\Inc tests\gyro_turn\test_real_drive.c Core\Src\mpu6050_yaw.c Core\Src\gyro_turn.c Core\Src\line_bypass_turn.c Core\Src\line_obstacle_bypass.c Core\Src\line_bypass_travel.c Core\Src\drive_base.c tests\irq_restore_stub.c Core\Src\line_turn_pulse.c Core\Src\line_turn_load.c Core\Src\motion_advanced.c Core\Src\line_fault_log.c Core\Src\line_wait_guard.c /Fomanual-build-gyro-host-test\ /Femanual-build-gyro-host-test\test_real_drive.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-gyro-host-test\test_real_drive.exe
if not "%errorlevel%"=="0" exit /b 1
python tests\gyro_turn\check_integration.py
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\sign_line\test_mpu_service.c Core\Src\mpu6050_yaw.c /Fomanual-build-gyro-host-test\ /Femanual-build-gyro-host-test\test_mpu_service.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-gyro-host-test\test_mpu_service.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /Itests\line_recovery\stubs /ICore\Inc tests\gyro_turn\test_ultrasonic_recovery.c Core\Src\ultrasonic_avoid.c Core\Src\ultrasonic_motion.c /Fomanual-build-gyro-host-test\ /Femanual-build-gyro-host-test\test_ultrasonic_recovery.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-gyro-host-test\test_ultrasonic_recovery.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /Itests\line_recovery\stubs /ICore\Inc tests\gyro_turn\test_return_gate.c /Fomanual-build-gyro-host-test\ /Femanual-build-gyro-host-test\test_return_gate.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-gyro-host-test\test_return_gate.exe
exit /b %errorlevel%
