# 작업 지시: LE Fixup Record 디코더

## 목표

LE fixup record table을 순회해 record 경계를 계산하고, `PIU.EXE`에서 관찰되는 내부 참조 형태를 1차 디코딩한다.

## 범위

* fixup record 구조 추가
* page별 span 기반 record 순회 추가
* 내부 target object/offset 디코딩 추가
* unsupported record 집계 추가
* 분석 도구 출력 확장
* `ARCHITECTURE.md`, `EXE_DESIGN.ko.md`, `EXE_DESIGN.en.md` 갱신
* 작업 로그 작성

## 비범위

* relocation 적용
* 모든 LE fixup variant 지원
* 원본 코드 실행

## 검증 절차

1. CMake configure를 수행한다.
2. Debug 빌드를 수행한다.
3. `repiu_exe_analyzer`를 `MASTER\PIU_1ST\PIU.EXE`에 실행한다.
4. 출력에서 `LE fixup records: valid`, `LE decoded fixup records:`를 확인한다.
5. consumed bytes가 record table size와 일치하는지 확인한다.

## Work Order: LE Fixup Record Decoder

## Goal

Walk the LE fixup record table, calculate record boundaries, and decode the internal reference forms observed in `PIU.EXE`.

## Scope

* Add fixup record structures.
* Add record walking based on per-page spans.
* Add internal target object/offset decoding.
* Add unsupported record accounting.
* Extend analysis tool output.
* Update `ARCHITECTURE.md`, `EXE_DESIGN.ko.md`, and `EXE_DESIGN.en.md`.
* Write the work log.

## Non-Scope

* Applying relocations.
* Supporting every LE fixup variant.
* Executing original code.

## Verification Procedure

1. Run CMake configure.
2. Build Debug.
3. Run `repiu_exe_analyzer` against `MASTER\PIU_1ST\PIU.EXE`.
4. Confirm `LE fixup records: valid` and `LE decoded fixup records:` in the output.
5. Confirm consumed bytes match the record table size.
