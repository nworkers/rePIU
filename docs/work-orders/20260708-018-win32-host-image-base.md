# Win32 Host Image Base 정책 작업 지시

Win32 x86 프로세스에서 원본 DOS/4GW 이미지의 고정 주소 범위를 피하도록 host executable image base 링크 정책을 추가한다.

## 작업 범위

* CMake에 Win32 x86 execution host 링크 정책 함수 추가
* `repiu_exe_analyzer` Win32 x86 빌드에 `/BASE:0x01000000`, `/DYNAMICBASE:NO` 적용
* analyzer 출력에 host image base policy 표시
* `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` 갱신
* 작업 완료 후 작업 로그 작성

## 제외 범위

* 실제 `VirtualAlloc` 예약
* dedicated execution host 분리
* 원본 entry 호출
* HLE dispatcher 연결

## 검증 절차

1. `scripts\build_win32_x86.bat`를 실행한다.
2. `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`를 실행한다.
3. `Win32 host image base policy: 0x01000000` 출력이 포함되는지 확인한다.
4. 주소 범위 dry-run 결과를 작업 로그에 기록한다.

## Work Order

Add a host executable image base link policy so the Win32 x86 process avoids the fixed address range required by the original DOS/4GW image.

## Scope

* Add a Win32 x86 execution host link policy function to CMake.
* Apply `/BASE:0x01000000` and `/DYNAMICBASE:NO` to the Win32 x86 `repiu_exe_analyzer` build.
* Print the host image base policy in analyzer output.
* Update `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, and `docs/EXE_DESIGN.en.md`.
* Write a work log after completion.

## Out of Scope

* Actual `VirtualAlloc` reservation.
* Splitting a dedicated execution host.
* Calling the original entry point.
* Connecting the HLE dispatcher.

## Verification Procedure

1. Run `scripts\build_win32_x86.bat`.
2. Run `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`.
3. Confirm output includes `Win32 host image base policy: 0x01000000`.
4. Record the address range dry-run result in the work log.
