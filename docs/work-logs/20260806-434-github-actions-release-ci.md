# Task 434 작업 로그 — 태그 CI 구축, 그리고 계획의 전제 하나가 틀렸습니다

설계: [20260806-434](../design/20260806-434-github-actions-release-ci.md) ·
작업 지시: [20260806-434](../work-orders/20260806-434-github-actions-release-ci.md) ·
가이드: [릴리스 절차와 CI](../guides/release-and-ci.md)

## 1. 한 줄 결과

태그 push에 대한 빌드 검증·샘플 테스트·아티팩트 발행 워크플로를 만들었습니다.
**구현 전 로컬 실측이 계획의 전제 하나를 반증**했고(§3), 사용자 지시로 샘플 스위트가
Debug에서 **Release로 바뀌면서** 빌드가 하나 줄었습니다(§4). **GitHub 실행은 아직
없습니다** — 태그 push는 사용자가 수행합니다.

## 2. 만든 것

| 파일 | 내용 | 성격 |
|---|---|---|
| `.github/workflows/ci.yml` | `main` push·PR에서 Debug 빌드 + probe 2종 | 신규 |
| `.github/workflows/release.yml` | `v*` 태그에서 버전 게이트 → Release 빌드 → probe → 819샘플 → 아티팩트 → Release 첨부 | 신규 |
| `scripts/package_release.ps1` | 아티팩트 2종 생성. 워크플로와 로컬이 같은 절차를 씀 | 신규 |
| `scripts/test_openwatcom_samples.ps1` | `-Configuration` 추가, `Configuration` 기록·경고, `RunCriterion` 기록 위치 결함 수정 | 변경 |
| `scripts/build_openwatcom_samples.ps1` | `-Configuration` 추가, 호스트 빌드를 `.bat` 대신 `.ps1` 직접 호출 | 변경 |
| `ARCHITECTURE.md` · `docs/guides/release-and-ci.md` | CI 구조와 반복 절차 | 신규/갱신 |

두 스크립트의 `-Configuration` **기본값은 Debug**이므로 인자 없이 부르던 기존 절차의
동작은 바뀌지 않습니다.

## 3. 정정 — `repiu_aot_probe`를 CI에서 전부 돌릴 수 없습니다

**설계 초안이 틀렸습니다.** "probe 2종은 저작물 없이 실행 가능"이라고 적었으나, 구현
전에 실제로 돌려 보니 `repiu_aot_probe`는 `argv[1]`로 DOS4GW 실행 파일을 받습니다
([`aot_probe/main.cpp:612`](../../src/tools/aot_probe/main.cpp)).

| 입력 | exit | 출력 |
|---|---:|---|
| `--timer-safe-point` | **0** | `timer_safe_point_probe=true` |
| `samples/dos4gw_hello/build/hello.exe` | **1** | `cache_message=direct control-flow target is outside the cache` |
| `clibexam__atouni_c` 등 OpenWatcom 샘플 3종 | **1** | 동일 |
| (인자 없음) | 2 | usage |

전체 단정 묶음(arena view, coherence, plan build benchmark, timer tick 단정 10개)은
`PIU.EXE`를 전제로 합니다. **CI에는 `--timer-safe-point`만 넣었습니다.**

`repiu_glide_issue_probe`는 인자 없이 exit 0 · `glide_issue_probe=pass`로 확인됐습니다.

**따라서 CI의 회귀 감시 가치는 사실상 전적으로 819샘플 스위트에 있습니다.** probe 두
개는 링크·초기화가 깨지지 않았음을 보는 얕은 확인입니다. 이 한계를 설계 §4, 가이드 §3,
`ARCHITECTURE.md`에 모두 명시했습니다.

## 4. 변경 — 샘플 스위트를 Release로 (사용자 지시)

초안은 Debug와 Release를 **둘 다** 빌드했습니다. 하네스의 `$Loader`가 Debug 경로로
하드코딩돼 있었기 때문입니다. `-Configuration`을 추가해 **Release 하나만** 빌드합니다.

| | 변경 전 | 변경 후 |
|---|---|---|
| release.yml 빌드 | Debug + Release | **Release만** |
| 샘플이 쓰는 로더 | Debug | **Release — 배포물과 동일** |

**배포하는 바이너리를 그대로 검증**하게 되고 빌드가 하나 줄었습니다.

**대가와 그 처리:** 통과 판정은 timeout을 실패로 세는데(Task 429), Debug는 plan build에서
Release 대비 11.34배 느립니다(Task 330). 구성이 다르면 timeout 경계의 샘플 판정이
달라지므로, Task 429가 `RunCriterion`에 쓴 것과 **같은 가드 패턴**을 구성에도 적용해
summary·baseline에 `Configuration`을 기록하고 `-CompareBaseline`이 다르면 경고합니다.

## 5. 함께 고친 결함 — Task 429의 기록 위치

Task 429는 baseline에 `RunCriterion`을 기록하려 했으나 실제로는 `Summary` 안에만
들어가고 **최상위에는 없었습니다.** 비교 코드는 최상위만 읽으므로, 새 기준으로
재기록해도 계속 `(none - predates Task 429)`로 경고했을 것입니다. 두 위치를 모두 읽도록
고치고 기록도 두 위치에 남깁니다.

**이것을 만난 경위:** 구성 경고를 같은 패턴으로 붙이려고 기존 경고 코드를 읽다가
발견했습니다. 패턴을 복제하려면 원본을 읽어야 하고, 읽으면 이런 것이 보입니다.

## 6. 검증

| # | 확인 | 방법 | 결과 |
|---:|---|---|---|
| 1 | 두 스크립트 파싱·파라미터 바인딩 | `Get-Command -Syntax` | **통과** — `-Configuration`이 첫 위치 파라미터로 노출 |
| 2 | `package_release.ps1` 실동작 | 실제 Release 트리로 실행 | **통과** — zip 2개 생성(3,408,451 / 4,573,030 바이트) |
| 3 | 아카이브 내용 | ZipFile 목록 | **통과** — 실행 파일 6종 + 문서 3종 / 리포트 3종 |
| 4 | probe 2종 | 직접 실행 | **통과** — §3 표 |
| 5 | 워크플로 YAML 문법 | **미검증** | 로컬에 YAML 파서(python·node) 없음. 탭 0개·스텝 키 구조는 육안 확인 |
| 6 | GitHub에서의 실행 | **미검증** | 태그 push 전. 첫 실행 결과는 별도 로그 |

**5번과 6번을 미검증으로 남깁니다.** 특히 총 실행 시간, 캐시 적중, `windows-2022`에서의
생성기 해석은 첫 실행에서만 확인됩니다.

## 6.1 하네스에 시간 상한이 없었습니다 (사용자 지적 → 수정)

Release 스위트를 로컬로 돌리던 중 진행이 멈췄고, 사용자가 hang을 의심했습니다.
**맞았습니다.**

```text
[744/819] test cplbexam iostream\istream\get.cpp
Id 3908  StartTime 13:51:58  CPU 1.015625  RunMin 39.4
Threads 1  MainWindowHandle 0  ThreadState Wait
```

**39.4분 동안 CPU 1.0초** — 스핀이 아니라 블록입니다. 원인 축은 하네스에 있었습니다.
`Invoke-Capture`가 `& $FilePath @Arguments`로 로더를 부르며 **시간 상한이 없고 콘솔
stdin을 상속**했고, 이 샘플은 `istream::get`으로 표준 입력을 읽습니다.

**로더의 1,000 ms timeout으로는 못 막습니다** — 그것은 게스트 실행 예산이고, 호스트
쪽에서 블록되면 그 경로에 도달하지 못합니다. CI였다면 job 한도 240분을 통째로 쓰고
아티팩트도 리포트도 없이 실패했을 것입니다.

사용자 지시대로 **샘플당 최대 10초, timeout은 실패**로 구현했습니다.

| 축 | 조치 |
|---|---|
| 시간 상한 | `-SampleTimeoutSeconds` 기본 10. 초과 시 kill |
| 판정 | `RunPassed`에 `-not $run.TimedOut` 항 추가. 상태는 `fail (harness timeout)` |
| 입력 | stdin을 빈 임시 파일로 redirect |
| 기준 ID | `...+no-harness-timeout`으로 상향(Task 429 규칙) |

**구현 중 결함 두 개를 실측으로 잡았습니다.**

1. **`Start-Process -PassThru`는 `Handle`을 먼저 읽지 않으면 `ExitCode`가 `$null`입니다.**
   첫 시험에서 정상 샘플 `_atouni.c`까지 fail이 나와 발견했습니다. 고치지 않았다면
   **819개 전부 실패**했을 것입니다.
2. **로더 출력은 전부 stderr입니다**(stdout 0바이트, stderr 37,434바이트). 두 스트림을
   모두 파일로 받아 합칩니다.

**검증(2샘플 표적 실행):**

| 샘플 | 결과 |
|---|---|
| `_atouni.c` | **pass** — 정상 샘플의 판정이 바뀌지 않음 |
| `cplbexam\iostream\istream\get.cpp` | **fail (harness timeout)** — 10초에 kill |

**미확정 하나:** `get.cpp`는 stdin redirect만 적용한 중간 시험에서는 10초를 쓰지 않고
빨리 끝났는데, ExitCode 수정 후 실행에서는 10초를 소진했습니다. 블록 지점이 stdin
읽기인지 다른 것인지는 **확정하지 못했습니다.** 두 겹으로 막았고 상한이 있으므로 CI
관점에서는 닫혔지만, 근본 원인은 별도 조사 대상입니다.

## 7. Release 구성 실측 (진행 중)

`-Configuration Release`로 819샘플을 로컬 실행해 Debug 기준선과의 차이를 재고 있습니다.
이 결과가 **첫 CI 실행에서 볼 회귀 목록과 같은 성질**이므로, 사용자가 baseline 재기록을
판단할 근거가 됩니다. 완료 후 이 절을 수치로 채웁니다.

## 8. 사용자 결정으로 남긴 것

* **baseline 재기록 시점.** 사용자가 경로 (나)를 택했습니다 — 먼저 재기록하지 않고,
  첫 태그 실행이 실패하면 그 리포트로 위양성을 확인한 뒤 재기록합니다. 가이드 §4에
  절차를 적었습니다.
* **첫 태그 push.** AGENTS.md대로 원격 push는 사용자가 직접 수행합니다.

## 9. 회고

* **계획서를 쓸 때 실행해 보지 않은 것이 오류였습니다.** "probe는 저작물 없이 된다"는
  코드를 읽고 추정한 것이었고, 구현 직전에 돌려 보니 틀렸습니다. **승인받은 계획의
  전제가 틀렸다면 구현보다 정정이 먼저**이므로 설계·지시를 먼저 고치고 진행했습니다.
* **가드 패턴을 복제하려고 원본을 읽은 것이 결함 하나를 잡았습니다.**
* **기본값을 바꾸지 않는 확장을 택했습니다.** `-Configuration` 기본값을 Debug로 두어
  기존 절차·기준선의 의미를 보존했고, 새 동작은 CI가 명시적으로 요청합니다.

---

# Task 434 Work Log — the tag CI is built, and one planning premise was wrong

## 1. Result in one line

Workflows now verify a tag, run the sample suite, and publish artifacts. **Local measurement
before implementation refuted one premise of the approved plan** (§3), and on the user's
instruction the sample suite moved from Debug to **Release**, removing a build (§4). **Nothing
has run on GitHub yet** — the tag push is the user's step.

## 2. What was built

Two new workflows (`ci.yml` for pushes and pull requests, `release.yml` for `v*` tags), a new
`scripts/package_release.ps1` so the workflow and a local run share one packaging procedure, a
`-Configuration` parameter on both OpenWatcom scripts, the configuration guard and a fix to
Task 429's recording position in the test harness, and the CI section in `ARCHITECTURE.md` with
a repeatable procedure in `docs/guides/release-and-ci.md`. **Both `-Configuration` parameters
default to Debug**, so existing invocations behave exactly as before.

## 3. Correction — `repiu_aot_probe` cannot run in full under CI

**The design draft was wrong.** It recorded both probes as runnable without copyrighted assets;
running them before implementation showed that `repiu_aot_probe` takes a DOS4GW executable as
`argv[1]` ([`aot_probe/main.cpp:612`](../../src/tools/aot_probe/main.cpp)). Measured:
`--timer-safe-point` exits 0 with `timer_safe_point_probe=true`; `hello.exe` and three
OpenWatcom sample executables all exit 1 at `cache_message=direct control-flow target is
outside the cache`; no argument exits 2 with usage. The full assertion set presumes `PIU.EXE`,
so **only `--timer-safe-point` went into CI**. `repiu_glide_issue_probe` was confirmed at exit 0
with `glide_issue_probe=pass`.

**CI's regression value therefore rests almost entirely on the 819-sample suite**, with the two
probes a shallow check that linking and initialisation are intact. That limit is stated in
design §4, guide §3 and `ARCHITECTURE.md`.

## 4. Change — the sample suite runs on Release, per user instruction

The draft built both configurations because the harness's `$Loader` was hard-coded to the Debug
path. With `-Configuration` added, **only Release is built**: the shipped binary is now the
tested one, and a whole build disappears from the run.

**The cost, and its handling:** the pass criterion counts a timeout as a failure (Task 429) and
Debug is 11.34x slower than Release at plan building (Task 330), so samples near the timeout
boundary can be judged differently between configurations. The **same guard pattern** Task 429
applied to `RunCriterion` is applied to the configuration — recorded in the summary and the
baseline, warned about by `-CompareBaseline`.

## 5. A defect fixed along the way

Task 429 meant to record `RunCriterion` in the baseline, but it only landed inside `Summary`
while the comparison reads the top level, so a baseline re-recorded under the new rule would
still have warned `(none - predates Task 429)`. Both positions are now read and written. It was
found by reading the existing warning in order to copy its pattern.

## 6. Verification

Both scripts parse and bind `-Configuration` (`Get-Command -Syntax`); `package_release.ps1`
runs against the real Release tree and produces both archives (3,408,451 and 4,573,030 bytes)
with the expected six executables, three documents and three report files; and the two probes
behave as tabulated in §3. **Two items are unverified**: the workflow YAML has no local parser
(no python or node available — only a tab-free, structurally eyeballed check was possible), and
nothing has executed on GitHub, so total wall time, cache hits and generator resolution on
`windows-2022` remain unknown until the first run.

## 6.1 The harness had no time bound (user's catch, fixed)

The local Release run stopped progressing and the user suspected a hang. **They were right**:
`cplbexam\iostream\istream\get.cpp` had the loader blocked for **39.4 minutes on 1.0 second of
CPU** — one thread, in `Wait`, no window. `Invoke-Capture` called the loader as
`& $FilePath @Arguments`, with **no time bound and the console's stdin inherited**, and that
sample reads standard input through `istream::get`. The loader's own 1,000 ms budget cannot
catch it, being a guest-execution budget. In CI this would have burned the whole 240-minute job
limit and produced neither artifact nor report.

Implemented as instructed — **10 seconds per sample, a timeout counts as a failure**: the
process is killed, `-not $run.TimedOut` joins `RunPassed` as its own term, the status reads
`fail (harness timeout)`, stdin is redirected from an empty file, and `RunCriterionId` was
bumped to `...+no-harness-timeout` per Task 429's rule.

**Two defects were caught by measurement while implementing this.** `Start-Process -PassThru`
leaves `ExitCode` at `$null` unless `Handle` is read first — found because the healthy sample
`_atouni.c` also came back as a failure, and left unfixed it would have failed **all 819**. And
the loader writes everything to stderr (stdout 0 bytes against stderr 37,434), so both streams
are captured and concatenated.

**Verified on a two-sample run:** `_atouni.c` **passes**, so healthy verdicts are unchanged, and
`get.cpp` reports **`fail (harness timeout)`** after being killed at ten seconds.

**One item is unresolved.** With only the stdin redirect in place, `get.cpp` finished quickly;
after the `ExitCode` fix it consumed the full ten seconds. Whether the block is the standard
input read or something else is **not settled**. Both defences are in place and the bound holds,
so the CI risk is closed, but the root cause is a separate investigation.

## 7. Release-configuration measurement (in progress)

The 819 samples are being run locally under `-Configuration Release` to measure the difference
against the Debug-recorded baseline. That result has the same character as the regression list
the first CI run will produce, so it is the evidence the user's re-recording decision rests on.
This section is filled in with figures once it completes.

## 8. Left to the user

The **baseline re-recording point**: path (나) was chosen — do not re-record first, let the
first tag run fail, and use its report to review the false positives before re-recording, per
guide §4. And the **first tag push**, since AGENTS.md keeps remote pushes with the user.

## 9. Retrospective

**Writing the plan without running anything was the error.** "The probes need no assets" was
inferred from reading code and turned out to be false the moment it was executed. When an
approved plan's premise is wrong, **correcting the plan comes before implementing it**, so the
design and work order were fixed first.

**Copying a guard pattern meant reading the original, and reading it caught a defect.**

**The extension keeps the old default.** `-Configuration` defaults to Debug so existing
procedures and the recorded baseline keep their meaning; the new behaviour is what CI asks for
explicitly.
