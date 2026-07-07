# 작업 지시: PIU.EXE 비실행 분석 도구

## 목표

`MASTER\PIU_1ST\PIU.EXE`를 실행하지 않고 읽어서 MZ/LE 헤더 주요 정보를 출력하는 C++ 콘솔 도구를 구현한다.

## 범위

* CMake 기반 최소 C++20 프로젝트 구조 추가
* `include/repiu/exe/`, `src/exe/`, `src/tools/exe_analyzer/` 추가
* MZ 헤더 파서 추가
* LE 헤더 고정 필드 파서 추가
* `repiu_exe_analyzer` 콘솔 도구 추가
* `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` 갱신
* 작업 로그 작성

## 비범위

* 원본 코드 실행
* LE 오브젝트 페이지 실제 메모리 매핑
* fixup record 상세 파싱과 relocation 적용
* DOS/DPMI HLE 구현

## 검증 절차

1. CMake configure를 수행한다.
2. Debug 빌드를 수행한다.
3. `repiu_exe_analyzer`를 `MASTER\PIU_1ST\PIU.EXE`에 실행한다.
4. 출력에서 `MZ: valid`, `LE offset: 0x00002C90`, `LE signature: valid`를 확인한다.

## Work Order: PIU.EXE Non-Executing Analysis Tool

## Goal

Implement a C++ console tool that reads `MASTER\PIU_1ST\PIU.EXE` without executing it and prints major MZ/LE header information.

## Scope

* Add a minimal CMake-based C++20 project structure.
* Add `include/repiu/exe/`, `src/exe/`, and `src/tools/exe_analyzer/`.
* Add an MZ header parser.
* Add a fixed-field LE header parser.
* Add the `repiu_exe_analyzer` console tool.
* Update `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, and `docs/EXE_DESIGN.en.md`.
* Write the work log.

## Non-Scope

* Executing original code.
* Mapping LE object pages into runtime memory.
* Detailed fixup record parsing and relocation application.
* DOS/DPMI HLE implementation.

## Verification Procedure

1. Run CMake configure.
2. Build Debug.
3. Run `repiu_exe_analyzer` against `MASTER\PIU_1ST\PIU.EXE`.
4. Confirm that the output includes `MZ: valid`, `LE offset: 0x00002C90`, and `LE signature: valid`.
