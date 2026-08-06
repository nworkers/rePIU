# Task 434 작업 지시 — GitHub Actions 릴리스 CI 구축

설계: [20260806-434](../design/20260806-434-github-actions-release-ci.md)

## 0. 선행 조건 (이 Task 착수 전, 사용자 수행)

**샘플 baseline을 새 판정 기준으로 재기록해야 합니다.** 설계 §10 참조. 이것이
끝나기 전에는 `release.yml`을 켜지 마십시오 — 첫 실행이 측정 정정을 회귀로 보고하며
실패합니다.

```powershell
scripts\build_openwatcom_samples.ps1
scripts\test_openwatcom_samples.ps1 -UpdateBaseline
```

수행 후 `tests/baselines/openwatcom_samples.json`의 `RunCriterion`이
`exit0+no-exception+returned+no-timeout`인지 확인하고 커밋합니다. 이때 나온
`RunPassed` 값이 CI의 기준선이 됩니다.

**이 단계를 건너뛰고 진행할 수도 있습니다.** 그 경우 첫 태그 실행이 실패하고, 리포트
아티팩트를 받아 위양성 목록을 확인한 뒤 로컬에서 재기록하는 순서가 됩니다. 어느 쪽을
택할지는 사용자 판단입니다.

## 1. 범위

```mermaid
flowchart LR
    S0["단계 0<br/>baseline 재기록<br/>(사용자)"] --> S1["단계 1<br/>ci.yml"]
    S1 --> S2["단계 2<br/>release.yml<br/>게이트·빌드·probe"]
    S2 --> S3["단계 3<br/>샘플 스위트 연결"]
    S3 --> S4["단계 4<br/>패키징 + Release 첨부"]
    S4 --> S5["단계 5<br/>문서 갱신"]
```

**범위 안:** `.github/workflows/` 두 파일, 패키징 스크립트 1개, 문서 갱신.

**범위 밖:** 기존 빌드·테스트 스크립트의 동작 변경, sccache 도입, baseline 값 변경,
`REPIU_*` 환경 변수 기본값 변경.

## 2. 단계별 작업

### 단계 1 — `.github/workflows/ci.yml`

| 항목 | 내용 |
|---|---|
| 트리거 | `push: branches [main]`, `pull_request` |
| 러너 | `windows-2022` (설계 §9) |
| 스텝 | checkout → `_deps` 캐시 복원 → `scripts\build_win32_x86.ps1 -Configuration Debug` → `repiu_aot_probe` → `repiu_glide_issue_probe` |
| 실패 조건 | 빌드 실패, probe exit ≠ 0 |

샘플 스위트는 **넣지 않습니다.** 매 push마다 819개를 돌리면 시간과 과금이 감당되지
않습니다(설계 §11).

### 단계 2 — `.github/workflows/release.yml` 골격

| 항목 | 내용 |
|---|---|
| 트리거 | `push: tags ['v*']`, `workflow_dispatch` |
| 러너 | `windows-2022` |
| 권한 | `contents: write` (Release 첨부에 필요) |

스텝 순서:

1. **버전 게이트.** `github.ref_name`에서 앞의 `v`를 제거해 `VERSION` 파일 내용과
   비교하고, 불일치면 두 값을 출력하고 exit 1.
   `workflow_dispatch`로 실행된 경우에는 태그가 없으므로 게이트를 건너뛰고 `VERSION`
   값을 그대로 사용합니다.
2. 캐시 복원 2종 (설계 §8).
3. `scripts\build_win32_x86.ps1 -Configuration Debug`
4. `scripts\build_win32_x86.ps1 -Configuration Release`
5. `repiu_aot_probe`, `repiu_glide_issue_probe` (Release 산출물로 실행)

**3과 4는 같은 트리를 씁니다.** 멀티컨피그이므로 configure는 첫 호출에서 한 번만
일어나고, 두 번째는 `--config`만 다릅니다.

### 단계 3 — 샘플 스위트 연결

```powershell
scripts\install_openwatcom.ps1
scripts\build_openwatcom_samples.ps1 -SkipHostBuild
scripts\test_openwatcom_samples.ps1 -CompareBaseline
```

* `-SkipHostBuild`를 쓰는 이유: 단계 2에서 이미 Debug 로더를 만들었으므로
  `build_win32_x86.bat`를 다시 부를 필요가 없습니다.
* `-SkipSetup`은 **쓰지 않습니다.** `setup_test_environment.ps1`이 OpenWatcom 설치를
  포함하는데, 캐시가 살아 있으면 `install_openwatcom.ps1`이 "already installed"로
  즉시 반환하므로 비용이 없습니다. 캐시가 비었을 때 스스로 복구하는 경로를
  남겨 둡니다.
* `-CompareBaseline`은 회귀 발생 시 예외를 던져 job을 실패시킵니다
  ([`test_openwatcom_samples.ps1:553`](../../scripts/test_openwatcom_samples.ps1)).

**리포트 업로드 스텝에는 `if: always()`를 붙입니다.** 회귀로 실패한 실행일수록 리포트가
필요합니다(설계 §6).

### 단계 4 — 패키징과 Release 첨부

`scripts/package_release.ps1`을 새로 만듭니다. 워크플로 YAML에 긴 복사 로직을 인라인으로
넣지 않고 스크립트로 분리해 **로컬에서도 같은 절차로 패키지를 만들 수 있게** 합니다.

| 파라미터 | 기본값 |
|---|---|
| `-Configuration` | `Release` |
| `-Version` | `VERSION` 파일에서 읽음 |
| `-OutputDir` | `build\package` |

산출물 두 개(설계 §6):

* `build\package\rePIU-v<version>-win32.zip`
* `build\package\openwatcom-samples-v<version>.zip`

첨부는 `gh release create`(또는 `softprops/action-gh-release`)로 하며, 태그가 이미
존재하므로 **release 생성 시 기존 태그를 재사용**합니다. 릴리스 노트는 자동 생성에
맡기고 이번 범위에서 본문을 작성하지 않습니다.

### 단계 5 — 문서 갱신

| 문서 | 갱신 내용 |
|---|---|
| `ARCHITECTURE.md` | CI 절 추가 — 워크플로 두 개의 역할과 검증 범위 경계 |
| `docs/guides/` | 릴리스 절차 가이드 신규 — 태그 push부터 아티팩트 확인까지, baseline 재기록 시점 포함 |
| `README.md` | 빌드 상태 배지(선택) |

## 3. 검증

| # | 확인 | 방법 | 기대 |
|---:|---|---|---|
| 1 | 워크플로 문법 | `act` 또는 push 후 Actions 로그 | 파싱 오류 없음 |
| 2 | 버전 게이트 (불일치) | `workflow_dispatch` 없이, `VERSION`과 다른 태그로 시험 | exit 1, 두 값이 로그에 출력 |
| 3 | 버전 게이트 (일치) | 실제 태그 | 통과 |
| 4 | Debug/Release 산출물 | 로그의 빌드 경로 | 두 구성 모두 `repiu_loader_win32.exe` 생성 |
| 5 | probe | exit code | `repiu_aot_probe` exit 0, `repiu_glide_issue_probe` pass |
| 6 | 샘플 스위트 | summary.json | `Total` 819, `RunCriterion` 일치, 회귀 0 |
| 7 | 아티팩트 | Release 페이지 | zip 2개 첨부, 이름의 버전이 태그와 일치 |
| 8 | 실패 시 리포트 | 회귀를 의도적으로 만든 실행 | job 실패 + 리포트 아티팩트 존재 |
| 9 | 캐시 | 2회차 실행 시간 | `_deps` clone과 OpenWatcom 다운로드가 생략됨 |
| 10 | 총 실행 시간 | Actions 로그 | **기록만** — 판정 기준 없음, 작업 로그에 남김 |

**8번은 baseline을 건드리지 않고 확인합니다.** 존재하지 않는 baseline 경로를
`-BaselinePath`로 주면 스크립트가 예외를 던지므로, 그것으로 "실패해도 리포트가
올라오는가"를 볼 수 있습니다.

## 4. 완료 기준

1. 태그 push 한 번으로 Release에 zip 2개가 첨부된다.
2. 회귀가 있으면 job이 실패하고, 그때도 샘플 리포트를 받을 수 있다.
3. `VERSION`과 태그가 어긋나면 빌드 전에 멈춘다.
4. 위 검증표 10항목의 결과가 작업 로그에 기록된다 — 특히 10번 실행 시간과, 설계 §11의
   미확정 항목(`repiu_aot_probe` 헤드리스 동작)의 판정.

## 5. 영향 범위와 되돌리기

**기존 코드는 건드리지 않습니다.** 새로 추가하는 것은 `.github/workflows/` 두 파일과
`scripts/package_release.ps1`뿐이며, 기존 빌드·테스트 스크립트는 호출만 합니다. 문제가
생기면 워크플로 파일 삭제로 완전히 되돌아갑니다.

`VERSION`은 변경하지 않습니다. 이 작업은 문서와 CI 설정이므로 머지 시 patch 증가 여부는
사용자 지시를 따릅니다.

---

# Task 434 Work Order — build the GitHub Actions release CI

Design: [20260806-434](../design/20260806-434-github-actions-release-ci.md)

## 0. Prerequisite (before this task, performed by the user)

**The sample baseline must be re-recorded under the new pass criterion** — see design §10. Do
not enable `release.yml` before this, or the first run fails by reporting measurement
corrections as regressions.

```powershell
scripts\build_openwatcom_samples.ps1
scripts\test_openwatcom_samples.ps1 -UpdateBaseline
```

Afterwards confirm that `RunCriterion` in `tests/baselines/openwatcom_samples.json` reads
`exit0+no-exception+returned+no-timeout` and commit it; the resulting `RunPassed` becomes CI's
bar.

**Proceeding without this step is also valid**: the first tag run then fails, and the report
artifact is used to review the false positives before re-recording locally. The choice is the
user's.

## 1. Scope

**In scope:** two files under `.github/workflows/`, one packaging script, documentation
updates. **Out of scope:** changing the behaviour of existing build or test scripts,
introducing sccache, changing baseline values, changing any `REPIU_*` default.

## 2. Steps

### Step 1 — `.github/workflows/ci.yml`

Triggered by pushes to `main` and by pull requests, on `windows-2022` (design §9): checkout,
restore the `_deps` cache, `scripts\build_win32_x86.ps1 -Configuration Debug`, then
`repiu_aot_probe` and `repiu_glide_issue_probe`. It fails on a build failure or a non-zero
probe exit. **The sample suite is not included** — running 819 samples on every push is
affordable in neither time nor billing (design §11).

### Step 2 — `.github/workflows/release.yml` skeleton

Triggered by `v*` tags and `workflow_dispatch`, on `windows-2022`, with `contents: write` for
the release upload. The steps are: the **version gate** (strip the leading `v` from
`github.ref_name`, compare against `VERSION`, print both values and exit 1 on mismatch; skip
the comparison for `workflow_dispatch`, which has no tag, and take `VERSION` as given); the two
cache restores; `build_win32_x86.ps1 -Configuration Debug`; the same for `Release`; and the two
probes run from the Release output. **Both builds share one tree** — the generator is
multi-config, so configure happens once and only `--config` differs.

### Step 3 — wiring the sample suite

```powershell
scripts\install_openwatcom.ps1
scripts\build_openwatcom_samples.ps1 -SkipHostBuild
scripts\test_openwatcom_samples.ps1 -CompareBaseline
```

`-SkipHostBuild` is used because step 2 already produced the Debug loader. `-SkipSetup` is
**not** used: `setup_test_environment.ps1` includes the OpenWatcom install, which returns
immediately as "already installed" when the cache is warm, so it costs nothing while leaving a
self-healing path for a cold cache. `-CompareBaseline` throws on regressions and fails the job
([`test_openwatcom_samples.ps1:553`](../../scripts/test_openwatcom_samples.ps1)). **The report
upload step carries `if: always()`**, because a run that failed on regressions is exactly when
the report is wanted (design §6).

### Step 4 — packaging and release upload

Add `scripts/package_release.ps1` rather than inlining copy logic into YAML, so **the same
procedure produces the package locally**. Parameters: `-Configuration` (default `Release`),
`-Version` (read from `VERSION`), `-OutputDir` (default `build\package`). It emits the two
archives from design §6. Upload via `gh release create` or `softprops/action-gh-release`,
**reusing the existing tag**; release notes are left to autogeneration in this scope.

### Step 5 — documentation

`ARCHITECTURE.md` gains a CI section covering both workflows and the verification boundary; a
new guide under `docs/guides/` documents the release procedure from tag push to artifact check,
including when the baseline is re-recorded; a build badge in `README.md` is optional.

## 3. Verification

Ten checks: workflow syntax parses; the version gate fails on mismatch printing both values and
passes on a real tag; both configurations produce `repiu_loader_win32.exe`; `repiu_aot_probe`
exits 0 and `repiu_glide_issue_probe` passes; `summary.json` shows 819 total with a matching
`RunCriterion` and zero regressions; the release page carries both archives with the tag's
version in their names; a deliberately failing run still uploads the report; the second run
skips the `_deps` clone and the OpenWatcom download; and total wall time is **recorded only**,
with no pass criterion, into the work log.

**Check eight is done without touching the baseline**: passing a non-existent path through
`-BaselinePath` makes the script throw, which exercises "does the report still upload when the
job fails".

## 4. Completion criteria

One tag push attaches both archives to the release; a regression fails the job while the sample
report remains retrievable; a `VERSION`/tag mismatch stops the run before the build; and the
ten verification results are recorded in the work log — in particular the wall time and the
verdict on design §11's open item about `repiu_aot_probe` running headless.

## 5. Impact and rollback

**No existing code is touched.** The additions are two workflow files and
`scripts/package_release.ps1`, which only invoke the existing build and test scripts. Deleting
the workflow files reverts everything. `VERSION` is not changed here; whether the merge bumps
the patch version follows the user's instruction.
