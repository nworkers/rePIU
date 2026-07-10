# OpenWatcom sample baseline refresh 작업 로그

## 실행 내용

현재 Win32 loader 구현 기준으로 OpenWatcom 샘플 manifest를 다시 생성하고 전체 샘플을 실행했다. 실행 결과로 baseline과 history milestone을 갱신했다.

## 결과 요약

* GeneratedAt: `2026-07-10 14:54:53`
* Version: `0.0.9`
* GitCommit: `9ff6963daf59bd27e961bfa27e167e49783f30bd`
* GitBranch: `feature-timeout-execution-observation`
* Total: `819`
* BuildPassed: `793`
* BuildSkipped: `26`
* RunEligible: `793`
* RunPassed: `473`
* OverallPassed: `473`
* BuildPassRate: `96.8`
* RunPassRate: `59.6`
* OverallPassRate: `57.8`

## 갱신 파일

* `tests/baselines/openwatcom_samples.json`
* `tests/history/openwatcom_samples/20260710-145454-0.0.9.json`
* `tests/history/openwatcom_samples/20260710-145454-0.0.9.html`

## 검증

* `powershell -ExecutionPolicy Bypass -File scripts\build_openwatcom_samples.ps1`
  * 결과: 성공
* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -UpdateBaseline`
  * 결과: 성공

# OpenWatcom Sample Baseline Refresh Work Log

## Execution

Regenerated the OpenWatcom sample manifest and ran the full sample set against the current Win32 loader implementation. The run refreshed the baseline and added a new history milestone.

## Result Summary

* GeneratedAt: `2026-07-10 14:54:53`
* Version: `0.0.9`
* GitCommit: `9ff6963daf59bd27e961bfa27e167e49783f30bd`
* GitBranch: `feature-timeout-execution-observation`
* Total: `819`
* BuildPassed: `793`
* BuildSkipped: `26`
* RunEligible: `793`
* RunPassed: `473`
* OverallPassed: `473`
* BuildPassRate: `96.8`
* RunPassRate: `59.6`
* OverallPassRate: `57.8`

## Updated Files

* `tests/baselines/openwatcom_samples.json`
* `tests/history/openwatcom_samples/20260710-145454-0.0.9.json`
* `tests/history/openwatcom_samples/20260710-145454-0.0.9.html`

## Verification

* `powershell -ExecutionPolicy Bypass -File scripts\build_openwatcom_samples.ps1`
  * Result: passed
* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -UpdateBaseline`
  * Result: passed
