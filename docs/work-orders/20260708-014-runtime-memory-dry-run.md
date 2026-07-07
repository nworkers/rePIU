# Runtime Memory Dry-Run 작업 지시

`Dos4gwLoadResult`를 입력으로 받아 실행 전 runtime memory 배치 계획을 계산한다.

## 작업 범위

* `RuntimeMemoryPlan` 구조 추가
* object region, entry linear address, stack top, HLE reserve base 계산
* analyzer 출력 추가
* `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` 갱신

## 제외 범위

* 실제 실행 메모리 할당
* 원본 코드 실행
* selector/GDT 구현
* HLE dispatcher 호출

## 검증 절차

1. Debug 빌드를 수행한다.
2. `repiu_exe_analyzer`를 인자 없이 실행한다.
3. runtime memory dry-run 출력과 기존 relocation 결과를 확인한다.

## Work Order

Calculate a pre-execution runtime memory layout plan from `Dos4gwLoadResult`.

## Scope

* Add the `RuntimeMemoryPlan` structure.
* Calculate object regions, entry linear address, stack top, and HLE reserve base.
* Add analyzer output.
* Update `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, and `docs/EXE_DESIGN.en.md`.

## Out of Scope

* Actual executable memory allocation.
* Executing original code.
* Selector/GDT implementation.
* Calling the HLE dispatcher.

## Verification Procedure

1. Build the Debug target.
2. Run `repiu_exe_analyzer` without arguments.
3. Confirm runtime memory dry-run output and existing relocation results.
