# LE Fixup Record 디코더 설계

## 배경

이전 단계에서 fixup page table과 fixup record table 범위를 검증했다.

다음 단계는 fixup record table을 순회하면서 record 경계를 식별하고, `PIU.EXE`에서 관찰되는 내부 참조 형태를 1차 디코딩하는 것이다.

## 목표

이번 단계는 relocation 적용 전 분석 단계로 다음을 구현한다.

* page별 fixup record span 순회
* 가변 길이 fixup record 경계 계산
* 단일 source offset record 디코딩
* 내부 target object와 target offset 디코딩
* source type, target flags, target object, target offset 통계 출력
* unsupported record 위치와 수를 분석 결과로 보고

## 비목표

* relocation 적용
* source list record 전체 지원
* imported name/ordinal target 지원
* additive fixup 적용
* 원본 코드 실행

## 1차 해석 규칙

record 시작은 다음 공통 구조로 해석한다.

* `source_type`: 1바이트
* `target_flags`: 1바이트
* `source_offset`: 16-bit little-endian

`target_flags`의 하위 target 종류가 내부 참조로 해석되는 record만 1차 지원한다.

현재 `PIU.EXE`에서 관찰되는 내부 참조는 object 번호가 1바이트이고, target offset은 `target_flags`의 32-bit offset flag 여부에 따라 16-bit 또는 32-bit로 해석한다.

지원하지 않는 flag 조합은 record 길이를 안전하게 알 수 없으므로, unsupported count와 첫 unsupported 위치를 기록한 뒤 현재 1차 디코딩은 실패로 처리한다.

## 검증 기준

`PIU.EXE` 분석 출력에서 다음을 확인한다.

* fixup record 디코딩 결과가 valid이다.
* decoded record 수가 0보다 크다.
* consumed byte 수가 fixup record table 크기와 일치한다.
* unsupported record 수를 출력한다.

## 다음 단계

다음 단계에서는 unsupported record 샘플을 바탕으로 source list, additive, imported target, object number 크기 변형을 확장하고, 내부 relocation 적용으로 넘어간다.

## Background

The previous step validated the fixup page table and fixup record table range.

The next step is to walk the fixup record table, identify record boundaries, and decode the internal reference forms observed in `PIU.EXE`.

## Goal

This step implements pre-relocation analysis:

* walking per-page fixup record spans
* calculating variable-length fixup record boundaries
* decoding single source offset records
* decoding internal target object and target offset
* printing statistics for source type, target flags, target object, and target offset
* reporting unsupported record location and count as analysis results

## Non-Goals

* applying relocations
* full source-list record support
* imported name/ordinal target support
* applying additive fixups
* executing original code

## Initial Interpretation Rules

Each record starts with this common structure:

* `source_type`: 1 byte
* `target_flags`: 1 byte
* `source_offset`: 16-bit little-endian

Only records whose low target kind is interpreted as an internal reference are supported in the first pass.

The internal references observed in `PIU.EXE` use a 1-byte object number, and the target offset is interpreted as 16-bit or 32-bit depending on the 32-bit offset flag in `target_flags`.

Unsupported flag combinations cannot be safely skipped because the record length is unknown, so the first-pass decoder records the unsupported count and first unsupported location, then reports decoding failure.

## Verification Criteria

The `PIU.EXE` analysis output must show:

* fixup record decoding is valid
* decoded record count is greater than 0
* consumed bytes match the fixup record table size
* unsupported record count is printed

## Next Step

The next step expands source-list, additive, imported target, and object-number size variants from unsupported samples, then proceeds to applying internal relocations.
