# OpenWatcom 샘플 빌드 실패 분류 작업 로그

## 요약

OpenWatcom 로컬 샘플 빌드에서 기존 `build_fail`로 기록되던 31개를 재분류했다.
옵션 누락 5개는 실제 빌드되도록 고쳤고, DOS/4GW console 대상과 맞지 않는 26개는 실패가 아니라 명시적 skip으로 기록하게 했다.

## 변경 내용

* `scripts/build_openwatcom_samples.ps1`에 샘플별 build plan을 추가했다.
* `clibexam/setnew.c`에는 `-cc++`를 적용해 C++로 컴파일한다.
* `cplbexam/contain/wcldintr.cpp`, `wcldptr.cpp`, `wcldval.cpp`, `cplbexam/ios/except.cpp`에는 `-xs`를 적용해 C++ 예외 처리를 켠다.
* DOS/4GW flat 32-bit console 대상과 맞지 않는 26개 샘플은 `BuildStatus = "skip"`으로 기록한다.
* manifest에 `BuildSkipped`, `BuildSkipReason`, `BuildOptions`를 추가했다.
* `scripts/test_openwatcom_samples.ps1`가 build skip 상태를 summary, baseline sample, HTML report에 반영하도록 갱신했다.
* 로더 실행 파일이 잠겨 있을 때 샘플 build plan만 검증할 수 있도록 `scripts/build_openwatcom_samples.ps1`에 `-SkipHostBuild` 옵션을 추가했다.
* `ARCHITECTURE.md`와 작업 단위 설계/지시 문서에 build plan과 skip 정책을 반영했다.

## 검증

실행:

* `powershell -ExecutionPolicy Bypass -File scripts\build_openwatcom_samples.ps1 -SkipSetup -SkipHostBuild`
* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -CompareBaseline`

결과:

* 전체 샘플: 819개
* 빌드 성공: 793개
* 빌드 skip: 26개
* 빌드 실패: 0개
* 실행 대상: 793개
* 실행 pass: 473개
* 전체 pass: 473개
* baseline new pass: 3개
* baseline regression: 0개
* missing sample: 0개

## 참고

일반 `scripts\build_openwatcom_samples.ps1 -SkipSetup`는 Win32 host rebuild 단계에서 `repiu_loader_win32.exe` 파일 잠금으로 실패했다.
확인 시 `repiu_loader_win32` 프로세스가 남아 있었지만, 현재 세션 권한으로는 `Stop-Process`와 `taskkill /F` 모두 접근 거부로 종료하지 못했다.
그래서 이번 검증은 새로 추가한 `-SkipHostBuild`로 기존 loader를 유지한 채 샘플 빌드 계획과 loader baseline 비교를 확인했다.

# OpenWatcom Sample Build Failure Classification Work Log

## Summary

Reclassified the 31 OpenWatcom local sample build failures.
Five samples that only needed build options now build, and 26 samples that do not fit the DOS/4GW console target are recorded as explicit skips instead of failures.

## Changes

* Added a per-sample build plan to `scripts/build_openwatcom_samples.ps1`.
* Applied `-cc++` to `clibexam/setnew.c` so it compiles as C++.
* Applied `-xs` to `cplbexam/contain/wcldintr.cpp`, `wcldptr.cpp`, `wcldval.cpp`, and `cplbexam/ios/except.cpp` to enable C++ exception handling.
* Recorded 26 samples that do not fit the DOS/4GW flat 32-bit console target with `BuildStatus = "skip"`.
* Added `BuildSkipped`, `BuildSkipReason`, and `BuildOptions` to the manifest.
* Updated `scripts/test_openwatcom_samples.ps1` to reflect build skips in the summary, baseline sample records, and HTML report.
* Added `-SkipHostBuild` to `scripts/build_openwatcom_samples.ps1` so the sample build plan can be verified when the loader executable is locked.
* Reflected the build plan and skip policy in `ARCHITECTURE.md` and the task design/work-order documents.

## Verification

Commands:

* `powershell -ExecutionPolicy Bypass -File scripts\build_openwatcom_samples.ps1 -SkipSetup -SkipHostBuild`
* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -CompareBaseline`

Results:

* Total samples: 819
* Build passed: 793
* Build skipped: 26
* Build failed: 0
* Run eligible: 793
* Run passed: 473
* Overall passed: 473
* Baseline new pass: 3
* Baseline regressions: 0
* Missing samples: 0

## Note

Plain `scripts\build_openwatcom_samples.ps1 -SkipSetup` failed during the Win32 host rebuild because `repiu_loader_win32.exe` was locked.
`repiu_loader_win32` processes were present, but both `Stop-Process` and `taskkill /F` failed with access denied under the current session.
The verification therefore used the new `-SkipHostBuild` option to keep the existing loader while validating the sample build plan and loader baseline comparison.
