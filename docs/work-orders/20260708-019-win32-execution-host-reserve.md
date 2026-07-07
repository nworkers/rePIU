# Win32 Execution Host 초기 예약 작업 지시

별도 Win32 x86 execution host를 추가하고, target profile에 저장된 예약 힌트를 사용해 원본 DOS/4GW 이미지 주소 범위를 시작 직후 예약해 본다.

## 작업 범위

* `TargetRuntimeReservationHint` 추가
* `piu_1st` target profile에 초기 예약 범위 기록
* fixed range 기반 Win32 runtime memory policy 생성 API 추가
* `VirtualAlloc(MEM_RESERVE)` 예약 시도 API 추가
* `repiu_win32_execution_host` target 추가
* 실행 host 출력과 검증 절차 추가
* `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` 갱신
* 작업 완료 후 작업 로그 작성

## 제외 범위

* 원본 executable image 복사
* page commit/protection 구현
* 원본 entry 호출
* HLE dispatcher 연결

## 검증 절차

1. `scripts\build_win32_x86.bat`를 실행한다.
2. `build\vs2022_win32_debug\Debug\repiu_win32_execution_host.exe`를 실행한다.
3. reservation attempt 출력이 포함되는지 확인한다.
4. `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`를 실행해 기존 analyzer 동작을 확인한다.

## Work Order

Add a dedicated Win32 x86 execution host and use the reservation hint stored in the target profile to reserve the original DOS/4GW image address range immediately after startup.

## Scope

* Add `TargetRuntimeReservationHint`.
* Record the initial reserve range in the `piu_1st` target profile.
* Add a fixed-range Win32 runtime memory policy creation API.
* Add a `VirtualAlloc(MEM_RESERVE)` reservation attempt API.
* Add the `repiu_win32_execution_host` target.
* Add execution host output and verification procedure.
* Update `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, and `docs/EXE_DESIGN.en.md`.
* Write a work log after completion.

## Out of Scope

* Copying the original executable image.
* Implementing page commit/protection.
* Calling the original entry point.
* Connecting the HLE dispatcher.

## Verification Procedure

1. Run `scripts\build_win32_x86.bat`.
2. Run `build\vs2022_win32_debug\Debug\repiu_win32_execution_host.exe`.
3. Confirm that reservation attempt output is included.
4. Run `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe` to confirm existing analyzer behavior.
