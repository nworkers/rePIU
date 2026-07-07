# 작업 지시: Relocation Skipped Source 분석

## 목표

내부 relocation dry-run에서 skipped로 남은 record의 source 형태를 자세히 출력한다.

## 범위

* source kind별 count 추가
* 첫 unsupported source sample 추가
* 첫 source out-of-range sample 추가
* 분석 도구 출력 확장
* `ARCHITECTURE.md`, `EXE_DESIGN.ko.md`, `EXE_DESIGN.en.md` 갱신
* 작업 로그 작성

## 비범위

* skipped relocation 적용
* source kind 의미 확정
* 원본 코드 실행

## 검증 절차

1. CMake configure를 수행한다.
2. Debug 빌드를 수행한다.
3. `repiu_exe_analyzer`를 `MASTER\PIU_1ST\PIU.EXE`에 실행한다.
4. 출력에서 source kind count와 첫 skipped sample을 확인한다.

## Work Order: Relocation Skipped Source Analysis

## Goal

Print detailed source information for records left skipped by the internal relocation dry-run.

## Scope

* Add counts by source kind.
* Add the first unsupported source sample.
* Add the first source out-of-range sample.
* Extend analysis tool output.
* Update `ARCHITECTURE.md`, `EXE_DESIGN.ko.md`, and `EXE_DESIGN.en.md`.
* Write the work log.

## Non-Scope

* Applying skipped relocations.
* Finalizing source kind semantics.
* Executing original code.

## Verification Procedure

1. Run CMake configure.
2. Build Debug.
3. Run `repiu_exe_analyzer` against `MASTER\PIU_1ST\PIU.EXE`.
4. Confirm source kind counts and first skipped samples in the output.
