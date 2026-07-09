@echo off
setlocal

powershell -ExecutionPolicy Bypass -File "%~dp0build_win32_x86.ps1"
if errorlevel 1 exit /b 1

endlocal
