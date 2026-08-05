# Task 428 작업 로그 — OpenWatcom 샘플 baseline을 v0.0.133으로 갱신

지시: [20260805-428](../work-orders/20260805-428-openwatcom-baseline-refresh.md)

## 1. 한 일

`scripts\test_openwatcom_samples.ps1 -UpdateBaseline`을 **legacy 기본 경로**로 수행해
baseline을 v0.0.59에서 v0.0.133으로 올렸습니다.

| 항목 | 이전 (0.0.59) | 이후 (0.0.133) |
|---|---:|---:|
| GeneratedAt | 2026-07-18 12:08:30 | 2026-08-05 19:17:18 |
| Total | 819 | 819 |
| BuildPassed / BuildSkipped | 793 / 26 | 793 / 26 |
| RunEligible | 793 | 793 |
| **RunPassed** | **529** | **535** |
| RunPassRate | 66.7% | 67.5% |
| OverallPassRate | 64.6% | 65.3% |

갱신 직전 비교에서 **회귀 0건, 신규 통과 6건**이었으므로 안전한 갱신입니다. 신규
통과 6건은 `b_keybrd.c`, `chainint.c`, `getdate.c`, `gettime.c`, `getvect.c`,
`setvect.c`로, 인터럽트·날짜·시각 계열입니다. 이번 backend 정리(Tasks 424~427)가
아니라 그 사이 74개 버전 동안의 다른 작업이 만든 개선입니다.

history 파일 두 개가 함께 생성됐습니다.

* `tests/history/openwatcom_samples/20260805-191718-0.0.133.json`
* `tests/history/openwatcom_samples/20260805-191718-0.0.133.html`

## 2. legacy 기준을 택한 이유

하네스는 `REPIU_EXECUTION_BACKEND`를 설정하지 않고 로더를 호출하므로 **기본 실행
경로가 legacy**입니다. `dynamic` 기준(574개)으로 기록하면 하네스를 평소대로 돌릴
때마다 39건이 회귀로 보고됩니다. 기록 기준과 실행 기본값을 일치시켰습니다.

`dynamic`이 legacy보다 39개 더 통과하고 **잃는 샘플이 0개**라는 사실은 이 갱신으로
사라지지 않습니다. 아래 §4에 남깁니다.

## 3. 검증

| # | 확인 | 결과 |
|---:|---|---|
| 1 | baseline `Version` | `0.0.133` |
| 2 | baseline `GitCommit` | `78366f8…` — HEAD와 일치 |
| 3 | baseline `RunPassed` | **535** (dynamic의 574가 아님) |
| 4 | history 파일 | JSON·HTML 2개 생성 |
| 5 | 갱신 후 `-CompareBaseline` | exit 0, **회귀 0 / 신규 통과 0** |

5번의 "신규 통과 0"이 기준선이 제자리에 놓였다는 증거입니다. 갱신 전에는 6이었습니다.

**Debug 로더는 이 작업 직전에 재빌드했습니다.** 하네스가 쓰는
`build/win32_x86_debug/Debug/repiu_loader_win32.exe`가 7월 31일자였으므로, 그대로
돌렸다면 옛 코드의 결과가 baseline으로 기록될 뻔했습니다.

## 4. 함께 기록 — backend별 샘플 통과와 비용

baseline 갱신과 별개로, 같은 스위트를 두 backend로 돌려 얻은 결과입니다.

| 항목 | legacy | dynamic |
|---|---:|---:|
| RunPassed | 535 | **574** |
| RunPassRate | 67.5% | 72.4% |
| 0.0.59 대비 회귀 | 0 | 0 |
| legacy 대비 손실 | — | **0** |

`dynamic`은 39개를 더 통과하고 잃는 샘플이 없습니다.

**다만 이 워크로드에서 `dynamic`은 더 느립니다.** 양쪽 다 통과하는 샘플 10개의
프로세스 wall-clock 중앙값 차이가 **+281 ms**였습니다.

| 샘플 | legacy | dynamic |
|---|---:|---:|
| `_atouni.c` | 131 ms | 170 ms |
| `_disable.c` | 94 ms | 203 ms |
| `_exit.c` | 124 ms | 405 ms |
| `_freect.c` | 110 ms | 626 ms |
| `_harderr.c` | 123 ms | 430 ms |

원인은 구조적입니다. AOT plan → emit → place 비용은 **프로세스당 고정**인데 이
샘플들은 100 ms대에 끝나므로 상각할 실행 시간이 없습니다. 실제 게임은 수 분 단위로
실행되어 같은 고정비를 프레임 수천 개에 나눠 냅니다.

**스위트 전체 wall clock(legacy 385초 대 dynamic 523초)은 성능 비교로 쓸 수
없습니다.** 두 실행이 서로 다른 양의 일을 했기 때문입니다 — `dynamic`은 39개
프로그램을 더 끝까지 실행했고, legacy가 그 샘플들에서 빨랐던 것은 속도가 아니라
일찍 실패했기 때문입니다. 역방향 사례로 `asctime.c`는 legacy에서 4,062 ms 걸려
실패하고 `dynamic`에서 671 ms에 통과합니다.

**결론:** OpenWatcom 샘플은 `dynamic`의 성능 이점을 보일 수 있는 워크로드가
아닙니다. 정확성 비교에는 유효하고, 성능은 PIU 기반 벤치마크로 재야 합니다.

---

# Task 428 Work Log — refreshing the OpenWatcom sample baseline to v0.0.133

Work order: [20260805-428](../work-orders/20260805-428-openwatcom-baseline-refresh.md).

## 1-2. What was done and why legacy

Ran `test_openwatcom_samples.ps1 -UpdateBaseline` on the **legacy** default path, moving the
baseline from v0.0.59 (2026-07-18) to v0.0.133. Build figures are unchanged at 793 passed and
26 skipped of 819; `RunPassed` moves 529 → 535, `RunPassRate` 66.7% → 67.5%, and
`OverallPassRate` 64.6% → 65.3%. The pre-refresh comparison showed **zero regressions and six
new passes** (`b_keybrd.c`, `chainint.c`, `getdate.c`, `gettime.c`, `getvect.c`, `setvect.c` —
interrupt, date, and time samples), which came from the 74 versions of other work between the
two baselines, not from the backend consolidation in Tasks 424-427. Two history files were
written alongside.

Legacy was chosen because the harness invokes the loader without setting
`REPIU_EXECUTION_BACKEND`, making legacy its default path; a `dynamic` baseline of 574 would
report 39 regressions on every ordinary run. That `dynamic` passes 39 more samples while losing
none is preserved in §4.

## 3. Verification

The written baseline reads `Version` 0.0.133 and `GitCommit` `78366f8…` matching HEAD, with
`RunPassed` **535** — the legacy figure, not `dynamic`'s 574 — and both history files present.
A follow-up `-CompareBaseline` run exited 0 with **zero regressions and zero new passes**; that
second zero, down from six, is the evidence the gate is now seated on current behaviour.

**The Debug loader was rebuilt immediately before this task.** The harness uses
`build/win32_x86_debug/Debug/repiu_loader_win32.exe`, which was dated July 31; running as-is
would have recorded results for old code as the baseline.

## 4. Recorded alongside — per-backend passes and cost

Running the same suite under both backends: `dynamic` passes 574 against legacy's 535, a 72.4%
run-pass rate against 67.5%, with zero regressions against the old baseline and **zero samples
lost** relative to legacy.

**On this workload `dynamic` is nevertheless slower.** Across ten samples that pass under both,
the median per-process wall-clock difference is **+281 ms** (for example `_freect.c` at 110 ms
against 626 ms, `_exit.c` at 124 ms against 405 ms). The cause is structural: the AOT
plan/emit/place cost is fixed per process while these programs finish in roughly 100 ms, so
there is no execution time to amortize it over — unlike the game, which runs for minutes and
spreads the same fixed cost across thousands of frames.

**The suite-level wall clock (legacy 385 s against dynamic 523 s) is not a valid performance
comparison**, because the two runs did different amounts of work: `dynamic` ran 39 more
programs to completion, and legacy's shorter time on those reflects failing early rather than
running fast. The reverse case exists too — `asctime.c` takes 4,062 ms to fail under legacy and
671 ms to pass under `dynamic`.

**Conclusion:** the OpenWatcom samples cannot demonstrate `dynamic`'s performance advantage.
They remain valid for correctness comparison; performance belongs to the PIU-based benchmarks.
