# OpenWatcom 로컬 샘플 테스트 스위트 작업 로그

## 결과

OpenWatcom 샘플을 Git에 포함하지 않고 로컬 설치물에서 빌드/실행하는 테스트 스위트 기반을 추가했다.

라이선스 판단:

* OpenWatcom v2 공식 라이선스는 Sybase Open Watcom Public License 1.0이다.
  * Repository: `https://github.com/open-watcom/open-watcom-v2`
  * License text: `https://raw.githubusercontent.com/open-watcom/open-watcom-v2/master/license.txt`
* 로컬 설치물에도 `tools/openwatcom/license.txt`가 포함되어 있다.
* `samples/clibexam`, `samples/cplbexam`에서 샘플별 별도 permissive license는 확인하지 못했다.
* 따라서 샘플 소스와 EXE는 Git에 포함하지 않는다.
* 빌드 산출물과 report는 Git 제외 경로인 `build/` 아래에 둔다.

구현 내용:

* `repiu_loader_win32`가 target id 외에 executable path를 직접 받을 수 있게 했다.
* direct executable path는 임시 `direct_executable` target profile로 실행한다.
* direct executable path의 HLE profile은 `dos4gw_console_sample`을 사용한다.
* 샘플별 target profile은 추가하지 않았다.
* `scripts/build_openwatcom_samples.ps1`를 추가했다.
* 빌드 스크립트는 로컬 OpenWatcom `clibexam`, `cplbexam` 샘플을 안정적인 정렬 순서로 열거한다.
* 랜덤 샘플링이나 일부 샘플 제한 없이 선택된 suite의 모든 소스 파일을 대상으로 한다.
* 각 샘플을 `build/openwatcom_samples/` 아래에 DOS/4GW EXE로 빌드한다.
* 빌드 결과 manifest를 `build/openwatcom_samples/manifest.json`에 생성한다.
* `scripts/test_openwatcom_samples.ps1`는 빌드 없이 manifest를 읽어 loader direct path로 실행한다.
* loader exit code 0이고 `exception caught: false`가 출력되면 run pass로 집계한다.
* 결과 HTML report를 `build/openwatcom_sample_report/index.html`에 생성한다.
* 이번 실행 summary JSON을 `build/openwatcom_sample_report/summary.json`에 생성한다.
* 현재 기준선 baseline을 `tests/baselines/openwatcom_samples.json`에 Git 관리 파일로 생성한다.
* baseline 갱신 시 `tests/history/openwatcom_samples/` 아래에 날짜와 버전이 포함된 milestone JSON을 누적 저장한다.
* `VERSION` 파일을 추가하고 현재 프로젝트 버전을 `0.0.1`로 설정했다.
* summary, baseline, milestone history에 프로젝트 버전을 포함한다.
* `-CompareBaseline`은 baseline 대비 새 pass, regression, 새 샘플, 사라진 샘플을 구분한다.
* 이전 pass가 현재 pass가 아니거나 baseline 샘플이 사라지면 비교 실패로 처리한다.
* 빌드 옵션은 `-Suites`, `-ManifestPath`, `-SkipSetup`을 제공한다.
* 테스트 옵션은 `-ManifestPath`, `-ReportPath`, `-SummaryPath`, `-RegressionPath`, `-BaselinePath`, `-HistoryPath`, `-CompareBaseline`, `-UpdateBaseline`을 제공한다.
* 머지 시 patch 증가, minor/major 증가 시 하위 버전 리셋 규칙을 `AGENTS.md`와 관련 문서에 남겼다.

## 검증

`powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

결과: 성공.

`build\win32_x86_debug\Debug\repiu_loader_win32.exe samples\dos4gw_hello\build\hello.exe`

결과: 성공.

* direct executable path가 `direct_executable` target으로 실행됐다.
* `Hello, world!` 출력 확인.
* original entry returned to host trampoline.

`powershell -ExecutionPolicy Bypass -File scripts\build_openwatcom_samples.ps1 -SkipSetup`

결과: 성공.

* 기본 suite 전체를 안정적인 순서로 빌드했다.
* 대상: 819개.
* 빌드 성공: 788개.
* manifest 생성: `build/openwatcom_samples/manifest.json`

`powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1`

결과: 성공.

* manifest의 819개 전체 샘플을 대상으로 loader 실행 report를 생성했다.
* Overall pass: 419 / 819, 51.2%.
* Build pass: 788 / 819, 96.2%.
* Run pass: 419 / 788 runnable, 53.2%.
* HTML report 생성: `build/openwatcom_sample_report/index.html`
* Summary JSON 생성: `build/openwatcom_sample_report/summary.json`

`powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -UpdateBaseline`

결과: 성공.

* baseline 생성: `tests/baselines/openwatcom_samples.json`
* milestone history 생성: `tests/history/openwatcom_samples/20260709-171446-0.0.1.json`
* version: `0.0.1`.
* baseline 대상: 819개.
* baseline Overall pass: 419 / 819.
* baseline Build pass: 788 / 819.
* baseline Run pass: 419 / 788 runnable.

`powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -CompareBaseline`

결과: 성공.

* new pass: 0.
* regression: 0.
* new sample: 0.
* missing sample: 0.
* 비교 결과 JSON 생성: `build/openwatcom_sample_report/regressions.json`

## 다음 작업

전체 샘플 pass율은 고정된 manifest와 Git 관리 baseline 기준으로 비교한다.
샘플 목록이 바뀌면 먼저 `scripts/build_openwatcom_samples.ps1`로 manifest를 다시 생성하고, 이후 `scripts/test_openwatcom_samples.ps1 -CompareBaseline`으로 regression을 확인한다.
의도한 개선 또는 대상 변경이면 `scripts/test_openwatcom_samples.ps1 -UpdateBaseline`으로 baseline을 갱신한다.
현재 loader의 다음 기능 개선은 기존 `piu_1st` 관찰 지점인 `[8B] 06` DS 기반 memory read 처리이다.

# OpenWatcom Local Sample Suite Work Log

## Result

Added the foundation for a test suite that builds and runs OpenWatcom samples from the local installation without committing those samples to Git.

License decision:

* OpenWatcom v2's official license is the Sybase Open Watcom Public License 1.0.
  * Repository: `https://github.com/open-watcom/open-watcom-v2`
  * License text: `https://raw.githubusercontent.com/open-watcom/open-watcom-v2/master/license.txt`
* The local installation also includes `tools/openwatcom/license.txt`.
* No sample-specific permissive license was found under `samples/clibexam` or `samples/cplbexam`.
* Therefore, sample sources and EXEs are not committed to Git.
* Build outputs and reports are generated under the Git-excluded `build/` tree.

Implemented changes:

* `repiu_loader_win32` can now accept an executable path in addition to target ids.
* Direct executable paths run through a temporary `direct_executable` target profile.
* Direct executable paths use the `dos4gw_console_sample` HLE profile.
* No per-sample target profiles were added.
* Added `scripts/build_openwatcom_samples.ps1`.
* The build script enumerates local OpenWatcom `clibexam` and `cplbexam` samples in a stable sorted order.
* Every source file in the selected suites is targeted without random sampling or partial sample limits.
* Each sample is built as a DOS/4GW EXE under `build/openwatcom_samples/`.
* The build manifest is generated at `build/openwatcom_samples/manifest.json`.
* `scripts/test_openwatcom_samples.ps1` reads the manifest and runs samples through the loader by direct path without building.
* Run pass is counted when loader exit code is 0 and `exception caught: false` appears in output.
* The HTML report is written to `build/openwatcom_sample_report/index.html`.
* The current summary JSON is written to `build/openwatcom_sample_report/summary.json`.
* The current baseline is stored as a Git-tracked file at `tests/baselines/openwatcom_samples.json`.
* Baseline updates append dated and versioned milestone JSON under `tests/history/openwatcom_samples/`.
* Added the `VERSION` file and set the current project version to `0.0.1`.
* Summary, baseline, and milestone history include the project version.
* `-CompareBaseline` distinguishes new passes, regressions, new samples, and missing samples.
* The comparison fails when a previous pass no longer passes or a baseline sample is missing.
* The build script provides `-Suites`, `-ManifestPath`, and `-SkipSetup`.
* The test script provides `-ManifestPath`, `-ReportPath`, `-SummaryPath`, `-RegressionPath`, `-BaselinePath`, `-HistoryPath`, `-CompareBaseline`, and `-UpdateBaseline`.
* Documented the merge-time patch bump and minor/major reset rules in `AGENTS.md` and related docs.

## Verification

`powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

Result: success.

`build\win32_x86_debug\Debug\repiu_loader_win32.exe samples\dos4gw_hello\build\hello.exe`

Result: success.

* Direct executable path ran as the `direct_executable` target.
* Confirmed `Hello, world!` output.
* The original entry returned to the host trampoline.

`powershell -ExecutionPolicy Bypass -File scripts\build_openwatcom_samples.ps1 -SkipSetup`

Result: success.

* Built the full default suite set in stable order.
* Total targets: 819.
* Build pass: 788.
* Manifest generated at `build/openwatcom_samples/manifest.json`.

`powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1`

Result: success.

* Generated a loader execution report for all 819 manifest samples.
* Overall pass: 419 / 819, 51.2%.
* Build pass: 788 / 819, 96.2%.
* Run pass: 419 / 788 runnable, 53.2%.
* HTML report generated at `build/openwatcom_sample_report/index.html`.
* Summary JSON generated at `build/openwatcom_sample_report/summary.json`.

`powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -UpdateBaseline`

Result: success.

* Baseline generated at `tests/baselines/openwatcom_samples.json`.
* Milestone history generated at `tests/history/openwatcom_samples/20260709-171446-0.0.1.json`.
* Version: `0.0.1`.
* Baseline total: 819.
* Baseline overall pass: 419 / 819.
* Baseline build pass: 788 / 819.
* Baseline run pass: 419 / 788 runnable.

`powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -CompareBaseline`

Result: success.

* New pass: 0.
* Regression: 0.
* New sample: 0.
* Missing sample: 0.
* Comparison JSON generated at `build/openwatcom_sample_report/regressions.json`.

## Next Work

Compare sample pass rates against the fixed manifest and Git-tracked baseline.
When the sample list changes, regenerate the manifest with `scripts/build_openwatcom_samples.ps1`, then check regressions with `scripts/test_openwatcom_samples.ps1 -CompareBaseline`.
When an improvement or target-list change is intentional, update the baseline with `scripts/test_openwatcom_samples.ps1 -UpdateBaseline`.
The next loader feature remains handling the current `piu_1st` observation point, `[8B] 06`, as a DS-based memory read.
