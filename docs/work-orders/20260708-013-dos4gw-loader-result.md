# DOS/4GW Loader Result 작업 지시

기존 analyzer의 MZ/LE/image/fixup/relocation 순차 호출을 공용 DOS/4GW loader result 함수로 묶는다.

## 작업 범위

* `Dos4gwLoadResult` 구조 추가
* `LoadDos4gwExecutable` 함수 추가
* analyzer가 loader result를 사용하도록 변경
* 기존 analyzer 출력 유지
* `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` 갱신

## 제외 범위

* 원본 코드 실행
* 실행 메모리 할당
* HLE 구현
* relocation 해석 확장

## 검증 절차

1. Debug 빌드를 수행한다.
2. `repiu_exe_analyzer`를 인자 없이 실행한다.
3. 기존 핵심 분석 수치가 유지되는지 확인한다.

## Work Order

Group the analyzer's existing MZ/LE/image/fixup/relocation sequence into a shared DOS/4GW loader result function.

## Scope

* Add the `Dos4gwLoadResult` structure.
* Add the `LoadDos4gwExecutable` function.
* Change the analyzer to use the loader result.
* Preserve existing analyzer output.
* Update `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, and `docs/EXE_DESIGN.en.md`.

## Out of Scope

* Executing original code.
* Allocating executable memory.
* HLE implementation.
* Expanding relocation interpretation.

## Verification Procedure

1. Build the Debug target.
2. Run `repiu_exe_analyzer` without arguments.
3. Confirm the existing key analysis numbers are preserved.
