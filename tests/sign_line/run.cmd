@echo off
setlocal
cd /d "%~dp0\..\.."
if not exist "manual-build-sign-line-host-test" mkdir "manual-build-sign-line-host-test"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\sign_line\test_sign_line.c Core\Src\vision_detection_parser.c Core\Src\simple_line_mode.c Core\Src\sign_route.c Core\Src\sign_slowdown.c /Fomanual-build-sign-line-host-test\ /Femanual-build-sign-line-host-test\test_sign_line.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-sign-line-host-test\test_sign_line.exe
if not "%errorlevel%"=="0" exit /b 1
python tests\sign_line\check_integration.py
exit /b %errorlevel%
