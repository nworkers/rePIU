# OpenWatcom 로컬 샘플 테스트 스위트 설계

## 배경

OpenWatcom 설치물에는 `samples/clibexam`과 `samples/cplbexam` 예제가 포함되어 있다.
이 샘플들은 DOS/4GW console runtime 경로의 호환성을 넓게 확인하는 데 유용하다.

다만 OpenWatcom 샘플 소스와 빌드 산출물을 Git에 직접 포함하는 것은 현재 프로젝트 라이선스 정책과 충돌할 수 있다.
OpenWatcom v2의 공식 라이선스는 Sybase Open Watcom Public License 1.0이며, source/object 배포 조건과 Covered Code 관련 조건이 있다.
로컬 설치물의 `samples/clibexam`, `samples/cplbexam`에서 별도 permissive 라이선스 고지는 확인하지 못했다.

따라서 이번 작업은 OpenWatcom 샘플을 저장소에 vendoring하지 않는다.
로컬 `tools/openwatcom/` 설치물에서 샘플을 빌드하고, 생성된 EXE와 HTML report도 Git 제외 경로에 둔다.

## 라이선스 판단

확인한 근거:

* OpenWatcom v2 repository의 루트 `license.txt`는 Sybase Open Watcom Public License 1.0을 명시한다.
  * Repository: `https://github.com/open-watcom/open-watcom-v2`
  * License text: `https://raw.githubusercontent.com/open-watcom/open-watcom-v2/master/license.txt`
* 로컬 설치물에도 `tools/openwatcom/license.txt`가 포함되어 있다.
* 라이선스 본문에는 Covered Code, Deploy, Source Code 공개/고지, object code 배포 시 source 제공 고지 조건이 있다.
* `samples/clibexam`, `samples/cplbexam`에는 샘플별 별도 permissive license 파일을 확인하지 못했다.

결론:

* OpenWatcom 샘플 소스와 EXE는 Git에 포함하지 않는다.
* 테스트 스크립트와 리포트 생성 로직만 Git에 포함한다.
* 샘플은 로컬 OpenWatcom 설치물에서 읽고, 빌드 산출물은 `build/` 아래에 생성한다.

## 목표

* OpenWatcom 로컬 샘플을 일괄 빌드하고 loader로 실행한다.
* `clibexam`과 `cplbexam` 샘플을 대상으로 한다.
* 매번 같은 pass율을 비교할 수 있도록 선택된 suite의 모든 소스 파일을 안정적인 정렬 순서로 대상으로 한다.
* 샘플 빌드 단계와 loader 실행 테스트 단계를 분리한다.
* profile을 샘플마다 추가하지 않는다.
* loader가 executable path를 직접 받아 실행할 수 있게 한다.
* 결과를 HTML report로 남긴다.
* 전체 pass율, build pass율, run pass율을 표시한다.
* 샘플별 상태를 Git 관리 baseline으로 저장해 새 pass와 regression을 구분한다.
* baseline 갱신 시 날짜별 milestone history JSON도 함께 저장해 이후 dashboard 입력으로 사용할 수 있게 한다.
* 각 report, baseline, history에는 프로젝트 버전 `major.minor.patch`를 포함한다.

## 비목표

* OpenWatcom 샘플 소스 또는 EXE를 Git에 커밋하지 않는다.
* 샘플별 target profile을 추가하지 않는다.
* 샘플별 기대 stdout을 수작업으로 정의하지 않는다.
* 랜덤 샘플링 또는 일부 샘플만 대상으로 하는 pass율을 만들지 않는다.
* loader의 HLE 범위를 이번 작업에서 확장하지 않는다.

## 설계

### Loader direct executable path

`repiu_loader_win32`는 기존 target id를 먼저 찾는다.
target id가 없고 인자가 파일 경로라면 임시 target profile을 만든다.

임시 profile:

* id: `direct_executable`
* executable path: 입력 경로
* working directory: executable parent directory
* asset root: executable parent directory
* HLE profile: `dos4gw_console_sample`
* runtime reserve hint: `0x00010000`, `0x00800000`

이 방식은 샘플마다 profile을 추가하지 않으면서 loader 경로를 그대로 사용한다.

### Sample build script

`scripts/build_openwatcom_samples.ps1`를 추가한다.

동작:

1. `scripts/setup_test_environment.ps1`를 호출해 OpenWatcom, CMake, loader 전제를 준비한다.
2. `scripts/build_win32_x86.bat`로 loader를 빌드한다.
3. `tools/openwatcom/samples/clibexam/**/*.c`와 `tools/openwatcom/samples/cplbexam/**/*.{c,cpp,cxx,cc}`를 열거한다.
4. 열거 결과는 이름순으로 정렬하며, 선택된 suite의 모든 파일을 대상으로 한다.
5. 각 source를 `build/openwatcom_samples/<safe-name>/sample.exe`로 빌드한다.
6. 빌드 결과 manifest를 `build/openwatcom_samples/manifest.json`에 쓴다.

옵션:

* `-Suites`: `clibexam`, `cplbexam` 중 실행할 suite를 선택한다. 기본값은 둘 다이다.
* `-ManifestPath`: 빌드 manifest 출력 경로를 바꾼다.
* `-SkipSetup`: 환경 준비 단계를 건너뛴다.

### Sample test script

`scripts/test_openwatcom_samples.ps1`는 빌드를 수행하지 않고 기존 manifest의 빌드 성공 EXE만 loader로 실행한다.

동작:

1. `build/openwatcom_samples/manifest.json`을 읽는다.
2. manifest에 기록된 모든 샘플을 순서대로 처리한다.
3. 빌드 실패 샘플은 run 대상에서 제외하고 report에는 build fail로 남긴다.
4. 빌드 성공한 EXE를 `repiu_loader_win32.exe <exe path>`로 실행한다.
5. loader exit code 0이고 `exception caught: false`가 출력되면 run pass로 본다.
6. HTML report를 `build/openwatcom_sample_report/index.html`에 쓴다.
7. 이번 실행 요약과 샘플별 상태를 `build/openwatcom_sample_report/summary.json`에 쓴다.
8. 요청 시 Git 관리 baseline과 비교하거나 baseline을 갱신한다.
9. baseline 갱신 시 `tests/history/openwatcom_samples/` 아래에 날짜와 버전이 포함된 누적 JSON을 함께 쓴다.

옵션:

* `-ManifestPath`: 입력 manifest 경로를 바꾼다.
* `-ReportPath`: HTML report 출력 경로를 바꾼다.
* `-SummaryPath`: 이번 실행 요약 JSON 출력 경로를 바꾼다.
* `-RegressionPath`: baseline 비교 결과 JSON 출력 경로를 바꾼다.
* `-BaselinePath`: Git 관리 baseline 경로를 바꾼다. 기본값은 `tests/baselines/openwatcom_samples.json`이다.
* `-HistoryPath`: Git 관리 milestone history 디렉터리를 바꾼다. 기본값은 `tests/history/openwatcom_samples`이다.
* `-CompareBaseline`: 현재 결과를 baseline과 비교한다. 이전 pass가 현재 fail이 되거나 baseline sample이 사라지면 실패한다.
* `-UpdateBaseline`: 현재 결과를 baseline으로 저장하고 날짜별 history JSON도 추가한다.

### Baseline policy

Git에는 매 실행 history를 모두 누적하지 않고 현재 기준선만 저장한다.
기본 baseline은 `tests/baselines/openwatcom_samples.json`이다.
마일스톤 누적 history는 `tests/history/openwatcom_samples/YYYYMMDD-HHMMSS-<version>.json` 형식으로 저장한다.
history는 baseline 갱신 시점의 결과만 저장하며, dashboard 입력으로 사용할 수 있는 summary와 샘플별 상태를 포함한다.

baseline은 summary와 샘플별 상태를 포함한다.
summary에는 `VERSION` 파일에서 읽은 프로젝트 버전을 포함한다.
샘플 상태는 `pass`, `build_fail`, `run_fail`, `not_run` 중 하나이다.
비교 결과는 다음 범주로 나눈다.

* `new pass`: baseline에서 실패였던 샘플이 현재 pass가 된 경우
* `regression`: baseline에서 pass였던 샘플이 현재 pass가 아닌 경우
* `unchanged fail`: 계속 pass가 아닌 경우
* `new sample`: baseline에는 없고 현재 manifest에 있는 경우
* `missing sample`: baseline에는 있고 현재 manifest에는 없는 경우

### Version policy

프로젝트 버전은 저장소 루트의 `VERSION` 파일에서 `major.minor.patch` 형식으로 관리한다.
초기 버전은 `0.0.1`이다.

머지 요청 시 `main`에 머지하기 전에 patch 버전을 1 증가시킨다.
minor 버전 증가 요청이 있으면 minor를 1 증가시키고 patch는 0으로 리셋한다.
major 버전 증가 요청이 있으면 major를 1 증가시키고 minor와 patch는 0으로 리셋한다.

## 검증

* direct executable path로 기존 `samples/dos4gw_hello/build/hello.exe`를 실행할 수 있어야 한다.
* `scripts/build_openwatcom_samples.ps1 -Suites clibexam -SkipSetup`가 선택 suite 전체에 대한 manifest를 생성해야 한다.
* `scripts/test_openwatcom_samples.ps1`가 manifest 전체를 대상으로 HTML report를 생성해야 한다.
* `scripts/test_openwatcom_samples.ps1 -UpdateBaseline`가 baseline을 생성해야 한다.
* `scripts/test_openwatcom_samples.ps1 -UpdateBaseline`가 날짜별 history JSON도 생성해야 한다.
* `scripts/test_openwatcom_samples.ps1 -CompareBaseline`가 baseline과 현재 결과를 비교해야 한다.
* `scripts/build_openwatcom_samples.ps1 -Suites cplbexam -SkipSetup`와 `scripts/test_openwatcom_samples.ps1` 조합이 C++ 샘플 report를 생성해야 한다.
* `scripts/test_all.ps1`가 계속 성공해야 한다.

# OpenWatcom Local Sample Suite Design

## Background

The OpenWatcom installation includes examples under `samples/clibexam` and `samples/cplbexam`.
These samples are useful for broader DOS/4GW console runtime compatibility checks.

However, committing OpenWatcom sample sources and build outputs directly to Git may conflict with the project's current license policy.
OpenWatcom v2's official license is the Sybase Open Watcom Public License 1.0, which includes conditions related to source/object distribution and Covered Code.
No separate permissive license notice was found for the local `samples/clibexam` and `samples/cplbexam` trees.

Therefore, this task does not vendor OpenWatcom samples into the repository.
It builds samples from the local `tools/openwatcom/` installation, and generated EXEs plus HTML reports live under Git-excluded paths.

## License Assessment

Evidence checked:

* The OpenWatcom v2 repository root `license.txt` states the Sybase Open Watcom Public License 1.0.
  * Repository: `https://github.com/open-watcom/open-watcom-v2`
  * License text: `https://raw.githubusercontent.com/open-watcom/open-watcom-v2/master/license.txt`
* The local installation also includes `tools/openwatcom/license.txt`.
* The license text includes Covered Code, Deploy, source availability/notice, and object-code source notice conditions.
* No sample-specific permissive license file was found under `samples/clibexam` or `samples/cplbexam`.

Conclusion:

* Do not commit OpenWatcom sample sources or EXEs to Git.
* Commit only the test script and report generation logic.
* Read samples from the local OpenWatcom installation and generate build outputs under `build/`.

## Goals

* Batch-build local OpenWatcom samples and run them through the loader.
* Target the `clibexam` and `cplbexam` sample trees.
* Use every source file in the selected suites in a stable sorted order so pass rates are comparable across runs.
* Separate the sample build step from the loader test step.
* Do not add one profile per sample.
* Let the loader accept an executable path directly.
* Write results to an HTML report.
* Show overall pass rate, build pass rate, and run pass rate.
* Store per-sample status in a Git-tracked baseline so new passes and regressions can be distinguished.
* When the baseline is updated, also write dated milestone history JSON for future dashboard input.
* Include the project `major.minor.patch` version in each report, baseline, and history file.

## Non-Goals

* Do not commit OpenWatcom sample sources or EXEs.
* Do not add per-sample target profiles.
* Do not manually define expected stdout per sample.
* Do not create pass rates from random sampling or partial sample subsets.
* Do not expand loader HLE coverage in this task.

## Design

### Loader Direct Executable Path

`repiu_loader_win32` first looks up the argument as an existing target id.
If no target id matches and the argument is a file path, it creates a temporary target profile.

Temporary profile:

* id: `direct_executable`
* executable path: input path
* working directory: executable parent directory
* asset root: executable parent directory
* HLE profile: `dos4gw_console_sample`
* runtime reserve hint: `0x00010000`, `0x00800000`

This keeps the loader path intact without adding one profile per sample.

### Sample Build Script

Add `scripts/build_openwatcom_samples.ps1`.

Behavior:

1. Calls `scripts/setup_test_environment.ps1` to prepare OpenWatcom, CMake, and loader prerequisites.
2. Builds the loader with `scripts/build_win32_x86.bat`.
3. Enumerates `tools/openwatcom/samples/clibexam/**/*.c` and `tools/openwatcom/samples/cplbexam/**/*.{c,cpp,cxx,cc}`.
4. Sorts the enumeration by name and targets every file in the selected suites.
5. Builds each source into `build/openwatcom_samples/<safe-name>/sample.exe`.
6. Writes the build manifest to `build/openwatcom_samples/manifest.json`.

Options:

* `-Suites`: selects which suites to run from `clibexam` and `cplbexam`. Defaults to both.
* `-ManifestPath`: overrides the build manifest path.
* `-SkipSetup`: skips environment preparation.

### Sample Test Script

`scripts/test_openwatcom_samples.ps1` does not build samples. It runs built EXEs from the existing manifest through the loader.

Behavior:

1. Reads `build/openwatcom_samples/manifest.json`.
2. Processes every sample recorded in the manifest in order.
3. Keeps build-failed samples in the report but excludes them from run attempts.
4. Runs each built EXE through `repiu_loader_win32.exe <exe path>`.
5. Counts run pass when loader exit code is 0 and output contains `exception caught: false`.
6. Writes an HTML report to `build/openwatcom_sample_report/index.html`.
7. Writes the current summary and per-sample status to `build/openwatcom_sample_report/summary.json`.
8. Optionally compares against or updates the Git-tracked baseline.
9. When updating the baseline, also writes dated versioned history JSON under `tests/history/openwatcom_samples/`.

Options:

* `-ManifestPath`: overrides the input manifest path.
* `-ReportPath`: overrides the HTML report path.
* `-SummaryPath`: overrides the current summary JSON path.
* `-RegressionPath`: overrides the baseline comparison JSON path.
* `-BaselinePath`: overrides the Git-tracked baseline path. The default is `tests/baselines/openwatcom_samples.json`.
* `-HistoryPath`: overrides the Git-tracked milestone history directory. The default is `tests/history/openwatcom_samples`.
* `-CompareBaseline`: compares the current result against the baseline. The script fails when a previous pass no longer passes or a baseline sample is missing.
* `-UpdateBaseline`: writes the current result as the baseline and appends dated history JSON.

### Baseline Policy

Git stores only the current baseline, not every run history.
The default baseline is `tests/baselines/openwatcom_samples.json`.
Milestone history is stored as `tests/history/openwatcom_samples/YYYYMMDD-HHMMSS-<version>.json`.
History is written only when the baseline is updated, and it contains dashboard-ready summary plus per-sample status.

The baseline contains a summary and per-sample status.
The summary includes the project version read from `VERSION`.
Sample status is one of `pass`, `build_fail`, `run_fail`, or `not_run`.
Comparison results are grouped as follows.

* `new pass`: a baseline failure passes now
* `regression`: a baseline pass no longer passes
* `unchanged fail`: a sample still does not pass
* `new sample`: a sample is present now but missing from the baseline
* `missing sample`: a sample is present in the baseline but missing from the current manifest

### Version Policy

The project version is stored in the repository-root `VERSION` file using `major.minor.patch`.
The initial version is `0.0.1`.

When the user requests a merge, increment the patch version by 1 before merging into `main`.
When the user requests a minor version bump, increment the minor version by 1 and reset the patch version to 0.
When the user requests a major version bump, increment the major version by 1 and reset the minor and patch versions to 0.

## Verification

* Existing `samples/dos4gw_hello/build/hello.exe` must run by direct executable path.
* `scripts/build_openwatcom_samples.ps1 -Suites clibexam -SkipSetup` must generate a manifest for the full selected suite.
* `scripts/test_openwatcom_samples.ps1` must generate an HTML report for the full manifest.
* `scripts/test_openwatcom_samples.ps1 -UpdateBaseline` must generate the baseline.
* `scripts/test_openwatcom_samples.ps1 -UpdateBaseline` must also generate dated history JSON.
* `scripts/test_openwatcom_samples.ps1 -CompareBaseline` must compare the current result with the baseline.
* `scripts/build_openwatcom_samples.ps1 -Suites cplbexam -SkipSetup` plus `scripts/test_openwatcom_samples.ps1` must generate a C++ sample report.
* `scripts/test_all.ps1` must continue to pass.
