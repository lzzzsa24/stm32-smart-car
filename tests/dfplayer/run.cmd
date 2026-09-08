@echo off
setlocal
cd /d "%~dp0\..\.."
if not exist "manual-build-dfplayer-host-test" mkdir "manual-build-dfplayer-host-test"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /W4 /WX /utf-8 /std:c11 /ICore\Inc tests\dfplayer\test_dfplayer_protocol.c Core\Src\dfplayer_protocol.c /Fomanual-build-dfplayer-host-test\ /Femanual-build-dfplayer-host-test\test_dfplayer_protocol.exe
if not "%errorlevel%"=="0" exit /b 1
manual-build-dfplayer-host-test\test_dfplayer_protocol.exe
exit /b %errorlevel%
