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

## 2026-07-11 추가 갱신

`implement-int21-ah3f-read` 브랜치의 `6a531eb53c158cc2a41203688aab386aabfb1757` 기준으로 OpenWatcom 샘플 baseline을 다시 갱신했다. `scripts\test_openwatcom_samples.ps1 -CompareBaseline`에서 기존 `0.0.9` baseline 대비 regression이 없고 신규 pass가 49개임을 확인한 뒤, `-UpdateBaseline`으로 baseline과 history milestone을 갱신했다.

### 결과 요약

* GeneratedAt: `2026-07-11 04:15:08`
* Version: `0.0.15`
* GitCommit: `6a531eb53c158cc2a41203688aab386aabfb1757`
* GitBranch: `implement-int21-ah3f-read`
* Total: `819`
* BuildPassed: `793`
* BuildSkipped: `26`
* RunEligible: `793`
* RunPassed: `522`
* OverallPassed: `522`
* BuildPassRate: `96.8`
* RunPassRate: `65.8`
* OverallPassRate: `63.7`

### baseline 비교

* BaselineVersion: `0.0.9`
* BaselineGeneratedAt: `2026-07-10 14:54:53`
* NewPassCount: `49`
* RegressionCount: `0`
* UnchangedFailCount: `297`
* NewSampleCount: `0`
* MissingSampleCount: `0`

### 갱신 파일

* `tests/baselines/openwatcom_samples.json`
* `tests/history/openwatcom_samples/20260711-041509-0.0.15.json`
* `tests/history/openwatcom_samples/20260711-041509-0.0.15.html`

### 검증

* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -CompareBaseline`
  * 결과: 성공, regression `0`
* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -UpdateBaseline`
  * 결과: 성공
* `powershell -ExecutionPolicy Bypass -File scripts\build_openwatcom_samples.ps1`
  * 결과: 실패
  * 사유: 이전 OpenWatcom 샘플 실행에서 남은 `repiu_loader_win32` 프로세스가 `build\win32_x86_debug\Debug\repiu_loader_win32.exe`를 잠그고 있어 `LNK1168`이 발생했다. 현재 권한 모델에서는 해당 프로세스 종료도 거부되었다.

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

## 2026-07-11 Additional Refresh

Refreshed the OpenWatcom sample baseline on branch `implement-int21-ah3f-read` at `6a531eb53c158cc2a41203688aab386aabfb1757`. First, `scripts\test_openwatcom_samples.ps1 -CompareBaseline` confirmed zero regressions and 49 new passes against the previous `0.0.9` baseline. Then `-UpdateBaseline` refreshed the tracked baseline and added a new history milestone.

### Result Summary

* GeneratedAt: `2026-07-11 04:15:08`
* Version: `0.0.15`
* GitCommit: `6a531eb53c158cc2a41203688aab386aabfb1757`
* GitBranch: `implement-int21-ah3f-read`
* Total: `819`
* BuildPassed: `793`
* BuildSkipped: `26`
* RunEligible: `793`
* RunPassed: `522`
* OverallPassed: `522`
* BuildPassRate: `96.8`
* RunPassRate: `65.8`
* OverallPassRate: `63.7`

### Baseline Comparison

* BaselineVersion: `0.0.9`
* BaselineGeneratedAt: `2026-07-10 14:54:53`
* NewPassCount: `49`
* RegressionCount: `0`
* UnchangedFailCount: `297`
* NewSampleCount: `0`
* MissingSampleCount: `0`

### Updated Files

* `tests/baselines/openwatcom_samples.json`
* `tests/history/openwatcom_samples/20260711-041509-0.0.15.json`
* `tests/history/openwatcom_samples/20260711-041509-0.0.15.html`

### Verification

* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -CompareBaseline`
  * Result: passed, regression `0`
* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -UpdateBaseline`
  * Result: passed
* `powershell -ExecutionPolicy Bypass -File scripts\build_openwatcom_samples.ps1`
  * Result: failed
  * Reason: stale `repiu_loader_win32` processes from previous OpenWatcom sample runs kept `build\win32_x86_debug\Debug\repiu_loader_win32.exe` locked, causing `LNK1168`. The current permission model also denied terminating those processes.
