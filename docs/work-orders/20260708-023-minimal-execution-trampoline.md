# Minimal Execution Trampoline 작업 지시

Relocated image placement 위에서 원본 entry를 별도 thread로 최소 호출하고, 예외/복귀/timeout을 관찰한다.

## 작업 범위

* Win32 minimal execution trampoline API 추가
* 별도 thread 기반 relocated entry 호출 구현
* SEH 예외 포착 구현
* timeout 처리 구현
* execution host 출력 추가
* `docs/TODO.md` 추가 또는 갱신
* 관련 설계 문서 갱신
* 작업 완료 후 작업 로그 작성

## 제외 범위

* guest stack 전환
* HLE dispatcher 구현
* INT/DPMI trap 처리
* 정상 게임 실행

## 검증 절차

1. `scripts\build_win32_x86.bat`를 실행한다.
2. `build\vs2022_win32_debug\Debug\repiu_win32_execution_host.exe`를 실행한다.
3. minimal execution attempt 결과가 출력되는지 확인한다.
4. `cmake --build build\vs2022_debug --config Debug`를 실행한다.

## Work Order

Call the original relocated entry minimally from a separate thread and observe exception, return, or timeout.

## Scope

* Add Win32 minimal execution trampoline API.
* Implement separate-thread relocated entry call.
* Implement SEH exception capture.
* Implement timeout handling.
* Add execution host output.
* Add or update `docs/TODO.md`.
* Update related design documents.
* Write a work log after completion.

## Out of Scope

* Guest stack switching.
* HLE dispatcher implementation.
* INT/DPMI trap handling.
* Normal game execution.

## Verification Procedure

1. Run `scripts\build_win32_x86.bat`.
2. Run `build\vs2022_win32_debug\Debug\repiu_win32_execution_host.exe`.
3. Confirm minimal execution attempt output.
4. Run `cmake --build build\vs2022_debug --config Debug`.
