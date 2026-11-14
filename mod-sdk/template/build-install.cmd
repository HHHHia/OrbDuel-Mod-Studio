@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-install.ps1" %*
set "build_result=%errorlevel%"
if not "%ORBDUEL_NO_PAUSE%"=="1" pause
exit /b %build_result%
