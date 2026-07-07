# Win32 주소 범위 Dry-Run 작업 지시

Win32 x86 실행 준비를 위해 실제 메모리 예약 전에 원본 DOS/4GW 이미지가 요구하는 주소 범위가 현재 프로세스에서 비어 있는지 검사한다.

## 작업 범위

* `Win32AddressRangeProbe` 결과 구조 추가
* `ProbeWin32RuntimeAddressRange` 함수 추가
* analyzer 출력에 Win32 주소 범위 dry-run 결과 추가
* `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` 갱신
* 작업 완료 후 작업 로그 작성

## 제외 범위

* `VirtualAlloc`으로 실제 메모리 예약
* 실행 권한 설정
* 원본 entry 호출
* HLE dispatcher 구현

## 검증 절차

1. 기존 Debug 빌드를 수행한다.
2. `scripts\build_win32_x86.bat`를 실행한다.
3. `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`를 실행한다.
4. 출력에 `Win32 allocation dry run`과 `Win32 target range available`이 포함되는지 확인한다.

## Work Order

Probe whether the original DOS/4GW image address range is free in the current process before actual Win32 x86 memory reservation.

## Scope

* Add the `Win32AddressRangeProbe` result structure.
* Add the `ProbeWin32RuntimeAddressRange` function.
* Add Win32 address range dry-run output to the analyzer.
* Update `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, and `docs/EXE_DESIGN.en.md`.
* Write a work log after completion.

## Out of Scope

* Reserving memory with `VirtualAlloc`.
* Setting executable permissions.
* Calling the original entry point.
* Implementing the HLE dispatcher.

## Verification Procedure

1. Run the existing Debug build.
2. Run `scripts\build_win32_x86.bat`.
3. Run `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`.
4. Confirm that the output includes `Win32 allocation dry run` and `Win32 target range available`.
