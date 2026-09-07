@echo off
setlocal
cd /d "%~dp0\..\.."
if not exist "manual-build-vision-line-v4-host-test" mkdir "manual-build-vision-line-v4-host-test"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\vision_line_v4\test_vision_line_v4.c Core\Src\vision_line_v4_parser.c Core\Src\vision_line_v4_control.c /Fomanual-build-vision-line-v4-host-test\ /Femanual-build-vision-line-v4-host-test\test_vision_line_v4.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-vision-line-v4-host-test\test_vision_line_v4.exe
if not "%errorlevel%"=="0" exit /b 1
python tests\vision_line_v4\check_integration.py
exit /b %errorlevel%
