# 정적 HLE Profile 작업 지시

target profile이 참조하는 `hle_profile_id`를 해석할 수 있는 정적 HLE profile registry를 추가한다.

## 작업 범위

* `HleProfile` 구조 추가
* HLE service enum과 이름 변환 함수 추가
* 내장 `piu_common` HLE profile 등록
* `repiu_exe_analyzer` 출력에 HLE profile 이름과 service 목록 추가
* `ARCHITECTURE.md`, `docs/PROJECT_CHARTER.md` 갱신

## 제외 범위

* 실제 DOS/DPMI/HW HLE 구현
* HLE hook 실행
* 동적 플러그인 시스템
* 원본 코드 실행

## 검증 절차

1. Debug 빌드를 수행한다.
2. `repiu_exe_analyzer`를 인자 없이 실행한다.
3. 출력에서 `HLE profile: piu_common`, HLE profile 이름, HLE service 목록을 확인한다.
4. 기존 relocation dry-run 결과가 유지되는지 확인한다.

## Work Order

Add a static HLE profile registry that resolves the `hle_profile_id` referenced by target profiles.

## Scope

* Add the `HleProfile` structure.
* Add an HLE service enum and name conversion function.
* Register the built-in `piu_common` HLE profile.
* Add the HLE profile name and service list to `repiu_exe_analyzer` output.
* Update `ARCHITECTURE.md` and `docs/PROJECT_CHARTER.md`.

## Out of Scope

* Actual DOS/DPMI/HW HLE implementation.
* Executing HLE hooks.
* Dynamic plugin system.
* Executing original code.

## Verification Procedure

1. Build the Debug target.
2. Run `repiu_exe_analyzer` without arguments.
3. Confirm `HLE profile: piu_common`, the HLE profile name, and the HLE service list in the output.
4. Confirm the existing relocation dry-run result is preserved.
