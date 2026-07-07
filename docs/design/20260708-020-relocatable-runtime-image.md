# Relocatable Runtime Image Dry-Run 설계

## 배경

Win32 x86 낮은 주소 고정 예약은 `0x00010000` 블록이 이미 `MEM_COMMIT` 상태라 안정적인 기본 경로로 보기 어렵다.

따라서 원본 DOS/4GW LE object를 안전한 새 runtime base에 배치하고, 원본 LE fixup/relocation 정보를 새 주소 기준으로 다시 적용하는 경로를 우선 설계한다.

## 목표

* 원본 object base와 새 object base의 매핑을 계산한다.
* 기본 새 runtime base는 `0x01000000`으로 둔다.
* object 간 상대 간격은 원본 LE object base 간격을 유지한다.
* relocated entry address와 relocated stack top을 계산한다.
* fixup record를 기준으로 새 relocation 적용 값을 dry-run으로 계산한다.
* 기존 skipped relocation 10개의 분포를 유지해서 위험을 계속 추적한다.

## 비목표

* 실제 executable memory allocation
* relocated image buffer에 실제 write
* page commit/protection 설정
* 원본 entry 호출

## 설계

`runtime` 모듈에 `RelocatableRuntimeImagePlan`을 추가한다.

계획 생성 함수는 `Dos4gwLoadResult`와 새 preferred runtime base를 입력으로 받는다.

원본 object 중 가장 낮은 `relocation_base_address`를 original image base로 보고, `delta = relocated_image_base - original_image_base`를 계산한다.

각 object의 새 base는 `original_object_base + delta`로 계산한다. 이렇게 하면 object 사이의 간격과 원본 내부 offset 구조는 유지하면서 전체 image만 위로 이동할 수 있다.

entry와 stack은 기존 object index와 offset을 사용해 새 object base 기준으로 계산한다.

relocation dry-run은 기존 `LeFixupRecordInfo`를 순회한다. source kind `0x07`은 32-bit internal pointer write로 보고, target object의 새 base와 target offset을 더한 값을 relocated applied value로 계산한다. source kind가 다르거나 source 위치가 범위를 벗어나는 record는 기존 dry-run과 같이 skipped로 집계한다.

## 검증 기준

* analyzer가 relocatable runtime image dry-run 결과를 출력한다.
* relocated image base가 `0x01000000`으로 출력된다.
* relocated entry와 stack top이 새 base 기준으로 출력된다.
* relocated relocation applied/skipped count가 기존 relocation dry-run과 같은 수준으로 출력된다.

## 향후 확장

다음 단계에서는 dry-run 계획을 바탕으로 실제 host memory에 relocated image buffer를 만들고, source kind `0x03`, `0x05`, `0x13` 등 skipped relocation 의미를 더 세분화한다.

## Background

Fixed low-address reservation in Win32 x86 is not reliable because the block at `0x00010000` is already `MEM_COMMIT`.

The project will prioritize placing the original DOS/4GW LE objects at a safe new runtime base and reapplying the original LE fixup/relocation information for that new address layout.

## Goal

* Calculate the mapping from original object bases to relocated object bases.
* Use `0x01000000` as the default new runtime base.
* Preserve relative spacing between original LE object bases.
* Calculate relocated entry address and relocated stack top.
* Dry-run relocated relocation values from fixup records.
* Keep tracking the existing 10 skipped relocations as risk items.

## Non-Goals

* Actual executable memory allocation.
* Writing to a relocated image buffer.
* Page commit/protection setup.
* Calling the original entry point.

## Design

Add `RelocatableRuntimeImagePlan` to the `runtime` module.

The builder receives `Dos4gwLoadResult` and a preferred relocated runtime base.

The lowest original object `relocation_base_address` is treated as the original image base. The relocation delta is `relocated_image_base - original_image_base`.

Each relocated object base is calculated as `original_object_base + delta`. This preserves object spacing and original internal offsets while moving the entire image upward.

Entry and stack addresses are calculated from the same object index and offset, but using relocated object bases.

The relocation dry-run walks `LeFixupRecordInfo`. Source kind `0x07` is treated as a 32-bit internal pointer write, and the relocated applied value is `relocated_target_object_base + target_offset`. Other source kinds or out-of-range source locations are counted as skipped, matching the existing dry-run behavior.

## Verification Criteria

* The analyzer prints relocatable runtime image dry-run results.
* The relocated image base is printed as `0x01000000`.
* Relocated entry and stack top are printed using the new base.
* Relocated relocation applied/skipped counts are printed at the same level as the existing relocation dry-run.

## Future Extension

The next step will use this dry-run plan to create an actual relocated image buffer in host memory and further classify skipped relocation source kinds such as `0x03`, `0x05`, and `0x13`.
