# Relocation Source Type 상세 분석 작업 지시

내부 relocation dry-run에서 skipped source를 full source type 단위로 분리해 출력한다.

## 작업 범위

* `LeRelocationDryRun`에 source type count 추가
* unsupported source kind별 첫 skipped sample 추가
* `repiu_exe_analyzer` 출력 확장
* `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` 갱신

## 제외 범위

* skipped relocation 적용
* source type 의미 최종 확정
* 원본 코드 실행

## 검증 절차

1. CMake 프로젝트를 구성한다.
2. Debug 빌드를 수행한다.
3. `repiu_exe_analyzer`로 `MASTER\PIU_1ST\PIU.EXE`를 분석한다.
4. 출력에서 source type count와 unsupported kind별 첫 sample을 확인한다.
5. 기존 applied/skipped/failed count가 유지되는지 확인한다.

## Work Order

Print skipped relocation sources by full source type in the internal relocation dry-run.

## Scope

* Add source type counts to `LeRelocationDryRun`.
* Add the first skipped sample for each unsupported source kind.
* Extend `repiu_exe_analyzer` output.
* Update `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, and `docs/EXE_DESIGN.en.md`.

## Out of Scope

* Applying skipped relocations.
* Finalizing source type semantics.
* Executing original code.

## Verification Procedure

1. Configure the CMake project.
2. Build the Debug target.
3. Analyze `MASTER\PIU_1ST\PIU.EXE` with `repiu_exe_analyzer`.
4. Confirm source type counts and the first sample for each unsupported kind.
5. Confirm the existing applied/skipped/failed counts are preserved.
