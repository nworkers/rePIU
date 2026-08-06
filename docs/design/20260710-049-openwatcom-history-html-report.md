# OpenWatcom 히스토리 HTML 리포트 설계

> **[Task 434에서 철회] HTML 스냅샷은 더 이상 만들지 않습니다.**
> 이 설계가 세워졌을 때는 과거 실행을 사람이 읽을 방법이 이 파일뿐이었습니다. 그 뒤
> 스냅샷이 담는 detail이 늘면서 파일이 **0.0.5의 5.6 MB에서 0.0.135의 28 MB까지**
> 커졌고, 디렉터리 전체가 **127 MB**가 됐습니다. git 히스토리는 지울 수 없으므로 갱신
> 한 번마다 저장소가 영구히 무거워집니다.
> 릴리스 워크플로가 같은 리포트를 **빌드 아티팩트로 업로드**하므로 사람이 읽을 사본은
> 따로 있고, 비교의 근거가 되는 구조화 데이터는 JSON이 그대로 유지합니다.
> 아래 본문은 당시 기록으로 보존합니다. 근거:
> [Task 434 설계 §6.1](20260806-434-github-actions-release-ci.md) ·
> [작업 로그](../work-logs/20260806-434-github-actions-release-ci.md)

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

> **[Withdrawn in Task 434] The HTML snapshot is no longer written.**
> When this design was written, that file was the only way to read a past run. The snapshots
> then grew with the detail they carry — **5.6 MB at 0.0.5 against 28 MB at 0.0.135, 127 MB
> across the directory** — and git history cannot be pruned, so every update made the
> repository permanently heavier. The release workflow now uploads the same report as a **build
> artifact**, so the readable copy has another home while the JSON keeps the structured data
> comparisons rest on. The text below is preserved as the record of its moment. See
> [Task 434 design §6.1](20260806-434-github-actions-release-ci.md) and its
> [work log](../work-logs/20260806-434-github-actions-release-ci.md).

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
