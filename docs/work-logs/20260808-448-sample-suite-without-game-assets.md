# Task 448 작업 로그 — 원본 자산 없이 샘플 스위트 빌드, 그리고 릴리스 노트

작업지시: [20260808-448](../work-orders/20260808-448-sample-suite-without-game-assets.md) ·
선행: [447 OpenWatcom 핀](20260808-447-pin-openwatcom-to-dated-snapshot.md)

## 1. 근인 — 쓰지도 않는 자산을 요구하고 있었습니다

Task 447이 설치 실패를 걷어내자 그 뒤 스텝이 처음으로 실행됐고, 거기서 걸렸습니다.
`setup_test_environment.ps1:129`가 `MASTER\PIU_1ST\PIU\PIU.EXE`를 필수로 봅니다. 그
트리는 원본 게임 자산이라 저장소에 없고, **클린 체크아웃은 원리적으로 만족시킬 수
없습니다.**

**그리고 이 호출자에게는 애초에 필요 없는 요구입니다.** 확인한 사실:

| 스크립트 | `MASTER` 참조 | `roms`/CHD 참조 |
|---|---|---|
| `build_openwatcom_samples.ps1` | 없음 | 없음 |
| `test_openwatcom_samples.ps1` | 없음 | 없음 |
| `package_release.ps1` | 없음 | 없음 |
| `build_dos4gw_hello.bat` | 없음 | 없음 |

샘플 스위트는 wcl386으로 자기 DOS 프로그램을 만들어 로더로 돌립니다. 자산 의존은
**오직 `setup_test_environment.ps1` 경유 한 곳**이었습니다.

## 2. 변경

`-SkipGameAssets` 스위치를 두고 `build_openwatcom_samples.ps1`이 넘깁니다. 자산이 없으면
throw 대신 경고하고, 툴체인 점검과 OpenWatcom 자가 치유 설치는 그대로 돕니다.
`piu_1st` 호출자는 끈 채로 둡니다 — 그쪽은 자산이 실제로 필요합니다. 워크플로가
`-SkipSetup`을 일부러 안 넘기는 이유(차가운 캐시 자가 치유)를 건드리지 않은 채,
쓰지 않는 요구만 없앴습니다.

## 3. 검증 — 자산이 없는 클린 루트에서 두 분기

스크립트만 빈 디렉터리에 복사해 `$Root`에 `MASTER`가 없는 상태를 만들었습니다.
**사용자 자산 트리는 건드리지 않았습니다.**

| 실행 | 결과 |
|---|---|
| 스위치 없음 | `Original executable was not found at MASTER\...` — **CI와 동일하게 실패** |
| `-SkipGameAssets` | `[warn] Original PIU executable is absent; skipping the game asset check.` 후 **다음 점검으로 진행** |

두 번째 실행은 OpenWatcom 점검에서 멈추는데, 이는 그 격리 루트에 툴체인이 없고 제가
`-SkipOpenWatcomInstall`을 강제했기 때문입니다. CI에서는 앞선 `Install OpenWatcom`
스텝이 이미 깔아 두므로 `[ok]` 분기를 탑니다.

## 4. 남은 CI 스텝 미리 감사

한 번에 하나씩 드러나는 상황이라 태그를 또 태우기 전에 훑었습니다.

| 스텝 | 클린 체크아웃에서 가능한가 | 근거 |
|---|---|---|
| Probe ×2 | **이미 통과함** | 이번 실패보다 앞 스텝이고 초록이었습니다 |
| Build OpenWatcom samples | 예 | 이번 수정 |
| Test samples `-CompareBaseline` | 예 | baseline이 저장소에 있음(`tests/baselines/openwatcom_samples.json`) |
| Package | 예 | 입력이 `VERSION`·`README.md`·`THIRD_PARTY_NOTICES.md`(전부 추적됨)와 앞 스텝 산출물 |
| Attach to release | 예 | 러너에 `gh` 존재, 토큰은 `github.token` |

**정직하게 말하면:** 마지막 네 스텝은 **CI에서 한 번도 실행된 적이 없습니다.** 입력이
전부 추적된다는 것까지가 제가 확인한 것이고, 실행 자체는 아직 증거가 없습니다. 다만
샘플 빌드+테스트 경로는 Task 447에서 **로컬로 끝까지 돌렸습니다**(819/819 빌드,
회귀 0).

## 5. 릴리스 노트

워크플로가 `--generate-notes`로 커밋 제목만 나열하고 있었습니다. 이제
`docs/release-notes/<tag>.md`가 있으면 그것을 쓰고(없으면 기존 동작), 이미 존재하는
릴리스에는 `gh release edit --notes-file`로 얹습니다.
[v0.0.143 노트](../release-notes/v0.0.143.md)를 v0.0.136 이후 구간으로 작성했습니다.

## 6. 버전이 0.0.143인 이유

`origin/main`이 이미 `f05a487`이므로 **v0.0.142는 push되어 CI가 돌았고 실패했습니다.**
푸시된 태그를 옮기는 대신 새 patch 버전으로 나갑니다. 릴리스 노트는 v0.0.136 이후
전부를 담으므로 사용자가 보는 내용은 달라지지 않습니다.

---

# Task 448 Work Log — building the sample suite without the game assets

## 1. Root cause

Clearing the install failure in Task 447 let the next step run for the first time, and it
stopped at `setup_test_environment.ps1:129`, which requires
`MASTER\PIU_1ST\PIU\PIU.EXE` — original game data that is not in the repository, so a clean
checkout cannot satisfy it in principle. The requirement also does not belong to this
caller: neither `build_openwatcom_samples.ps1`, `test_openwatcom_samples.ps1`,
`package_release.ps1` nor `build_dos4gw_hello.bat` references `MASTER`, `roms` or a CHD.
The suite compiles its own DOS programs with wcl386 and runs them through the loader; the
asset dependency existed at exactly one place, through this setup script.

## 2. The change

A `-SkipGameAssets` switch demotes the missing tree to a warning while keeping the
toolchain checks and the self-healing OpenWatcom install, and
`build_openwatcom_samples.ps1` passes it. `piu_1st` callers leave it off, because they do
need the assets. The workflow's reason for not passing `-SkipSetup` is untouched.

## 3. Verification in a clean root

Copying the script alone into an empty directory made `$Root` a tree with no `MASTER`,
**without touching the user's asset tree**. Without the switch it failed exactly as CI did;
with it, `[warn] Original PIU executable is absent` and execution continued. That second
run then stops at the OpenWatcom check only because the isolated root has no toolchain and
`-SkipOpenWatcomInstall` was forced; in CI the earlier install step has already run, so it
takes the `[ok]` branch.

## 4. Auditing the rest of the pipeline

Since the failures are surfacing one at a time, the remaining steps were checked before
spending another tag. The two probes already passed — they run before this failure and were
green. The sample build is fixed here. The baseline comparison has its baseline in the
repository at `tests/baselines/openwatcom_samples.json`. Packaging takes `VERSION`,
`README.md` and `THIRD_PARTY_NOTICES.md`, all tracked, plus artifacts produced earlier in
the run. Attaching the release needs `gh` and `github.token`, both present on the runner.

**Stated plainly: the last four steps have never executed on CI.** What is verified is that
their inputs are all tracked; their execution is not yet evidenced. The sample build and
test path, however, was run locally end to end during Task 447 — 819 of 819 built, zero
regressions.

## 5. Release notes

The workflow called `--generate-notes`, which lists commit subjects. It now prefers
`docs/release-notes/<tag>.md` when present, falls back to generated notes otherwise, and
applies the file to an existing release with `gh release edit --notes-file`. The
[v0.0.143 notes](../release-notes/v0.0.143.md) cover everything since v0.0.136.

## 6. Why the version is 0.0.143

`origin/main` is already at `f05a487`, so **v0.0.142 was pushed, ran and failed**. Rather
than move a pushed tag, this ships as the next patch version. The notes cover everything
since v0.0.136, so nothing is lost from the reader's point of view.
