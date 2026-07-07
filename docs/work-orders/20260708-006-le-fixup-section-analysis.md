# 작업 지시: LE Fixup Section 분석

## 목표

`PIU.EXE`의 LE fixup page table과 fixup record table 범위를 분석하고, relocation 적용 전 검증 정보를 출력한다.

## 범위

* fixup page table 파서 추가
* page별 fixup span 계산
* fixup record table 크기 계산
* fixup page table 단조 증가 검증
* 분석 도구 출력 확장
* `ARCHITECTURE.md`, `EXE_DESIGN.ko.md`, `EXE_DESIGN.en.md` 갱신
* 작업 로그 작성

## 비범위

* fixup record 상세 디코딩
* relocation 적용
* 원본 코드 실행

## 검증 절차

1. CMake configure를 수행한다.
2. Debug 빌드를 수행한다.
3. `repiu_exe_analyzer`를 `MASTER\PIU_1ST\PIU.EXE`에 실행한다.
4. 출력에서 `LE fixup page table: valid`, `LE fixup page table entries: 393`, `LE fixup page table monotonic: true`를 확인한다.

## Work Order: LE Fixup Section Analysis

## Goal

Analyze the LE fixup page table and fixup record table range in `PIU.EXE`, then print validation information before applying relocations.

## Scope

* Add a fixup page table parser.
* Calculate per-page fixup spans.
* Calculate fixup record table size.
* Validate fixup page table monotonicity.
* Extend analysis tool output.
* Update `ARCHITECTURE.md`, `EXE_DESIGN.ko.md`, and `EXE_DESIGN.en.md`.
* Write the work log.

## Non-Scope

* Detailed fixup record decoding.
* Applying relocations.
* Executing original code.

## Verification Procedure

1. Run CMake configure.
2. Build Debug.
3. Run `repiu_exe_analyzer` against `MASTER\PIU_1ST\PIU.EXE`.
4. Confirm `LE fixup page table: valid`, `LE fixup page table entries: 393`, and `LE fixup page table monotonic: true` in the output.
