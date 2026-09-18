# Task 706 작업 로그 — Linux x64 telemetry source 플랫폼 분리

## 수행 결과

`linux_x64_native_write_trace.cpp`를 공용 `repiu_exe` source 목록에서 제거하고 기존
Linux x64 전용 CMake 조건에 편성했습니다. Win32 x86 target은 더 이상 Linux SysV x64
dispatch-frame header를 컴파일하지 않으며, Linux x64 target은 기존 telemetry 구현과 ABI
검증을 그대로 유지합니다.

## 검증

- `cmd /c scripts\build_win32_x86.bat`: 종료 코드 0
- Win32 x86 Debug 전체 target 링크 성공
  - `repiu.exe`
  - `repiu_aot_probe.exe`
  - `repiu_core_probe.exe`
  - `repiu_supervisor_win32.exe`
  - 나머지 분석 및 probe 도구
- Win32 x86 `repiu_core_probe`: 적용 가능한 25/25 성공, Linux x64 전용 4개 group skip
- Linux x64 Debug `repiu`, `repiu_core_probe` 빌드 성공
- Linux x64 `repiu_core_probe`: 27/27 성공

MSVC의 기존 C4819 및 macro 재정의 경고와 WSL 파일시스템 clock-skew 경고는 있었지만
오류나 probe 실패는 없었습니다.

---

## English

### Result

`linux_x64_native_write_trace.cpp` was removed from the common `repiu_exe`
source list and assigned to the existing Linux x64 CMake condition. Win32 x86
no longer compiles the Linux SysV x64 dispatch-frame header, while Linux x64
retains the existing telemetry implementation and ABI checks.

### Verification

- `cmd /c scripts\build_win32_x86.bat`: exit code 0
- Every Win32 x86 Debug target linked, including `repiu.exe`,
  `repiu_aot_probe.exe`, `repiu_core_probe.exe`,
  `repiu_supervisor_win32.exe`, and the remaining analysis/probe tools.
- Win32 x86 `repiu_core_probe`: all 25 applicable groups passed; four Linux
  x64-only groups were skipped as intended.
- Linux x64 Debug `repiu` and `repiu_core_probe` built successfully.
- Linux x64 `repiu_core_probe`: all 27 groups passed.

Existing MSVC C4819 and macro-redefinition warnings and WSL filesystem
clock-skew warnings remained, but there were no errors or probe failures.
