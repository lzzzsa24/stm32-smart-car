@echo off
cd /d "%~dp0\..\.."
python tests\mode5_bypass\test_app_profile.py
if errorlevel 1 exit /b 1
call tests\gyro_turn\run.cmd
exit /b %errorlevel%
