# LE Fixup Section 분석 설계

## 배경

LE 이미지 매핑 dry-run은 오브젝트와 페이지를 메모리 버퍼에 배치하는 데 성공했다.

하지만 아직 fixup record를 해석하거나 relocation을 적용하지 않았기 때문에, 엔트리 포인트 실행으로 넘어가기 전에 fixup section의 구조를 검증해야 한다.

## 목표

이번 단계는 fixup 적용 전 준비 단계로, 다음 정보를 분석한다.

* fixup page table 위치와 엔트리 수
* fixup record table 위치와 크기
* page별 fixup record span
* fixup이 존재하는 페이지 수
* 가장 큰 page fixup span
* fixup page table의 단조 증가 여부
* fixup record table 뒤에 남는 trailing byte 수

## 비목표

* fixup record 가변 길이 디코딩
* relocation 적용
* 내부/외부 참조별 relocation 처리
* 실행 가능한 메모리 이미지 확정
* 원본 코드 실행

## 해석 규칙

LE fixup page table은 `page_count + 1`개의 32-bit little-endian offset으로 해석한다.

각 offset은 fixup record table 시작 기준 상대 offset이다.

page N의 fixup record span은 `offset[N + 1] - offset[N]`이다.

fixup record table의 끝은 현재 `import_module_name_table_offset`으로 계산한다. 이 값은 `fixup_record_table_offset`보다 커야 한다.

## 검증 기준

`PIU.EXE`에서 다음 조건을 만족해야 한다.

* fixup page table 엔트리 수는 `393`이다.
* fixup page table은 단조 증가한다.
* fixup record table 크기는 양수이다.
* fixup record span이 있는 페이지가 하나 이상 존재한다.

## 다음 단계

이 분석이 안정화되면 다음 작업에서 fixup record 가변 길이 디코더를 추가하고, 내부 relocation부터 적용한다.

## Background

The LE image mapping dry-run successfully placed objects and pages into memory buffers.

However, fixup records are not decoded and relocations are not applied yet, so the fixup section structure must be validated before moving toward entry point execution.

## Goal

This step analyzes the following information as preparation before applying fixups:

* fixup page table location and entry count
* fixup record table location and size
* per-page fixup record spans
* number of pages with fixups
* largest page fixup span
* monotonicity of the fixup page table
* trailing bytes after the fixup record table spans

## Non-Goals

* variable-length fixup record decoding
* applying relocations
* internal/external relocation handling
* final executable memory image
* executing original code

## Interpretation Rules

The LE fixup page table is interpreted as `page_count + 1` 32-bit little-endian offsets.

Each offset is relative to the start of the fixup record table.

The fixup record span for page N is `offset[N + 1] - offset[N]`.

The end of the fixup record table is currently calculated from `import_module_name_table_offset`. That value must be greater than `fixup_record_table_offset`.

## Verification Criteria

For `PIU.EXE`, these conditions must hold:

* the fixup page table entry count is `393`
* the fixup page table is monotonic
* the fixup record table size is positive
* at least one page has a non-empty fixup record span

## Next Step

After this analysis is stable, the next task will add a variable-length fixup record decoder and apply internal relocations first.
