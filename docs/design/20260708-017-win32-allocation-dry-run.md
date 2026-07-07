# Win32 주소 범위 Dry-Run 설계

## 배경

이전 단계에서 Win32 x86 빌드가 가능하고, 32-bit host process에서는 원본 32-bit x86 entry로 직접 제어를 넘길 수 있다는 정책 판단을 확인했다.

다음 단계에서 실제 실행 가능한 메모리를 예약하기 전에, 현재 Win32 프로세스의 주소 공간에서 DOS/4GW LE 이미지가 요구하는 고정 주소 범위가 비어 있는지 확인해야 한다.

이번 단계는 실제 `VirtualAlloc` 호출로 메모리를 예약하지 않는다. `VirtualQuery`로 주소 범위 상태만 확인하고 분석기 출력에 보고한다.

## 목표

* Win32 runtime memory policy가 요구하는 `[preferred_allocation_base, hle_reserve_base)` 범위를 검사한다.
* 검사 결과를 `Win32AddressRangeProbe` 구조로 보고한다.
* 분석기에서 주소 범위 dry-run 결과를 출력한다.
* 이후 실제 executable memory allocation 단계의 판단 근거를 남긴다.

## 비목표

* 실행 가능한 메모리 예약
* 원본 entry 호출
* HLE dispatcher 연결
* 주소 재배치 fallback 구현

## 설계

`src/platform/win32/runtime_memory_policy.cpp`에 Win32 전용 주소 범위 probe 함수를 추가한다.

검사 함수는 `Win32RuntimeMemoryPolicy`를 입력으로 받고, 정책이 유효하지 않으면 실패로 보고한다.

정책이 유효하면 `preferred_allocation_base`부터 `required_reserve_size`만큼의 주소 범위를 `VirtualQuery`로 순회한다. 각 region의 상태가 `MEM_FREE`이면 계속 진행하고, `MEM_RESERVE` 또는 `MEM_COMMIT` 등 비어 있지 않은 region을 만나면 첫 blocking block 정보를 기록하고 `range_available=false`로 보고한다.

이 dry-run은 x64 analyzer에서도 호출 가능하지만, 직접 원본 x86 실행 가능 여부는 기존 `direct_x86_execution_supported` 정책 값을 그대로 따른다. 실제 실행 준비 판단은 Win32 x86 빌드 출력에서 우선 확인한다.

## 검증 기준

* 기존 x64 Debug 빌드가 성공한다.
* Win32 x86 Debug 빌드가 성공한다.
* Win32 x86 analyzer 출력에 주소 범위 dry-run 결과가 포함된다.
* 주소 범위가 비어 있지 않더라도 analyzer는 실패하지 않고 blocking block 정보를 출력한다.

## 향후 확장

다음 단계에서는 dry-run 결과를 바탕으로 실제 `VirtualAlloc` 예약 정책을 설계한다. 목표 주소 범위가 이미 점유되어 있다면 고정 주소 실행을 유지할지, 별도 32-bit helper process 초기화 순서를 조정할지, 또는 relocation 기반 fallback을 둘지 판단해야 한다.

## Background

The previous step verified that a Win32 x86 build is available and that a 32-bit host process can directly transfer control to the original 32-bit x86 entry point.

Before reserving executable memory, the loader needs to check whether the fixed address range required by the DOS/4GW LE image is free in the current Win32 process.

This step does not reserve memory with `VirtualAlloc`. It only queries address range state with `VirtualQuery` and reports the result through the analyzer.

## Goal

* Probe the `[preferred_allocation_base, hle_reserve_base)` range required by the Win32 runtime memory policy.
* Report the result through a `Win32AddressRangeProbe` structure.
* Print the address range dry-run result in the analyzer.
* Leave a decision point for a later executable memory allocation step.

## Non-Goals

* Reserving executable memory.
* Calling the original entry point.
* Connecting the HLE dispatcher.
* Implementing relocation fallback.

## Design

Add a Win32-specific address range probe function to `src/platform/win32/runtime_memory_policy.cpp`.

The probe function receives `Win32RuntimeMemoryPolicy`. If the policy is invalid, the probe reports failure.

If the policy is valid, the function walks the range starting at `preferred_allocation_base` for `required_reserve_size` bytes with `VirtualQuery`. It continues while each region is `MEM_FREE`. If it sees a non-free region such as `MEM_RESERVE` or `MEM_COMMIT`, it records the first blocking block and reports `range_available=false`.

The dry-run can also be called from an x64 analyzer, but direct original x86 execution still follows the existing `direct_x86_execution_supported` policy. Execution readiness should be judged primarily from the Win32 x86 build output.

## Verification Criteria

* The existing x64 Debug build succeeds.
* The Win32 x86 Debug build succeeds.
* The Win32 x86 analyzer output includes the address range dry-run result.
* Even if the range is not free, the analyzer does not fail and prints the blocking block information.

## Future Extension

The next step will design actual `VirtualAlloc` reservation policy based on this dry-run result. If the target range is occupied, the project must decide whether to preserve fixed-address execution, adjust 32-bit helper process initialization order, or add a relocation-based fallback.
