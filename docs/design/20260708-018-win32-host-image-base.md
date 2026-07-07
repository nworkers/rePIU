# Win32 Host Image Base 정책 설계

## 배경

Win32 x86 analyzer의 주소 범위 dry-run 결과, 원본 DOS/4GW 이미지가 요구하는 `[0x00010000, 0x005E7000)` 범위가 현재 프로세스에서 완전히 비어 있지 않았다.

원본 보호 모드 코드를 고정 주소로 실행하려면, host executable과 host runtime이 가능한 한 이 범위를 피해야 한다.

이번 단계에서는 dedicated execution host를 만들기 전 임시 기준으로 `repiu_exe_analyzer`의 Win32 x86 image base를 DOS/4GW 이미지 범위 밖으로 이동한다.

## 목표

* Win32 x86 빌드에서 host executable image base를 `0x01000000`으로 지정한다.
* ASLR이 image base를 다시 낮은 주소로 옮기지 않도록 MSVC 링크 옵션에서 `/DYNAMICBASE:NO`를 적용한다.
* analyzer 출력에 host image base 정책 값을 표시한다.
* 주소 범위 dry-run 결과 변화를 관찰한다.

## 비목표

* 실제 executable memory reservation
* dedicated execution host 분리
* CRT/heap/DLL 로드 순서 제어
* 원본 entry 호출

## 설계

CMake에 `repiu_configure_win32_execution_host` 함수를 추가한다.

이 함수는 MSVC, Win32, 32-bit 빌드에서만 대상 executable에 다음 링크 옵션을 적용한다.

* `/BASE:0x01000000`
* `/DYNAMICBASE:NO`

`0x01000000`은 현재 `PIU.EXE` runtime HLE reserve base인 `0x005E7000`보다 충분히 위에 있으므로 원본 이미지 예약 범위와 직접 충돌하지 않는다.

현재는 dedicated host가 없기 때문에 `repiu_exe_analyzer`에만 적용한다. 이후 실행 전용 host target이 생기면 같은 함수를 그 target에 적용한다.

## 검증 기준

* Win32 x86 빌드가 성공한다.
* analyzer 출력에 `Win32 host image base policy: 0x01000000`이 포함된다.
* analyzer 주소 범위 dry-run 결과를 작업 로그에 기록한다.

## 향후 확장

host image base 이동만으로 주소 범위가 완전히 비지 않는다면, 다음 단계에서는 더 이른 시점에 `VirtualAlloc`으로 목표 범위를 선점하는 dedicated Win32 x86 execution host를 설계한다.

## Background

The Win32 x86 analyzer address range dry-run showed that the original DOS/4GW image range `[0x00010000, 0x005E7000)` is not fully free in the current process.

To execute the original protected-mode code at fixed addresses, the host executable and host runtime should avoid this range as much as possible.

Before introducing a dedicated execution host, this step moves the Win32 x86 image base of `repiu_exe_analyzer` outside the DOS/4GW image range as a temporary baseline.

## Goal

* Set the host executable image base to `0x01000000` for Win32 x86 builds.
* Apply `/DYNAMICBASE:NO` in MSVC link options so ASLR does not move the image back into low memory.
* Print the host image base policy in analyzer output.
* Observe how the address range dry-run result changes.

## Non-Goals

* Actual executable memory reservation.
* Splitting a dedicated execution host.
* Controlling CRT, heap, or DLL load order.
* Calling the original entry point.

## Design

Add a `repiu_configure_win32_execution_host` function to CMake.

The function applies these link options only for MSVC Win32 32-bit executable targets:

* `/BASE:0x01000000`
* `/DYNAMICBASE:NO`

`0x01000000` is well above the current `PIU.EXE` runtime HLE reserve base `0x005E7000`, so it does not directly collide with the original image reservation range.

Because there is no dedicated host yet, this is applied only to `repiu_exe_analyzer`. When an execution-only host target is added later, the same function should be applied to that target.

## Verification Criteria

* The Win32 x86 build succeeds.
* Analyzer output includes `Win32 host image base policy: 0x01000000`.
* The analyzer address range dry-run result is recorded in the work log.

## Future Extension

If moving the host image base alone does not make the target range fully free, the next step should design a dedicated Win32 x86 execution host that reserves the target range with `VirtualAlloc` as early as possible.
