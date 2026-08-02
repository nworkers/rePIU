# 20260802-396 pumpit3 ZIP+CHD 프로필 추가 계획 / pumpit3 ZIP+CHD Profile Addition Plan

## 한국어

### 목표

`roms/pumpit3.zip` 및 `roms/pumpit3/*.chd` 자산을 탐색하여 자동으로 materialized 런타임 디렉터리를 구축하는 `pumpit3` TargetProfile을 내장 프로필 목록에 추가한다.

### 작업 범위

1. `src/target/target_profile.cpp`: `BuiltInTargetProfiles()`에 `pumpit3` 타겟 프로필 정의 추가.
2. `docs/design/20260802-396-pumpit3-zip-chd-profile.md`: 설계 문서 작성.
3. `docs/work-orders/20260802-396-pumpit3-zip-chd-profile.md`: 작업 지시서 작성.
4. 검증: `exe_analyzer` 및 `repiu_host`를 사용한 `pumpit3` 타겟 동작 및 기존 프로필 회귀 확인.
5. `docs/work-logs/20260802-396-pumpit3-zip-chd-profile.md`: 작업 로그 작성.

### 검증 절차

1. 빌드: `cmake --build build --config Release`
2. `exe_analyzer --target pumpit3` 실행 및 추출된 `PIU/PIU.EXE` 분석 결과 검증
3. `repiu_host --target pumpit3 --smoke-test-frames 120` 실행 및 호스트 실행 검증
4. `pumpit1`, `pumpit2` 타겟 회귀 검증

---

## English

### Objective

Add the `pumpit3` TargetProfile to the built-in profile list to locate `roms/pumpit3.zip` and `roms/pumpit3/*.chd` and materialize runtime directory automatically.

### Task Scope

1. `src/target/target_profile.cpp`: Add `pumpit3` profile definition to `BuiltInTargetProfiles()`.
2. `docs/design/20260802-396-pumpit3-zip-chd-profile.md`: Write design document.
3. `docs/work-orders/20260802-396-pumpit3-zip-chd-profile.md`: Write work order.
4. Verification: Validate `pumpit3` target behavior and check existing profiles for regressions using `exe_analyzer` and `repiu_host`.
5. `docs/work-logs/20260802-396-pumpit3-zip-chd-profile.md`: Write work log.

### Verification Procedure

1. Build: `cmake --build build --config Release`
2. Run `exe_analyzer --target pumpit3` and verify extracted `PIU/PIU.EXE` analysis output
3. Run `repiu_host --target pumpit3 --smoke-test-frames 120` and verify host initialization
4. Verify no regressions on `pumpit1` and `pumpit2` targets
