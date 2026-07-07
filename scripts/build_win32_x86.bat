@echo off
setlocal

cmake -S . -B build\vs2022_win32_debug -G "Visual Studio 17 2022" -A Win32
if errorlevel 1 exit /b 1

cmake --build build\vs2022_win32_debug --config Debug
if errorlevel 1 exit /b 1

endlocal
