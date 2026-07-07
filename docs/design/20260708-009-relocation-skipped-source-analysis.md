# Relocation Skipped Source 분석 설계

## 배경

내부 relocation dry-run은 `PIU.EXE`에서 14637개 relocation을 적용하고 10개 record를 skipped로 남겼다.

skipped record를 무리해서 적용하면 원본 실행 이미지가 잘못될 수 있으므로, 먼저 skipped source 형태를 더 자세히 출력해야 한다.

## 목표

이번 단계는 적용 로직을 확장하지 않고 관찰성을 강화한다.

* source kind별 record 수 출력
* 첫 unsupported source kind record 출력
* 첫 source out-of-range record 출력
* skipped record의 page, source type, source offset, target object, target offset 출력

## 비목표

* skipped relocation 적용
* source kind `0x03` 의미 확정
* `source_offset=0xFFFE` 의미 확정
* 원본 코드 실행

## 검증 기준

분석 도구 출력에서 다음을 확인한다.

* `LE relocation source kind counts:`가 출력된다.
* `LE first unsupported source relocation:`이 출력된다.
* `LE first out-of-range relocation:`이 출력된다.
* 기존 relocation dry-run 결과는 유지된다.

## 다음 단계

다음 단계에서는 출력된 sample을 기준으로 source kind `0x03`과 out-of-range source offset의 의미를 별도 설계하고, 적용 가능하면 skipped count를 줄인다.

## Background

The internal relocation dry-run applied 14637 relocations in `PIU.EXE` and left 10 records skipped.

Applying skipped records prematurely can corrupt the original executable image, so the skipped source forms must be printed in more detail first.

## Goal

This step improves observability without expanding relocation application logic.

* Print record counts by source kind.
* Print the first unsupported source kind record.
* Print the first source out-of-range record.
* Print skipped record page, source type, source offset, target object, and target offset.

## Non-Goals

* Applying skipped relocations.
* Finalizing source kind `0x03` semantics.
* Finalizing `source_offset=0xFFFE` semantics.
* Executing original code.

## Verification Criteria

The analysis tool output must include:

* `LE relocation source kind counts:`
* `LE first unsupported source relocation:`
* `LE first out-of-range relocation:`
* unchanged existing relocation dry-run results

## Next Step

The next step uses the printed samples to design source kind `0x03` and out-of-range source offset handling, then reduces the skipped count if safe.
