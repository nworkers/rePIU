# OpenWatcom 히스토리 HTML 리포트 작업 로그

## 요약

`tests` 디렉터리가 Git에 포함되어 있는지 확인했고, baseline history 갱신 시 JSON과 같은 basename의 HTML 테스트 리포트도 함께 저장하도록 변경했다.

## 확인 결과

* `git ls-files tests`에서 baseline과 history JSON들이 출력되어 `tests`가 Git 추적 대상임을 확인했다.
* `git check-ignore tests tests\baselines\openwatcom_samples.json tests\history\openwatcom_samples`는 출력이 없어서 `.gitignore`가 `tests`를 제외하지 않음을 확인했다.

## 변경 내용

* `scripts/test_openwatcom_samples.ps1 -UpdateBaseline`이 history JSON 생성 후 같은 basename의 HTML report도 `tests/history/openwatcom_samples/`에 복사한다.
* `ARCHITECTURE.md`에 baseline update가 날짜/버전별 JSON과 HTML report를 함께 남긴다는 내용을 반영했다.
* 작업 단위 설계 문서와 작업 지시 문서를 추가했다.
* 새 baseline history 파일 쌍을 생성했다.

## 검증

실행:

* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -UpdateBaseline`

결과:

* 새 history JSON: `tests/history/openwatcom_samples/20260710-000038-0.0.5.json`
* 새 history HTML: `tests/history/openwatcom_samples/20260710-000038-0.0.5.html`
* 전체 샘플: 819개
* 빌드 성공: 793개
* 빌드 skip: 26개
* 실행 pass: 473개
* 전체 pass: 473개

# OpenWatcom History HTML Report Work Log

## Summary

Confirmed that the `tests` directory is included in Git, and changed baseline history updates to store an HTML test report with the same basename as the JSON history file.

## Checks

* `git ls-files tests` printed baseline and history JSON files, confirming that `tests` is tracked.
* `git check-ignore tests tests\baselines\openwatcom_samples.json tests\history\openwatcom_samples` printed nothing, confirming that `.gitignore` does not exclude `tests`.

## Changes

* `scripts/test_openwatcom_samples.ps1 -UpdateBaseline` now copies the HTML report to `tests/history/openwatcom_samples/` after writing the history JSON, using the same basename.
* Updated `ARCHITECTURE.md` to state that baseline updates leave dated/versioned JSON and HTML reports together.
* Added the task design and work-order documents.
* Generated a new baseline history file pair.

## Verification

Command:

* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -UpdateBaseline`

Results:

* New history JSON: `tests/history/openwatcom_samples/20260710-000038-0.0.5.json`
* New history HTML: `tests/history/openwatcom_samples/20260710-000038-0.0.5.html`
* Total samples: 819
* Build passed: 793
* Build skipped: 26
* Run passed: 473
* Overall passed: 473
