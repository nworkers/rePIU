# OpenWatcom 히스토리 HTML 리포트 설계

## 배경

OpenWatcom 샘플 baseline 갱신은 `tests/baselines/openwatcom_samples.json`을 갱신하고, 같은 결과를 `tests/history/openwatcom_samples/` 아래의 날짜별 JSON으로 남긴다.
`tests` 디렉터리는 이미 Git 추적 대상이며 `.gitignore`에서 제외되어 있지 않다.

JSON은 dashboard 입력으로 좋지만, 사람이 특정 baseline 갱신 시점의 실패/skip/run 상태를 바로 확인하기에는 HTML report가 더 편하다.

## 설계

`scripts/test_openwatcom_samples.ps1 -UpdateBaseline`은 기존 HTML report를 먼저 `build/openwatcom_sample_report/index.html`에 생성한다.
baseline 갱신 단계에서는 같은 timestamp/version basename을 사용해 다음 두 파일을 함께 남긴다.

* `tests/history/openwatcom_samples/YYYYMMDD-HHMMSS-<version>.json`
* `tests/history/openwatcom_samples/YYYYMMDD-HHMMSS-<version>.html`

HTML 파일은 해당 실행의 report snapshot이다.
JSON과 HTML은 같은 basename을 공유하므로, 특정 milestone의 구조화 데이터와 사람이 읽는 report를 쉽게 짝지을 수 있다.

## 검증

* `git ls-files tests`로 `tests`가 Git 추적 대상인지 확인한다.
* `git check-ignore tests tests/baselines/openwatcom_samples.json tests/history/openwatcom_samples`가 아무 항목도 출력하지 않는지 확인한다.
* `scripts/test_openwatcom_samples.ps1 -UpdateBaseline`을 실행한다.
* 새 history JSON과 같은 basename의 HTML이 생성되는지 확인한다.

# OpenWatcom History HTML Report Design

## Background

OpenWatcom sample baseline updates refresh `tests/baselines/openwatcom_samples.json` and store the same result as dated JSON under `tests/history/openwatcom_samples/`.
The `tests` directory is already tracked by Git and is not excluded by `.gitignore`.

JSON is useful as dashboard input, but an HTML report is easier for humans to inspect the fail/skip/run state for a specific baseline update.

## Design

`scripts/test_openwatcom_samples.ps1 -UpdateBaseline` already writes the current HTML report to `build/openwatcom_sample_report/index.html` before updating the baseline.
During the baseline update step, it now writes these two files with the same timestamp/version basename:

* `tests/history/openwatcom_samples/YYYYMMDD-HHMMSS-<version>.json`
* `tests/history/openwatcom_samples/YYYYMMDD-HHMMSS-<version>.html`

The HTML file is a report snapshot for that run.
JSON and HTML share the same basename so the structured data and human-readable report for a milestone can be paired easily.

## Verification

* Confirm `tests` is tracked with `git ls-files tests`.
* Confirm `git check-ignore tests tests/baselines/openwatcom_samples.json tests/history/openwatcom_samples` prints nothing.
* Run `scripts/test_openwatcom_samples.ps1 -UpdateBaseline`.
* Confirm a new history JSON and same-basename HTML are generated.
