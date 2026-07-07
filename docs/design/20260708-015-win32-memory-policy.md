# Win32/x86 Runtime Memory Policy 설계

## 배경

runtime memory dry-run은 원본 LE object region, entry, stack top, HLE reserve base를 계산한다.

하지만 원본 32-bit x86 코드를 직접 실행하려면 host process도 32-bit여야 한다.

현재 개발/검증 빌드는 x64 Visual Studio 구성을 사용할 수 있으므로, 실행 가능 여부를 코드에서 명시적으로 보고해야 한다.

이번 단계는 실제 메모리 할당이 아니라 Win32/x86 실행 메모리 정책을 정리하고 analyzer에 출력하는 단계이다.

## 목표

* Win32 runtime memory policy report 구조를 추가한다.
* host pointer bit 수를 기록한다.
* direct 32-bit x86 execution 가능 여부를 기록한다.
* object region과 HLE reserve base를 기반으로 예약해야 할 주소 범위를 계산한다.
* analyzer에 Win32 runtime memory policy 요약을 출력한다.

## 비목표

* `VirtualAlloc` 호출
* 고정 주소 매핑 시도
* 원본 entry 호출
* selector/GDT 구현
* x64 helper process 또는 CPU emulation 구현

## 설계

Win32/x86 direct execution 정책은 다음 기준을 따른다.

* 32-bit host process에서만 원본 32-bit x86 entry로 직접 제어 이전할 수 있다.
* 64-bit host process에서는 직접 호출하지 않고, 향후 32-bit helper process나 별도 execution backend가 필요하다.
* 현재 단계에서는 이 판단을 analyzer에 출력하고 실패가 아니라 unsupported 상태로 보고한다.

메모리 정책 보고서는 다음 정보를 포함한다.

* host pointer bits
* direct x86 execution supported 여부
* preferred allocation base
* required reserve size
* HLE reserve base
* 설명 문자열

preferred allocation base는 runtime object region의 가장 낮은 base address이다.

required reserve size는 HLE reserve base에서 preferred allocation base를 뺀 값이다.

## 검증 기준

* Debug 빌드가 성공한다.
* analyzer 출력에 `Win32 runtime memory policy:`가 포함된다.
* host pointer bits가 출력된다.
* direct x86 execution 지원 여부가 출력된다.
* 기존 runtime memory dry-run과 relocation 결과가 유지된다.

## 향후 확장

다음 단계에서 32-bit Win32 빌드 구성을 추가하고, 같은 정책 구조를 이용해 실제 `VirtualAlloc` dry-run 또는 allocation을 구현한다.

## Background

The runtime memory dry-run calculates original LE object regions, entry, stack top, and HLE reserve base.

However, directly executing original 32-bit x86 code requires the host process itself to be 32-bit.

The current development and verification build may use a Visual Studio x64 configuration, so code should explicitly report whether direct execution is possible.

This step documents the Win32/x86 executable memory policy and prints it in the analyzer without allocating memory.

## Goal

* Add a Win32 runtime memory policy report structure.
* Record host pointer bit count.
* Record whether direct 32-bit x86 execution is supported.
* Calculate the address range that would need to be reserved from object regions and the HLE reserve base.
* Print a Win32 runtime memory policy summary in the analyzer.

## Non-Goals

* Calling `VirtualAlloc`.
* Trying fixed-address mapping.
* Calling the original entry point.
* Selector/GDT implementation.
* x64 helper process or CPU emulation implementation.

## Design

The Win32/x86 direct execution policy uses these rules:

* Direct control transfer to the original 32-bit x86 entry point is allowed only from a 32-bit host process.
* A 64-bit host process must not call the entry directly and will need a future 32-bit helper process or separate execution backend.
* The current step prints this judgment in the analyzer and reports unsupported status rather than failing.

The memory policy report contains:

* host pointer bits
* whether direct x86 execution is supported
* preferred allocation base
* required reserve size
* HLE reserve base
* explanatory message

The preferred allocation base is the lowest base address among runtime object regions.

The required reserve size is the HLE reserve base minus the preferred allocation base.

## Verification Criteria

* Debug build succeeds.
* Analyzer output includes `Win32 runtime memory policy:`.
* Host pointer bits are printed.
* Direct x86 execution support is printed.
* Existing runtime memory dry-run and relocation results are preserved.

## Future Extension

A later step will add a 32-bit Win32 build configuration and use this same policy structure for an actual `VirtualAlloc` dry-run or allocation.
