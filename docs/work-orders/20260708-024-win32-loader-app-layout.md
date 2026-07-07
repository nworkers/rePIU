# Win32 로더 앱 구조 정리 작업 지시

## 목표

실제 Win32 로더 진입점을 `src/host/win32/main.cpp`로 이동하고 executable target 이름을 `repiu_loader_win32`로 변경한다.

## 작업 항목

1. `src/tools/win32_execution_host/main.cpp`를 `src/host/win32/main.cpp`로 이동한다.
2. CMake executable target을 `repiu_loader_win32`로 변경한다.
3. Win32 loader host image-base 정책 함수 이름을 현재 역할에 맞게 갱신한다.
4. 현재 구조 문서와 executable 설계 문서를 갱신한다.
5. Win32 x86 빌드와 x64 Debug 빌드를 검증한다.

## 제외 범위

이번 작업에서는 loader 실행 흐름, relocation 처리, trampoline 동작을 변경하지 않는다.

## 검증 절차

1. `cmd /c scripts\build_win32_x86.bat`
2. `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe`
3. `cmake --build build\vs2022_debug --config Debug`
4. `build\vs2022_debug\Debug\repiu_loader_win32.exe`

# Win32 Loader App Layout Work Order

## Goal

Move the practical Win32 loader entry point to `src/host/win32/main.cpp` and rename the executable target to `repiu_loader_win32`.

## Tasks

1. Move `src/tools/win32_execution_host/main.cpp` to `src/host/win32/main.cpp`.
2. Rename the CMake executable target to `repiu_loader_win32`.
3. Rename the Win32 loader host image-base policy helper to match the current role.
4. Update the current architecture document and executable design notes.
5. Verify the Win32 x86 build and the x64 Debug build.

## Out of Scope

This task does not change loader flow, relocation handling, or trampoline behavior.

## Verification Procedure

1. `cmd /c scripts\build_win32_x86.bat`
2. `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe`
3. `cmake --build build\vs2022_debug --config Debug`
4. `build\vs2022_debug\Debug\repiu_loader_win32.exe`
