# 작업 지시: 내부 Relocation Dry-Run

## 목표

디코딩된 내부 fixup record를 사용해 LE 이미지 버퍼에 relocation 값을 적용하는 dry-run을 구현한다.

## 범위

* relocation dry-run 결과 구조 추가
* source page를 소유한 오브젝트 탐색 추가
* source write 위치 계산 추가
* target virtual address 계산 추가
* source kind `0x07` 32-bit write 적용
* 그 외 source kind는 skipped로 집계
* 현재 버퍼 모델에 직접 쓸 수 없는 source offset은 source out-of-range skipped로 집계
* 분석 도구 출력 확장
* `ARCHITECTURE.md`, `EXE_DESIGN.ko.md`, `EXE_DESIGN.en.md` 갱신
* 작업 로그 작성

## 비범위

* 실행 가능한 Win32 메모리 생성
* 원본 코드 실행
* external/import relocation 지원
* selector/descriptor 구현

## 검증 절차

1. CMake configure를 수행한다.
2. Debug 빌드를 수행한다.
3. `repiu_exe_analyzer`를 `MASTER\PIU_1ST\PIU.EXE`에 실행한다.
4. 출력에서 `LE relocation dry run: valid`, `LE failed relocations: 0`을 확인한다.
5. applied relocation 수와 skipped relocation 수의 합이 decoded fixup record 수와 일치하는지 확인한다.

## Work Order: Internal Relocation Dry-Run

## Goal

Use decoded internal fixup records to apply relocation values to the LE image buffers as a dry-run.

## Scope

* Add relocation dry-run result structures.
* Add lookup from source page to owning object.
* Add source write location calculation.
* Add target virtual address calculation.
* Apply source kind `0x07` as a 32-bit write.
* Count other source kinds as skipped.
* Count source offsets that cannot be directly written into the current buffer model as source out-of-range skipped.
* Extend analysis tool output.
* Update `ARCHITECTURE.md`, `EXE_DESIGN.ko.md`, and `EXE_DESIGN.en.md`.
* Write the work log.

## Non-Scope

* Creating executable Win32 memory.
* Executing original code.
* Supporting external/import relocations.
* Implementing selectors/descriptors.

## Verification Procedure

1. Run CMake configure.
2. Build Debug.
3. Run `repiu_exe_analyzer` against `MASTER\PIU_1ST\PIU.EXE`.
4. Confirm `LE relocation dry run: valid` and `LE failed relocations: 0` in the output.
5. Confirm applied relocation count plus skipped relocation count matches decoded fixup record count.
