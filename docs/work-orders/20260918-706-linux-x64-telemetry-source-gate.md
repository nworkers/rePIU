# Task 706 작업 지시 — Linux x64 telemetry source 플랫폼 분리

## 목표

Linux x64 native-write telemetry source를 해당 플랫폼 target에만 편성하여 Win32 x86
전체 빌드를 복구하고 Linux x64 기능을 유지합니다.

## 작업 범위

1. 공용 `repiu_exe` source 목록에서 Linux x64 전용 telemetry source를 제거합니다.
2. 기존 Linux x64 CMake 조건에 source를 추가합니다.
3. Win32 x86 Debug 전체 빌드와 Linux x64 Debug 빌드/core probe를 검증합니다.
4. 아키텍처, 누적 분석, 작업 로그를 갱신합니다.

## 제외 범위

- Linux x64 dispatch-frame layout 또는 static assertion 변경
- native-write trace 동작 변경
- 다른 플랫폼 전용 source의 전면 재분류

---

## English

### Objective

Compile the Linux x64 native-write telemetry source only for its owning platform,
restoring the complete Win32 x86 build while retaining Linux x64 behavior.

### Scope

Remove the source from the common `repiu_exe` list, add it to the existing Linux
x64 CMake condition, verify complete Win32 x86 Debug and Linux x64 Debug/core
probe builds, and update architecture, cumulative analysis, and the work log.

### Out of scope

Changing the Linux x64 dispatch-frame layout or assertions, changing native-write
trace behavior, or comprehensively reclassifying every platform-specific source.
