@echo off
setlocal
cd /d "%~dp0\..\.."
if not exist "manual-build-gyro-host-test" mkdir "manual-build-gyro-host-test"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /Itests\line_recovery\stubs /ICore\Inc /Ireusable\gyro_angle\examples tests\gyro_turn\test_gyro_turn.c reusable\gyro_angle\examples\angle_mode_example.c Core\Src\mpu6050_yaw.c Core\Src\gyro_turn.c Core\Src\line_bypass_turn.c /Fomanual-build-gyro-host-test\ /Femanual-build-gyro-host-test\test_gyro_turn.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-gyro-host-test\test_gyro_turn.exe
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /Itests\gyro_turn\bus_stubs /ICore\Inc tests\gyro_turn\test_bus.c Core\Src\mpu6050_bus.c /Fomanual-build-gyro-host-test\ /Femanual-build-gyro-host-test\test_bus.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-gyro-host-test\test_bus.exe
if not "%errorlevel%"=="0" exit /b 1
python tests\gyro_turn\check_integration.py
exit /b %errorlevel%
