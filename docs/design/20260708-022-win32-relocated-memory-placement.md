# Win32 Relocated Memory Placement 설계

## 배경

Relocated image buffer 단계에서 원본 LE image를 `0x01000000` 기준 C++ buffer로 구체화하고 relocation 값을 실제로 기록했다.

다음 단계는 이 buffer를 Win32 x86 process memory에 배치하는 것이다.

주의할 점은 기존 Win32 host image base 정책도 `0x01000000`을 사용했다는 점이다. relocated image를 같은 주소에 배치하려면 host executable은 더 높은 주소로 이동해야 한다.

## 목표

* Win32 x86 host image base를 `0x10000000`으로 이동한다.
* relocated image base `0x01000000`은 원본 image runtime 배치 주소로 유지한다.
* `RelocatedRuntimeImage`를 Win32 process memory에 `VirtualAlloc`으로 예약/커밋한다.
* object buffer 내용을 process memory에 복사한다.
* object flags를 기준으로 최소 page protection을 적용한다.
* execution host에서 배치 결과를 출력한다.

## 비목표

* 원본 entry 호출
* guest stack 전환
* HLE dispatcher 연결
* skipped relocation 해석 완료

## 설계

Win32 platform 모듈에 `Win32RelocatedImagePlacement`와 `PlaceWin32RelocatedImage`를 추가한다.

placement 함수는 relocated image 전체 범위를 계산하고, `VirtualAlloc(base, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)`로 먼저 읽기/쓰기 가능한 메모리를 확보한다.

이후 object별 buffer를 `relocated_base_address`에 해당하는 process memory 위치로 복사한다.

복사가 끝나면 object flags의 writable/executable bit를 기준으로 `VirtualProtect`를 호출한다.

현재 LE object flag는 다음 최소 정책으로 해석한다.

* `0x00000002`: writable
* `0x00000004`: executable
* executable + writable: `PAGE_EXECUTE_READWRITE`
* executable only: `PAGE_EXECUTE_READ`
* writable only: `PAGE_READWRITE`
* otherwise: `PAGE_READONLY`

## 검증 기준

* Win32 x86 빌드가 성공한다.
* `repiu_win32_execution_host.exe`가 relocated image placement 결과를 출력한다.
* placement base가 `0x01000000`으로 출력된다.
* copied object count가 4로 출력된다.
* applied protection count가 4로 출력된다.
* 기존 analyzer 빌드가 유지된다.

## 향후 확장

다음 단계에서는 relocated memory placement 위에서 guest stack/trampoline 설계를 시작한다. entry 호출 전에는 skipped relocation 10개와 selector/far pointer 계열 의미를 더 확인해야 한다.

## Background

The relocated image buffer step materialized the original LE image as C++ buffers at relocated base `0x01000000` and wrote relocated relocation values.

The next step is to place those buffers into Win32 x86 process memory.

One important issue is that the previous Win32 host image base policy also used `0x01000000`. To place the relocated image there, the host executable must move to a higher address.

## Goal

* Move the Win32 x86 host image base to `0x10000000`.
* Keep relocated image base `0x01000000` as the original image runtime placement address.
* Reserve and commit process memory for `RelocatedRuntimeImage` with `VirtualAlloc`.
* Copy object buffers into process memory.
* Apply minimal page protection based on object flags.
* Print placement results from the execution host.

## Non-Goals

* Calling the original entry point.
* Switching to the guest stack.
* Connecting the HLE dispatcher.
* Fully interpreting skipped relocations.

## Design

Add `Win32RelocatedImagePlacement` and `PlaceWin32RelocatedImage` to the Win32 platform module.

The placement function calculates the full relocated image range and reserves/commits it with `VirtualAlloc(base, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)`.

Then it copies each object buffer to its `relocated_base_address` location in process memory.

After copying, it calls `VirtualProtect` per object based on writable/executable bits in the object flags.

The current minimal LE object flag policy is:

* `0x00000002`: writable
* `0x00000004`: executable
* executable + writable: `PAGE_EXECUTE_READWRITE`
* executable only: `PAGE_EXECUTE_READ`
* writable only: `PAGE_READWRITE`
* otherwise: `PAGE_READONLY`

## Verification Criteria

* The Win32 x86 build succeeds.
* `repiu_win32_execution_host.exe` prints relocated image placement results.
* Placement base is printed as `0x01000000`.
* Copied object count is printed as 4.
* Applied protection count is printed as 4.
* Existing analyzer build remains working.

## Future Extension

The next step will design the guest stack/trampoline on top of relocated memory placement. Before calling entry, the 10 skipped relocations and selector/far pointer meanings should be investigated further.

## 추가 결정: Relocated Base 후보 선택

실제 Win32 x86 execution host에서 `0x01000000` 예약이 실패할 수 있음이 확인되었다.

따라서 `0x01000000`은 첫 후보로 유지하되, execution host는 `0x02000000`, `0x03000000` 등 더 높은 후보를 순서대로 probe하고, 비어 있는 첫 base를 선택한다.

선택된 base를 기준으로 relocatable runtime image plan과 relocated image buffer를 다시 생성한 뒤 Win32 process memory에 배치한다.

## Additional Decision: Relocated Base Candidate Selection

The actual Win32 x86 execution host may fail to reserve `0x01000000`.

Therefore, `0x01000000` remains the first candidate, but the execution host probes higher candidates such as `0x02000000` and `0x03000000` in order and selects the first free base.

The relocatable runtime image plan and relocated image buffer are rebuilt for the selected base before Win32 process memory placement.
