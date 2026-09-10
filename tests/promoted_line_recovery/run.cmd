@echo off
setlocal
cd /d "%~dp0\..\.."
if not exist manual-build-promoted-line-host-test mkdir manual-build-promoted-line-host-test
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if not "%errorlevel%"=="0" exit /b 1
for %%s in (120000 90000) do (
cl /nologo /W4 /WX /utf-8 /std:c11 /DLINE_SEARCH_NOMINAL_YAW_MDEG_S=%%s /Itests\line_recovery\stubs /ICore\Inc tests\promoted_line_recovery\test_line_recovery.c Core\Src\promoted_line_tracking.c Core\Src\line_fault_log.c Core\Src\line_sensor_sample.c Core\Src\line_sensor_clock.c tests\promoted_line_recovery\tick_stub.c Core\Src\promoted_line_recovery.c Core\Src\buzzer_phrase_40077493715.c /Fomanual-build-promoted-line-host-test\ /Femanual-build-promoted-line-host-test\test.exe
if errorlevel 1 exit /b 1
manual-build-promoted-line-host-test\test.exe
if errorlevel 1 exit /b 1
)
exit /b 0
