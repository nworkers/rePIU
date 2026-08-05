# Task 428 작업 지시 — OpenWatcom 샘플 baseline을 v0.0.133으로 갱신

선행 측정: [Task 427 작업 로그](../work-logs/20260805-427-unreferenced-execution-entry-points.md)
이후 수행한 샘플 스위트 실행

## 1. 왜 갱신하는가

기록된 baseline이 **v0.0.59(2026-07-18)** 로 74개 버전 뒤처져 있습니다. 그 사이 통과
샘플이 늘어 회귀 감지 기준으로서의 민감도가 떨어졌습니다.

| 항목 | baseline 0.0.59 | v0.0.133 legacy |
|---|---:|---:|
| RunPassed | 529 | 535 |
| RunPassRate | 66.7% | 67.5% |
| 회귀 | — | **0** |
| 신규 통과 | — | 6 |

회귀 0건이므로 갱신은 안전합니다. 지금 baseline을 올려두면 이후 실행이 **535개를
기준선으로** 회귀를 잡습니다.

## 2. 왜 legacy 기준인가

하네스는 `REPIU_EXECUTION_BACKEND`를 설정하지 않고 로더를 호출하므로 **기본 실행
경로가 legacy**입니다. baseline을 `dynamic` 기준(574개)으로 잡으면 하네스를 평소대로
돌릴 때마다 39건이 회귀로 보고됩니다. 기록 기준과 실행 기본값을 일치시킵니다.

`dynamic`이 39개를 더 통과한다는 사실 자체는
[Task 427 이후 측정](#)에 남아 있으며, 이 갱신으로 사라지지 않습니다.

## 3. 절차

```powershell
# 환경 변수를 반드시 비웁니다. dynamic이 남아 있으면 잘못된 기준이 기록됩니다.
[Environment]::SetEnvironmentVariable("REPIU_EXECUTION_BACKEND", $null)
scripts\test_openwatcom_samples.ps1 -UpdateBaseline
```

`-UpdateBaseline`은 `-CompareBaseline`과 병용할 수 없고, 방금 수행한 실행 결과로
다음 셋을 씁니다.

* `tests/baselines/openwatcom_samples.json`
* `tests/history/openwatcom_samples/<타임스탬프>-0.0.133.json`
* `tests/history/openwatcom_samples/<타임스탬프>-0.0.133.html`

**Debug 로더가 최신이어야 합니다.** 하네스는
`build/win32_x86_debug/Debug/repiu_loader_win32.exe`를 쓰며, 이 경로가 오래된
바이너리면 옛 코드의 결과가 기록됩니다.

## 4. 검증

| # | 확인 | 통과 조건 |
|---:|---|---|
| 1 | baseline의 `Version` | `0.0.133` |
| 2 | baseline의 `GitCommit` | 현재 HEAD와 일치 |
| 3 | baseline의 `RunPassed` | **535** (dynamic의 574가 아님) |
| 4 | history 파일 | JSON과 HTML 두 개가 생성됨 |
| 5 | 갱신 후 재비교 | `-CompareBaseline` 실행이 회귀 0으로 통과 |

3번이 핵심입니다. 574가 기록됐다면 환경 변수가 남아 있었다는 뜻이므로 되돌리고
다시 수행합니다.

## 5. 완료 기준

1. §4의 다섯 항목이 통과했습니다.
2. 갱신된 baseline과 history 파일이 커밋됐습니다.
3. 작업 로그에 이전/이후 수치와 legacy 기준 선택 근거를 남겼습니다.

---

# Task 428 Work Order — refresh the OpenWatcom sample baseline to v0.0.133

## 1-2. Why, and why legacy

The recorded baseline is **v0.0.59 (2026-07-18)**, 74 versions behind, and its sensitivity as a
regression gate has decayed as more samples began passing: v0.0.133 under legacy runs 535
against the baseline's 529, with **zero regressions** and six new passes. Refreshing is
therefore safe and makes later runs gate on 535.

The baseline is taken under **legacy** because the harness invokes the loader without setting
`REPIU_EXECUTION_BACKEND`, so legacy is its default path. A `dynamic` baseline of 574 would
report 39 regressions every time the harness is run normally. That `dynamic` passes 39 more
samples remains recorded in the Task 427 follow-up measurement and is not lost by this refresh.

## 3. Procedure

Clear `REPIU_EXECUTION_BACKEND` explicitly, then run
`scripts\test_openwatcom_samples.ps1 -UpdateBaseline`, which cannot be combined with
`-CompareBaseline` and writes the baseline plus a timestamped history JSON and HTML from the
run it just performed. **The Debug loader must be current** — the harness uses
`build/win32_x86_debug/Debug/repiu_loader_win32.exe`, and a stale binary there records results
for old code.

## 4-5. Verification and completion

The baseline must read `Version` `0.0.133`, a `GitCommit` matching HEAD, and `RunPassed`
**535** — not the `dynamic` figure of 574, which would mean the environment variable was still
set and requires redoing the run — with both history files created and a follow-up
`-CompareBaseline` run passing with zero regressions. Done when those five checks hold, the
baseline and history files are committed, and the work log records the before/after figures
and the reason for choosing legacy.
