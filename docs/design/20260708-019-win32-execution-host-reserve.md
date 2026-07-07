# Win32 Execution Host 초기 예약 설계

## 배경

Win32 x86 analyzer에서 host image base를 `0x01000000`으로 이동해도 원본 DOS/4GW 이미지 예약 범위 `[0x00010000, 0x005E7000)`는 완전히 비어 있지 않았다.

analyzer는 실행 파일 분석, 파일 읽기, C++ 컨테이너 할당을 수행한 뒤 주소 범위를 검사하므로, 실제 실행 host보다 주소 공간 점유가 많을 수 있다.

이번 단계에서는 dedicated Win32 x86 execution host target을 추가하고, target profile에 저장된 예약 힌트를 사용해 가능한 이른 시점에 목표 범위를 `VirtualAlloc(MEM_RESERVE)`로 선점해 본다.

## 목표

* `TargetProfile`에 초기 Win32 runtime reservation hint를 추가한다.
* `piu_1st` profile에 `base=0x00010000`, `size=0x005D7000`을 기록한다.
* Win32 runtime memory policy에 fixed range 기반 policy 생성과 reservation attempt API를 추가한다.
* `repiu_win32_execution_host` executable target을 추가한다.
* execution host 시작 직후 목표 주소 범위 예약을 시도하고 결과를 출력한다.

## 비목표

* 원본 executable image 복사
* page별 commit/protection 설정
* 원본 entry 호출
* HLE dispatcher 연결
* CRT 이전 custom entry point 구현

## 설계

`TargetRuntimeReservationHint`를 `TargetProfile`에 추가한다. 이 값은 이전 분석 단계에서 확인한 runtime object range와 HLE reserve base를 target별로 캐시한 것이다.

Win32 전용 정책 모듈에는 `BuildWin32RuntimeMemoryPolicyFromFixedRange`를 추가한다. 실행 host는 실행 파일을 읽기 전에 이 힌트만으로 reserve policy를 만들 수 있다.

`ReserveWin32RuntimeAddressRange`는 `VirtualAlloc(base, size, MEM_RESERVE, PAGE_READWRITE)`를 호출한다. 성공하면 reservation 정보를 반환하고, 실패하면 Windows error code와 메시지를 반환한다. 실패는 프로세스 실패로 취급하지 않고 관찰 결과로 남긴다.

새 executable target `repiu_win32_execution_host`는 현재 단계에서 주소 예약만 수행한다. 이후 이 target에 loader image copy, commit/protection, HLE dispatcher, entry transfer를 단계적으로 붙인다.

## 검증 기준

* Win32 x86 빌드가 성공한다.
* `repiu_win32_execution_host.exe`가 실행된다.
* 출력에 fixed reserve range와 reservation attempt 결과가 포함된다.
* 기존 analyzer 빌드와 실행이 유지된다.

## 향후 확장

초기 예약이 성공하면 다음 단계에서 reserved range 안에 LE object page를 commit하고 relocation 적용 이미지 데이터를 복사한다.

초기 예약이 실패하면 더 이른 custom entry point, reservation-only bootstrap process, 또는 relocation fallback 여부를 설계해야 한다.

## Background

Even after moving the Win32 x86 analyzer host image base to `0x01000000`, the original DOS/4GW image reservation range `[0x00010000, 0x005E7000)` was not fully free.

The analyzer probes the range after executable analysis, file reads, and C++ container allocations, so it may occupy more address space than a real execution host.

This step adds a dedicated Win32 x86 execution host target and uses a reservation hint stored in the target profile to try `VirtualAlloc(MEM_RESERVE)` as early as practical.

## Goal

* Add an initial Win32 runtime reservation hint to `TargetProfile`.
* Record `base=0x00010000` and `size=0x005D7000` for the `piu_1st` profile.
* Add fixed-range policy creation and reservation attempt APIs to the Win32 runtime memory policy module.
* Add a `repiu_win32_execution_host` executable target.
* Try reserving the target address range immediately after execution host startup and print the result.

## Non-Goals

* Copying the original executable image.
* Per-page commit or protection setup.
* Calling the original entry point.
* Connecting the HLE dispatcher.
* Implementing a custom pre-CRT entry point.

## Design

Add `TargetRuntimeReservationHint` to `TargetProfile`. This caches the target-specific runtime object range and HLE reserve base discovered in previous analysis stages.

The Win32 policy module gains `BuildWin32RuntimeMemoryPolicyFromFixedRange`. The execution host can build a reserve policy from the hint before reading the executable.

`ReserveWin32RuntimeAddressRange` calls `VirtualAlloc(base, size, MEM_RESERVE, PAGE_READWRITE)`. On success, it reports reservation details. On failure, it reports the Windows error code and message. Failure is treated as an observation rather than a process failure.

The new `repiu_win32_execution_host` target only performs address reservation in this step. Later steps will add loader image copy, commit/protection, HLE dispatch, and entry transfer to this target.

## Verification Criteria

* The Win32 x86 build succeeds.
* `repiu_win32_execution_host.exe` runs.
* Output includes the fixed reserve range and reservation attempt result.
* Existing analyzer build and execution remain working.

## Future Extension

If the early reservation succeeds, the next step will commit LE object pages inside the reserved range and copy the relocated image data.

If the early reservation fails, the project should design an earlier custom entry point, a reservation-only bootstrap process, or a relocation fallback.

## 설계 결정: Relocation 기반 로드 우선

Win32 x86 execution host에서도 `0x00010000` 시작 블록이 이미 `MEM_COMMIT` 상태로 확인되었으므로, 원래 주소에 그대로 할당하는 경로는 기본 실행 전략으로 삼지 않는다.

프로젝트는 원본 DOS/4GW LE 이미지의 relocation 정보를 활용해 안전한 새 runtime base에 올리는 방향을 우선 설계한다.

고정 주소 로드는 관찰과 비교 검증용 fallback으로 남기되, 다음 작업부터는 relocatable runtime image를 주 경로로 다룬다.

이 결정은 원본 게임 로직을 C++로 재작성하지 않는다는 원칙과 충돌하지 않는다. 원본 코드는 계속 실행 대상이며, 변경되는 것은 loader가 원본 relocation 정보를 적용해 메모리 배치 주소를 바꾸는 방식이다.

## Design Decision: Prefer Relocation-Based Loading

Because the Win32 x86 execution host still observes a `MEM_COMMIT` block at `0x00010000`, fixed-address allocation should not be the default execution strategy.

The project will prioritize loading the original DOS/4GW LE image at a safe new runtime base using its relocation information.

Fixed-address loading remains useful as an observation and comparison fallback, but the next work should treat a relocatable runtime image as the primary path.

This decision does not conflict with the principle of preserving original game logic. The original code remains the execution target; the loader only changes the memory placement by applying the original relocation metadata.
