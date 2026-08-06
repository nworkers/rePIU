# Task 435 작업 로그 — 실행 기본값을 `dynamic` · 무제한으로

설계: [20260807-435](../design/20260807-435-execution-default-policy.md) ·
작업 지시: [20260807-435](../work-orders/20260807-435-execution-default-policy.md)

## 1. 한 일

| 항목 | 이전 | 이후 |
|---|---|---|
| `REPIU_EXECUTION_BACKEND` 미설정 | `legacy` | **`dynamic`** |
| `REPIU_EXECUTION_TIMEOUT_MS` 미설정 | 1000 ms | **0 = 무제한** |

기본값 결정을 host 익명 namespace에서 runtime으로 옮기고 probe로 고정했습니다.
`src/host/win32/main.cpp`에는 `0` → Win32 `INFINITE` 매핑만 남았습니다.

* `include/repiu/runtime/execution_backend.h` · `.cpp`: `kDefaultExecutionBackend`,
  `ResolveExecutionBackend`
* `include/repiu/runtime/execution_timeout.h` · `.cpp`(신규):
  `kUnlimitedExecutionTimeoutMilliseconds`, `kDefaultExecutionTimeoutMilliseconds`,
  `ResolveExecutionTimeoutMilliseconds`
* `execution_backend_probe.cpp`에 기본값 단정 추가, `execution_timeout_probe.cpp` 신규
* `scripts/test_all.ps1` · `scripts/test_openwatcom_samples.ps1`: 실행 정책 고정(§3)
* README·ARCHITECTURE·`docs/guides/gameplay-scene-capture.md`·`docs/guides/release-and-ci.md`
  ·`docs/analysis/aot-execution-backend.md`·`docs/analysis/current-execution-frontier.md`

## 2. 구현 중 확인한 것 — 무제한은 1초 무진행 감시도 함께 끕니다

`PollThreadUntilExit`(`live_telemetry_snapshot.cpp:474`, `:488`)는 **wall-clock 예산과
1초 무진행 판정을 같은 `INFINITE` 스위치**로 껐다 켭니다. 따라서 기본값이 무제한이
되면서 두 감시가 함께 꺼졌습니다.

이것은 부작용이 아니라 frontier 항목 **1''** 이 요구하던 상태입니다. 그 항목은 정상
대기 중인 게스트를 무진행 감시가 죽여 **프레임 기반 측정이 전부 왜곡된다**고 적고,
우회로 `REPIU_EXECUTION_TIMEOUT_MS=0`을 지목하고 있었습니다. 그 우회가 기본값이
됐습니다. **감시 자체는 고치지 않았으므로** 예산을 명시하는 실행에서는 그대로
오판합니다 — frontier 1''을 "완화"로 갱신하고 남은 수정을 명시했습니다.

## 3. 회귀 harness 두 곳이 기본값에 의존하고 있었습니다

기본값이 실제로 쓰이던 유일한 곳입니다. 둘 다 **baseline이 기록된 값을 스스로
고정**하도록 바꿨습니다(매개변수 `-Backend legacy`, `-GuestTimeoutMilliseconds 1000`).

| harness | 그대로 뒀다면 |
|---|---|
| `test_all.ps1` | `piu_1st`가 끝나지 않아 30초 harness kill → **회귀 테스트 실패**. 통과 조건 하나가 `minimal execution attempt timed out`이었습니다 |
| `test_openwatcom_samples.ps1` | 멈춘 샘플이 로더 예산(≈1초) 대신 harness kill(10초)로 끝나 **판정 근거와 스위트 시간이 함께 변함**. baseline은 `legacy`에서 기록된 것 |

제품 기본값을 따라가게 두면 코드 변경 없이 `tests/baselines`의 의미가 바뀝니다.
Task 429·434가 pass criterion과 build configuration에 세워 둔 것과 같은 종류의 방벽을
실행 정책에도 세운 것입니다.

## 4. 검증 (2026-08-07, Win32 x86 Debug)

| 검증 | 결과 |
|---|---|
| Debug 빌드 | **exit 0** (신규 warning 없음, C4819는 기존 한글 주석 파일과 동일) |
| `repiu_aot_probe.exe MASTER\PIU_1ST\PIU\PIU.EXE` | **exit 0**, `execution_backend_policy=true`, `execution_timeout_policy=true` |
| 미설정 스모크(`piu_1st`) | `Win32 requested execution backend: dynamic`, `Win32 guest execution timeout: disabled`. **9초 뒤에도 실행 중**이어서 강제 종료 — 스스로 멈추지 않는 것이 의도한 동작 |
| 고정 스모크(`legacy` + `1000`) | **1.8초에 exit 0**, `Win32 guest execution timeout: 1000 ms`, `minimal execution attempt timed out` — 이전 동작 그대로 |
| 두 harness 스크립트 | PowerShell 파서 검사 통과 |

**돌리지 않은 것:** `test_all.ps1` 전체 실행(호스트 재빌드와 OpenWatcom `dos4gw_hello`
빌드를 포함)과 OpenWatcom 819 샘플 스위트. 두 harness가 의존하던 로더 동작은 위
고정 스모크로 직접 확인했고, 스크립트 변경은 환경 변수 고정과 복원뿐입니다.

## 5. 남은 것

1. **무진행 감시 수정**(frontier 1''): 진행 판정에 HLE 활동을 포함시키는 일은 그대로
   남았습니다. 예산을 명시하는 측정 절차와 회귀 harness는 여전히 영향을 받습니다.
2. **`dynamic` 기준 OpenWatcom baseline**: 제품 기본값과 스위트 기준이 갈렸습니다.
   재기록은 사람의 판단이므로 지시가 있을 때 수행합니다.
3. `main.cpp:85`의 `[repiu-live-debug] env REPIU_EXECUTION_TIMEOUT_MS` 상시 출력은
   이번 범위 밖이라 그대로 뒀습니다.

---

# Task 435 Work Log — defaulting to `dynamic` with no time limit

## 1. What changed

Unset `REPIU_EXECUTION_BACKEND` now resolves to **`dynamic`** rather than `legacy`, and unset
`REPIU_EXECUTION_TIMEOUT_MS` to **`0`, meaning unlimited**, rather than 1000 ms. The decision
moved out of an anonymous namespace in `src/host/win32/main.cpp` into the runtime layer under
probe coverage — `kDefaultExecutionBackend` and `ResolveExecutionBackend` in
`execution_backend.{h,cpp}`, and a new `execution_timeout.{h,cpp}` — leaving the host with only
the `0` → Win32 `INFINITE` mapping. The backend probe gained a default assertion, a new timeout
probe covers its four branches, both regression harnesses pin their execution policy, and the
README, `ARCHITECTURE.md`, the gameplay capture and release/CI guides, and two analysis topics
were corrected.

## 2. Found while implementing — unlimited also disables the one-second stall watchdog

`PollThreadUntilExit` gates both the wall-clock budget and the one-second no-progress verdict on
the same `INFINITE` switch (`live_telemetry_snapshot.cpp:474` and `:488`), so both went off with
the new default. That is not a side effect but the state frontier item **1''** was asking for:
it recorded the watchdog killing a guest that was legitimately waiting on timer ticks, called
that a distortion of every frame-based measurement, and named `REPIU_EXECUTION_TIMEOUT_MS=0` as
the workaround. The workaround is now the default. **The watchdog itself is unfixed** and still
misjudges any run that states a budget, so the frontier item is marked *mitigated* with the
remaining repair named.

## 3. Two regression harnesses depended on the defaults

They were the only consumers of the defaults, and both now pin the values their baselines were
recorded under, as overridable parameters. Left alone, `test_all.ps1` would never see `piu_1st`
return — one of its accepted outcomes is the guest exhausting the 1000 ms budget — and would
fail on its own thirty-second kill, while `test_openwatcom_samples.ps1` would move stalled
samples from a ~1 second loader timeout to a 10 second harness kill, changing both the verdict
basis and the suite's running time against a baseline recorded on `legacy`. Following the
product default would have changed what `tests/baselines` means with no code change behind it:
this is the same guard Tasks 429 and 434 put around the pass criterion and the build
configuration, extended to the execution policy.

## 4. Verification (2026-08-07, Win32 x86 Debug)

The Debug build passes with no new warnings; `repiu_aot_probe.exe` against
`MASTER\PIU_1ST\PIU\PIU.EXE` exits 0 reporting `execution_backend_policy=true` and
`execution_timeout_policy=true`; a smoke with the environment cleared logs
`Win32 requested execution backend: dynamic` and `Win32 guest execution timeout: disabled` and
was **still running after nine seconds**, which is the intended behaviour, so it was killed; a
pinned `legacy` plus `1000` smoke exits 0 after 1.8 seconds with
`minimal execution attempt timed out`, exactly as before; and both harness scripts pass a
PowerShell parse check.

**Not run:** the full `test_all.ps1` (it rebuilds the host and the OpenWatcom `dos4gw_hello`
sample) and the 819-sample OpenWatcom suite. The loader behaviour those harnesses depend on was
confirmed directly by the pinned smoke, and the script changes are confined to setting and
restoring two environment variables.

## 5. Left open

Fixing the no-progress watchdog itself (frontier 1'') remains, and still affects any procedure
that states a budget. The OpenWatcom baseline is now pinned to `legacy` while the product
default is `dynamic`; re-recording it on `dynamic` is a human judgement and awaits a decision.
The unconditional `[repiu-live-debug]` print of the timeout variable at `main.cpp:85` was left
in place as out of scope.
