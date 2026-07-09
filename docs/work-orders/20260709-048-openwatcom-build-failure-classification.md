# OpenWatcom 샘플 빌드 실패 분류 작업 지시

## 목표

OpenWatcom 로컬 샘플 빌드에서 DOS/4GW console 대상과 맞지 않는 샘플을 실패가 아닌 명시적 skip으로 분류하고, 옵션 누락 샘플은 빌드되도록 한다.

## 작업 범위

1. OpenWatcom 샘플 빌드 실패 유형을 설계 문서에 기록한다.
2. `scripts/build_openwatcom_samples.ps1`에 샘플별 build plan을 추가한다.
3. `setnew.c`에는 `-cc++`를 적용한다.
4. C++ 예외 샘플 4개에는 `-xs`를 적용한다.
5. DOS/4GW console 대상과 맞지 않는 26개 샘플은 `BuildStatus = "skip"`으로 기록한다.
6. `scripts/test_openwatcom_samples.ps1`가 skip 상태를 report와 summary에 반영하도록 갱신한다.
7. 샘플 빌드 계획만 검증할 수 있도록 `-SkipHostBuild` 옵션을 추가한다.
8. `ARCHITECTURE.md`의 OpenWatcom 샘플 스위트 설명에 build plan/skip 정책을 반영한다.
9. 빌드와 baseline 비교를 실행해 회귀가 없는지 확인한다.
10. 작업 로그를 남긴다.

## 비목표

* OpenWatcom 원본 샘플 소스를 수정하지 않는다.
* OpenWatcom 샘플 소스나 EXE를 Git에 추가하지 않는다.
* loader HLE 범위를 이 작업에서 확장하지 않는다.
* 실행 실패 샘플을 이번 작업에서 해결하지 않는다.

## 검증

* `scripts/build_openwatcom_samples.ps1 -SkipSetup -SkipHostBuild`
* manifest에서 `BuildStatus = "fail"`이 0개인지 확인
* manifest에서 `BuildStatus = "skip"`이 26개인지 확인
* `scripts/test_openwatcom_samples.ps1 -CompareBaseline`

# OpenWatcom Sample Build Failure Classification Work Order

## Goal

Classify OpenWatcom local samples that do not match the DOS/4GW console target as explicit skips instead of failures, and build samples that were failing only because of missing options.

## Scope

1. Document the OpenWatcom sample build failure categories in the design document.
2. Add a per-sample build plan to `scripts/build_openwatcom_samples.ps1`.
3. Apply `-cc++` to `setnew.c`.
4. Apply `-xs` to the four C++ exception samples.
5. Record 26 samples that do not fit the DOS/4GW console target as `BuildStatus = "skip"`.
6. Update `scripts/test_openwatcom_samples.ps1` to reflect skipped samples in reports and summaries.
7. Add a `-SkipHostBuild` option so only the sample build plan can be verified.
8. Reflect the build plan and skip policy in `ARCHITECTURE.md`.
9. Run the build and baseline comparison to check for regressions.
10. Leave a work log.

## Non-Goals

* Do not modify original OpenWatcom sample sources.
* Do not add OpenWatcom sample sources or EXEs to Git.
* Do not expand loader HLE coverage in this task.
* Do not fix runtime-failing samples in this task.

## Verification

* `scripts/build_openwatcom_samples.ps1 -SkipSetup -SkipHostBuild`
* Confirm that `BuildStatus = "fail"` count is 0 in the manifest.
* Confirm that `BuildStatus = "skip"` count is 26 in the manifest.
* `scripts/test_openwatcom_samples.ps1 -CompareBaseline`
