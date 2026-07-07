# Runtime Memory Dry-Run 설계

## 배경

`Dos4gwLoadResult`는 원본 DOS/4GW 실행 파일의 LE 이미지와 relocation dry-run 결과를 한 구조로 제공한다.

다음 실행 준비 단계는 이 결과를 실제 실행 가능한 메모리에 바로 배치하기 전에, 원본 코드가 기대하는 object base, entry point, stack top, HLE 예약 영역을 계산하는 것이다.

이번 단계는 실행 메모리를 할당하지 않는 dry-run이다.

## 목표

* `RuntimeMemoryPlan` 구조를 추가한다.
* LE object별 base/size/copy 정보를 runtime region으로 변환한다.
* entry linear address를 계산한다.
* stack top linear address를 계산한다.
* object 영역 뒤의 HLE 예약 시작 주소를 page align으로 계산한다.
* analyzer에 runtime memory dry-run 요약을 출력한다.

## 비목표

* `VirtualAlloc` 등 실제 실행 메모리 할당
* 32-bit 원본 코드 실행
* selector/GDT 구현
* HLE dispatcher 호출

## 설계

`RuntimeMemoryPlan`은 다음 정보를 가진다.

* 유효성
* object region 목록
* entry linear address
* stack top linear address
* HLE reserve base
* total object virtual bytes

object region은 LE object의 relocation base address와 virtual size를 그대로 사용한다.

entry address는 entry object의 base와 entry offset을 더해 계산한다.

stack top은 stack object의 base와 stack offset을 더해 계산한다. stack offset은 object size와 같을 수 있으므로 `<= object size`를 유효 범위로 본다.

HLE reserve base는 모든 object region의 끝 중 최댓값을 4KB 단위로 올림 정렬한다.

## 검증 기준

* Debug 빌드가 성공한다.
* analyzer 출력에 `Runtime memory dry run: valid`가 포함된다.
* entry linear address가 출력된다.
* stack top linear address가 출력된다.
* HLE reserve base가 출력된다.
* 기존 relocation dry-run 결과가 유지된다.

## 향후 확장

다음 단계에서 이 dry-run 계획을 기반으로 Win32/x86 실행 메모리 할당과 selector abstraction 설계를 진행한다.

## Background

`Dos4gwLoadResult` provides the original DOS/4GW executable's LE image and relocation dry-run result in one structure.

The next execution preparation step is to calculate the object bases, entry point, stack top, and HLE reserve area expected by the original code before placing anything into executable memory.

This step is a dry-run and does not allocate executable memory.

## Goal

* Add the `RuntimeMemoryPlan` structure.
* Convert each LE object into a runtime region with base/size/copy information.
* Calculate the entry linear address.
* Calculate the stack top linear address.
* Calculate a page-aligned HLE reserve base after the object ranges.
* Print the runtime memory dry-run summary in the analyzer.

## Non-Goals

* Actual executable memory allocation such as `VirtualAlloc`.
* Executing original 32-bit code.
* Selector/GDT implementation.
* Calling the HLE dispatcher.

## Design

`RuntimeMemoryPlan` contains:

* validity
* object region list
* entry linear address
* stack top linear address
* HLE reserve base
* total object virtual bytes

Each object region uses the LE object's relocation base address and virtual size directly.

The entry address is calculated as entry object base plus entry offset.

The stack top is calculated as stack object base plus stack offset. The stack offset may equal the object size, so `<= object size` is considered valid.

The HLE reserve base is the maximum object region end rounded up to a 4 KB boundary.

## Verification Criteria

* Debug build succeeds.
* Analyzer output includes `Runtime memory dry run: valid`.
* Entry linear address is printed.
* Stack top linear address is printed.
* HLE reserve base is printed.
* Existing relocation dry-run results are preserved.

## Future Extension

A later step will use this dry-run plan to design Win32/x86 executable memory allocation and selector abstraction.
