# OpenWatcom 히스토리 HTML 리포트 작업 지시

## 목표

OpenWatcom baseline history에 JSON뿐 아니라 같은 실행의 HTML 테스트 리포트도 함께 남긴다.

## 작업 범위

1. `tests` 디렉터리가 Git 추적 대상인지 확인한다.
2. `.gitignore`가 `tests`를 제외하지 않는지 확인한다.
3. `scripts/test_openwatcom_samples.ps1 -UpdateBaseline`이 history JSON과 같은 basename의 HTML report를 생성하도록 수정한다.
4. `ARCHITECTURE.md`에 history HTML report 정책을 반영한다.
5. 설계 문서와 작업 로그를 남긴다.
6. baseline update를 실행해 JSON/HTML 생성과 수치를 확인한다.
7. 변경을 커밋한다.

## 비목표

* OpenWatcom 샘플 소스나 EXE를 Git에 추가하지 않는다.
* 매번 생성되는 `build/openwatcom_sample_report/` 파일은 계속 Git 제외 경로로 둔다.
* 기존 과거 history JSON마다 HTML을 역생성하지 않는다.

## 검증

* `git ls-files tests`
* `git check-ignore tests tests/baselines/openwatcom_samples.json tests/history/openwatcom_samples`
* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -UpdateBaseline`

# OpenWatcom History HTML Report Work Order

## Goal

Store the HTML test report alongside the JSON whenever the OpenWatcom baseline history is updated.

## Scope

1. Confirm that the `tests` directory is tracked by Git.
2. Confirm that `.gitignore` does not exclude `tests`.
3. Update `scripts/test_openwatcom_samples.ps1 -UpdateBaseline` to generate an HTML report with the same basename as the history JSON.
4. Reflect the history HTML report policy in `ARCHITECTURE.md`.
5. Leave a design document and work log.
6. Run a baseline update and confirm JSON/HTML generation plus summary numbers.
7. Commit the changes.

## Non-Goals

* Do not add OpenWatcom sample sources or EXEs to Git.
* Keep per-run `build/openwatcom_sample_report/` files under the Git-excluded build path.
* Do not retroactively generate HTML for every older history JSON.

## Verification

* `git ls-files tests`
* `git check-ignore tests tests/baselines/openwatcom_samples.json tests/history/openwatcom_samples`
* `powershell -ExecutionPolicy Bypass -File scripts\test_openwatcom_samples.ps1 -UpdateBaseline`
