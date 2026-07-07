# Relocated Image Buffer 설계

## 배경

이전 단계에서 원본 LE object 배치를 `0x01000000` 기준으로 옮기는 relocatable runtime image dry-run이 성립함을 확인했다.

다음 단계는 실제 실행 가능한 OS 메모리로 가기 전에, C++ owned buffer 안에 relocated image를 구성하고 relocation 값을 새 base 기준으로 써 넣는 것이다.

## 목표

* `RelocatedRuntimeImage` 구조를 추가한다.
* relocatable runtime image plan에 따라 object별 memory buffer를 복사한다.
* source kind `0x07` relocation에 대해 relocated target value를 실제 buffer에 32-bit little-endian으로 기록한다.
* 적용/스킵/실패 통계를 출력한다.
* 기존 low fixed-address allocation 경로와 분리한다.

## 비목표

* `VirtualAlloc`으로 executable memory 할당
* page protection 설정
* 원본 entry 호출
* skipped relocation 의미 해석 완료

## 설계

runtime 모듈에 `RelocatedRuntimeImage`와 `BuildRelocatedRuntimeImage`를 추가한다.

`BuildRelocatedRuntimeImage`는 `Dos4gwLoadResult`와 `RelocatableRuntimeImagePlan`을 입력으로 받는다.

각 object buffer는 기존 mapped object memory를 그대로 복사한다. 그 뒤 fixup record를 다시 순회하면서 source kind `0x07`에 대해 source object offset 위치에 relocated target address를 기록한다.

source kind가 `0x07`이 아니거나 source range가 buffer 밖이면 기존 dry-run과 동일하게 skipped로 집계한다. target object가 없거나 source page owner를 찾을 수 없는 경우는 실패로 처리한다.

## 검증 기준

* analyzer가 relocated image buffer 결과를 출력한다.
* relocated image buffer의 applied/skipped/failed count가 relocatable dry-run과 일치한다.
* 첫 relocation sample에서 `previous` 값은 기존 original relocation 값이고, `applied` 값은 relocated 값으로 출력된다.

## 향후 확장

다음 단계에서는 relocated image buffer를 Win32 x86 process memory에 배치하고, object flags를 기준으로 page protection을 설계한다.

## Background

The previous step verified that the original LE object layout can be moved to a relocated runtime image base at `0x01000000`.

The next step is to materialize that plan into C++ owned buffers and write relocated relocation values before moving to actual executable OS memory.

## Goal

* Add a `RelocatedRuntimeImage` structure.
* Copy each object memory buffer according to the relocatable runtime image plan.
* Write relocated target values for source kind `0x07` as 32-bit little-endian values.
* Print applied/skipped/failed statistics.
* Keep this path separate from low fixed-address allocation.

## Non-Goals

* Allocating executable memory with `VirtualAlloc`.
* Setting page protection.
* Calling the original entry point.
* Fully interpreting skipped relocations.

## Design

Add `RelocatedRuntimeImage` and `BuildRelocatedRuntimeImage` to the runtime module.

`BuildRelocatedRuntimeImage` receives `Dos4gwLoadResult` and `RelocatableRuntimeImagePlan`.

Each object buffer is copied from the existing mapped object memory. Then the function walks fixup records again and, for source kind `0x07`, writes the relocated target address into the source object buffer.

Non-`0x07` source kinds and out-of-range source locations are counted as skipped, matching the existing dry-run. Missing target objects or missing source page owners are treated as failures.

## Verification Criteria

* The analyzer prints relocated image buffer results.
* Relocated image buffer applied/skipped/failed counts match the relocatable dry-run.
* The first relocation sample prints the existing original relocation value as `previous` and the relocated value as `applied`.

## Future Extension

The next step will place the relocated image buffer into Win32 x86 process memory and design page protection based on object flags.
