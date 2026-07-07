# 작업 지시: LE 이미지 매핑 Dry-Run

## 목표

`PIU.EXE`의 LE 오브젝트 테이블과 페이지 테이블을 파싱하고, 원본 코드를 실행하지 않는 메모리 이미지 dry-run을 구현한다.

## 범위

* LE 오브젝트 레코드 구조 추가
* LE 페이지 레코드 구조 추가
* 오브젝트/페이지 테이블 파서 추가
* 오브젝트별 메모리 버퍼 생성과 페이지 복사 구현
* 분석 도구 출력 확장
* `ARCHITECTURE.md`, `EXE_DESIGN.ko.md`, `EXE_DESIGN.en.md` 갱신
* 작업 로그 작성

## 비범위

* relocation 적용
* fixup record 상세 분석
* 원본 코드 실행
* DPMI/DOS HLE 구현

## 검증 절차

1. CMake configure를 수행한다.
2. Debug 빌드를 수행한다.
3. `repiu_exe_analyzer`를 `MASTER\PIU_1ST\PIU.EXE`에 실행한다.
4. 출력에서 `LE image map: valid`, `LE mapped objects: 4`, `LE entry mapping: valid`를 확인한다.

## Work Order: LE Image Mapping Dry-Run

## Goal

Parse the LE object table and page table in `PIU.EXE`, then implement a memory image dry-run without executing original code.

## Scope

* Add LE object record structures.
* Add LE page record structures.
* Add object/page table parsers.
* Create per-object memory buffers and copy pages into them.
* Extend analysis tool output.
* Update `ARCHITECTURE.md`, `EXE_DESIGN.ko.md`, and `EXE_DESIGN.en.md`.
* Write the work log.

## Non-Scope

* Applying relocations.
* Detailed fixup record analysis.
* Executing original code.
* Implementing DPMI/DOS HLE.

## Verification Procedure

1. Run CMake configure.
2. Build Debug.
3. Run `repiu_exe_analyzer` against `MASTER\PIU_1ST\PIU.EXE`.
4. Confirm `LE image map: valid`, `LE mapped objects: 4`, and `LE entry mapping: valid` in the output.
