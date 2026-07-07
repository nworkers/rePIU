# 정적 TargetProfile 작업 지시

DOS/4GW 공용 로더 방향에 맞춰 정적 target profile 등록 구조를 추가한다.

## 작업 범위

* `TargetProfile`과 format hint 구조 추가
* 내장 `piu_1st` target profile 등록
* profile id 조회 함수 추가
* `repiu_exe_analyzer`의 기본 경로를 target profile 기반으로 변경
* `ARCHITECTURE.md`, `docs/PROJECT_CHARTER.md` 갱신

## 제외 범위

* 동적 플러그인 시스템
* HLE override 실행
* DOS/4GW loader class 리팩터링
* 원본 코드 실행

## 검증 절차

1. Debug 빌드를 수행한다.
2. `repiu_exe_analyzer`를 인자 없이 실행한다.
3. 출력에서 기본 target과 profile 기반 경로를 확인한다.
4. `repiu_exe_analyzer MASTER\PIU_1ST\PIU.EXE`를 실행해 명시 경로 분석이 유지되는지 확인한다.

## Work Order

Add a static target profile registration structure aligned with the shared DOS/4GW loader direction.

## Scope

* Add `TargetProfile` and format hint structures.
* Register the built-in `piu_1st` target profile.
* Add profile id lookup.
* Change the default path in `repiu_exe_analyzer` to use the target profile.
* Update `ARCHITECTURE.md` and `docs/PROJECT_CHARTER.md`.

## Out of Scope

* Dynamic plugin system.
* Executing HLE overrides.
* DOS/4GW loader class refactoring.
* Executing original code.

## Verification Procedure

1. Build the Debug target.
2. Run `repiu_exe_analyzer` without arguments.
3. Confirm the default target and profile-based path in the output.
4. Run `repiu_exe_analyzer MASTER\PIU_1ST\PIU.EXE` and confirm explicit path analysis still works.
