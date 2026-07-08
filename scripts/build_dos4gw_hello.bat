@echo off
setlocal

set "ROOT=%~dp0.."
set "WATCOM=%ROOT%\tools\openwatcom"
set "PATH=%WATCOM%\binnt;%PATH%"
set "INCLUDE=%WATCOM%\h;%WATCOM%\h\nt"
set "LIB=%WATCOM%\lib386;%WATCOM%\lib386\dos"

if not exist "%WATCOM%\binnt\wcl386.exe" (
    echo OpenWatcom was not found at %WATCOM%
    echo Run: powershell -ExecutionPolicy Bypass -File scripts\install_openwatcom.ps1
    exit /b 1
)

set "SAMPLE_DIR=%ROOT%\samples\dos4gw_hello"
set "BUILD_DIR=%SAMPLE_DIR%\build"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

pushd "%BUILD_DIR%"
wcl386 -q -bt=dos -l=dos4g -fe=hello.exe ..\hello.c
if errorlevel 1 (
    popd
    exit /b 1
)
popd

endlocal
