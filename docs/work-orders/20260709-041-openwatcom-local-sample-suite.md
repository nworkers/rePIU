# OpenWatcom 로컬 샘플 테스트 스위트 작업 지시

## 작업 항목

1. OpenWatcom 샘플 소스/EXE를 Git에 포함하지 않는 라이선스 판단을 문서화한다.
2. loader가 target id 외에 executable path를 직접 받을 수 있게 한다.
3. direct executable path는 임시 `dos4gw_console_sample` profile로 실행한다.
4. `scripts/build_openwatcom_samples.ps1`를 추가해 선택된 suite의 모든 샘플을 안정적인 순서로 빌드한다.
5. 빌드 결과 manifest를 `build/openwatcom_samples/manifest.json`에 생성한다.
6. `scripts/test_openwatcom_samples.ps1`는 빌드 없이 manifest의 모든 샘플을 loader로 실행한다.
7. build/run/pass율을 HTML report로 생성한다.
8. 이번 실행 summary JSON을 Git 제외 report 경로에 생성한다.
9. Git 관리 baseline을 `tests/baselines/openwatcom_samples.json`에 저장할 수 있게 한다.
10. baseline 비교 시 새 pass, regression, new sample, missing sample을 구분한다.
11. baseline 갱신 시 `tests/history/openwatcom_samples/` 아래에 날짜별 milestone JSON을 누적 저장한다.
12. `VERSION` 파일을 추가하고 현재 버전을 `0.0.1`로 설정한다.
13. summary, baseline, history에 프로젝트 버전을 포함한다.
14. 머지 시 patch 증가, minor/major 증가 시 하위 버전 리셋 규칙을 문서화한다.
15. 범위에 맞는 검증을 수행한다.
16. 작업 로그를 남긴다.

## 비목표

* OpenWatcom 샘플 소스/EXE 커밋
* 샘플별 profile 추가
* 샘플별 기대 출력 정의
* 랜덤 샘플링 또는 일부 파일만 대상으로 한 pass율 산출
* 매 실행 history 전체를 Git에 누적
* 자동 major/minor 판단

# OpenWatcom Local Sample Suite Work Order

## Tasks

1. Document the license decision not to commit OpenWatcom sample sources or EXEs.
2. Let the loader accept an executable path in addition to target ids.
3. Run direct executable paths through a temporary `dos4gw_console_sample` profile.
4. Add `scripts/build_openwatcom_samples.ps1` to build every sample in the selected suites in a stable order.
5. Generate the build manifest at `build/openwatcom_samples/manifest.json`.
6. Let `scripts/test_openwatcom_samples.ps1` run every manifest sample through the loader without building.
7. Generate build/run/pass rates as an HTML report.
8. Generate the current summary JSON under the Git-excluded report path.
9. Allow storing the Git-tracked baseline at `tests/baselines/openwatcom_samples.json`.
10. Distinguish new passes, regressions, new samples, and missing samples during baseline comparison.
11. When updating the baseline, append dated milestone JSON under `tests/history/openwatcom_samples/`.
12. Add the `VERSION` file and set the current version to `0.0.1`.
13. Include the project version in the summary, baseline, and history.
14. Document the merge-time patch bump and minor/major reset rules.
15. Run verification appropriate to the scope.
16. Leave a work log.

## Non-Goals

* Committing OpenWatcom sample sources or EXEs.
* Adding per-sample profiles.
* Defining expected output per sample.
* Calculating pass rates from random sampling or partial file subsets.
* Committing the full history of every run to Git.
* Automatically deciding major or minor version bumps.
