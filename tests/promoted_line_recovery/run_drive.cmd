@echo off
setlocal
cd /d "%~dp0\..\.."
if not exist manual-build-promoted-drive-host-test mkdir manual-build-promoted-drive-host-test
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /DTEST_REAL_DRIVE_BASE /DMPU6050_BYPASS_ENABLED=0 /W4 /WX /utf-8 /std:c11 /Itests\line_recovery\stubs /ICore\Inc tests\promoted_line_recovery\test_line_turn_load.c Core\Src\drive_base.c Core\Src\line_turn_pulse.c Core\Src\encoder_turn.c Core\Src\encoder_linear.c Core\Src\line_obstacle_bypass.c Core\Src\line_bypass_turn.c Core\Src\line_bypass_travel.c Core\Src\motion_advanced.c Core\Src\line_wait_guard.c Core\Src\line_turn_load.c Core\Src\promoted_line_tracking.c Core\Src\line_sensor_sample.c Core\Src\line_sensor_clock.c tests\promoted_line_recovery\tick_stub.c Core\Src\promoted_line_recovery.c Core\Src\line_fault_log.c Core\Src\buzzer_phrase_40077493715.c /Fomanual-build-promoted-drive-host-test\ /Femanual-build-promoted-drive-host-test\test_line_turn_load.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-promoted-drive-host-test\test_line_turn_load.exe
exit /b %errorlevel%
