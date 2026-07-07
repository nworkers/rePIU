# Win32 로더 앱 구조 정리 작업 로그

## 변경 내용

실제 Win32 로더 진입점을 `src/tools/win32_execution_host/main.cpp`에서 `src/host/win32/main.cpp`로 이동했다.

CMake executable target 이름을 `repiu_win32_execution_host`에서 `repiu_loader_win32`로 변경했다.

Win32 host image-base 정책 함수 이름을 `repiu_configure_win32_loader_host`로 변경했다.

콘솔 출력의 일부 `execution host` 문구를 `loader`로 바꿔 현재 역할이 더 명확하게 보이도록 했다.

## 수정 파일

* `CMakeLists.txt`
* `src/host/win32/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-024-win32-loader-app-layout.md`
* `docs/work-orders/20260708-024-win32-loader-app-layout.md`

## 검증

* `cmd /c scripts\build_win32_x86.bat`: 성공
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe`: 성공
* `cmake -S . -B build\vs2022_debug -G "Visual Studio 17 2022" -A x64`: 성공
* `cmake --build build\vs2022_debug --config Debug`: 성공
* `build\vs2022_debug\Debug\repiu_loader_win32.exe`: 성공

Win32 x86 실행에서는 기존과 같이 relocated base `0x02000000`을 선택했고, minimal execution attempt는 `0x020F3890`에서 `0xC0000096` 예외를 관찰했다.

x64 실행에서는 minimal execution attempt가 unsupported로 안전하게 종료되었다.

# Win32 Loader App Layout Work Log

## Changes

Moved the practical Win32 loader entry point from `src/tools/win32_execution_host/main.cpp` to `src/host/win32/main.cpp`.

Renamed the CMake executable target from `repiu_win32_execution_host` to `repiu_loader_win32`.

Renamed the Win32 host image-base policy helper to `repiu_configure_win32_loader_host`.

Updated some console output from `execution host` wording to `loader` so the current role is clearer.

## Modified Files

* `CMakeLists.txt`
* `src/host/win32/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-024-win32-loader-app-layout.md`
* `docs/work-orders/20260708-024-win32-loader-app-layout.md`

## Verification

* `cmd /c scripts\build_win32_x86.bat`: passed
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe`: passed
* `cmake -S . -B build\vs2022_debug -G "Visual Studio 17 2022" -A x64`: passed
* `cmake --build build\vs2022_debug --config Debug`: passed
* `build\vs2022_debug\Debug\repiu_loader_win32.exe`: passed

The Win32 x86 run selected relocated base `0x02000000`, as before, and the minimal execution attempt observed exception `0xC0000096` at `0x020F3890`.

The x64 run safely ended the minimal execution attempt as unsupported.
