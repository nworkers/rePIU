# OpenWatcom sample baseline refresh 작업 지시

## 목표

현재 Win32 loader 구현 기준으로 OpenWatcom 샘플을 다시 빌드하고 실행한 뒤, baseline과 history를 최신 결과로 갱신한다.

## 범위

* OpenWatcom 샘플 manifest를 다시 생성한다.
* 생성된 manifest의 샘플을 loader로 실행한다.
* `tests/baselines/openwatcom_samples.json`을 갱신한다.
* `tests/history/openwatcom_samples/`에 새 milestone JSON/HTML을 남긴다.
* 결과 요약과 회귀 여부를 작업 로그에 기록한다.

## 검증

* `powershell -ExecutionPolicy Bypass -File scripts\build_openwatcom_samples.ps1`
* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -UpdateBaseline`

# OpenWatcom Sample Baseline Refresh Work Order

## Goal

Rebuild and rerun the OpenWatcom samples against the current Win32 loader implementation, then refresh the baseline and history.

## Scope

* Regenerate the OpenWatcom sample manifest.
* Run manifest samples through the loader.
* Update `tests/baselines/openwatcom_samples.json`.
* Add new milestone JSON/HTML files under `tests/history/openwatcom_samples/`.
* Record the summary and regression status in the work log.

## Verification

* `powershell -ExecutionPolicy Bypass -File scripts\build_openwatcom_samples.ps1`
* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -UpdateBaseline`
