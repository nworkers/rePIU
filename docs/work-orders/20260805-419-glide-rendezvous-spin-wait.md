# Task 419 작업 지시 — Glide rendezvous 스핀 대기

설계: [20260805-419](../design/20260805-419-glide-rendezvous-spin-wait.md)

## 1. 구현

`glide_opengl_backend.cpp` / `glide_opengl_backend.h` 두 파일입니다.

1. **원자 미러 추가** — `std::atomic<bool> host_command_pending_atomic_`,
   `host_command_complete_atomic_`. 기존 `bool`과 **같은 지점에서 함께 갱신**하되,
   기존 뮤텍스 규약과 조건변수 술어는 **바꾸지 않습니다.**
2. **스핀 헬퍼** — 예산(µs) 안에서 `YieldProcessor()`를 돌며 원자 미러를 읽고,
   조건이 서면 즉시 반환합니다. 예산 계산은 `ReadGlideGateTimingCycles()`가 아니라
   `QueryPerformanceCounter` 기준으로 하여 타이밍 계측 on/off와 독립시킵니다.
3. **적용 지점 세 곳** — `InvokeOnHostThread`의 pending 대기·complete 대기,
   `WaitAndPumpHostCommands`의 pending 대기. **스핀 성공 후에도 반드시 락을 잡고
   기존 술어로 재확인**합니다(힌트 규칙).
4. **스위치** — `REPIU_GLIDE_RENDEZVOUS_SPIN_US`, 기본 **20**, `0`이면 예전 동작.
   다른 gate와 같은 방식으로 **한 번만 해석**합니다(hot path에서 `getenv` 금지).
5. **계측** — `spin-hit` / `spin-miss`를 게스트측·호스트측으로 나눠 세고,
   `main.cpp`의 gate 타이밍 블록 옆에 한 줄로 냅니다.

**금지:** 조건변수 술어를 원자 미러로 바꾸는 것. lost wakeup의 원인이 됩니다.

## 2. 빌드

```powershell
cmd /c scripts\build_win32_x86_release.bat
```

편집한 번역 단위는 먼저 `cl /Zs`로 문법 검사합니다(전체 Release 빌드가 40분 이상).

## 3. A/B 측정

같은 빌드·같은 세션·**창 정상**·EEPROM 실행별 격리·60초, `REPIU_EXECUTION_TIME_PROFILE=1`.

| 조건 | 설정 | 횟수 |
|---|---|---:|
| off | `REPIU_GLIDE_RENDEZVOUS_SPIN_US=0` | pumpit3 3회 |
| on | 기본값(20) | pumpit3 3회 |
| 대조 | on, pumpit1 | 1회 |

교대로 돌립니다(off, on, off, on, …). 부팅 크래시(frontier 항목 3)는 표본에서 빼고
횟수만 기록합니다.

## 4. 읽을 줄

```
Win32 glide gate timing enabled/rendezvous/direct/clamped
Win32 glide gate cycles queue/wake/work/complete/residual/total
Win32 glide gate share queue/wake/work/complete/residual
Win32 glide gate mean per rendezvous total/wake/work
Win32 glide gate spin guest-hit/guest-miss/host-hit/host-miss   (신규)
Win32 Glide call trace: ... _GRBUFFERSWAP@4 count=
Win32 AOT generation publishes/quarantines
Win32 guest position census ... cpu-share (있으면)
```

## 5. 판정 — 설계 §4 그대로

| 종점 | 통과 |
|---|---|
| 1차 프레임 | pumpit3 중앙값 **≥ 2,620** (기준 2,477~2,515의 +5%) |
| 2차 구간 | `wake + complete` 합이 **30% 미만** |
| 정확성 | ordinal 호출 수 불변, `frame-errors=0`, Glide 실패 0 |
| 회귀 | pumpit1이 대조 범위 안 |

**1차 미달이면 기본값을 `0`으로 바꾸고 음성 결과로 기록합니다.** 2차만 통과한 경우
"지연은 없앴으나 프레임은 그대로"이며, 그 자체가 다음 축을 고르는 자료입니다
(Task 365·368과 같은 형태).

## 6. 완료 기준

1. 빌드가 통과하고 A/B 7회가 §4의 줄을 모두 냅니다.
2. §5 표가 채워졌습니다.
3. 작업 로그를 쓰고, [frontier](../analysis/current-execution-frontier.md) 항목 1과
   [Glide gate 비용 귀속](../analysis/glide-gate-cost-attribution.md) 15-1절을
   결과로 갱신했습니다.
4. 기본값(채택/미채택)이 frontier 환경 변수 표에 반영됐습니다.

---

# Task 419 Work Order — spin-then-wait for the Glide rendezvous

Design: [20260805-419](../design/20260805-419-glide-rendezvous-spin-wait.md).

## 1. Implementation

Two files, `glide_opengl_backend.cpp` and its header. Add `std::atomic<bool>` mirrors of
`host_command_pending_` and `host_command_complete_`, updated at the same points as the
existing flags, **without changing the mutex protocol or the condition-variable predicates**.
Add a spin helper that yields with `YieldProcessor()` inside a microsecond budget measured by
`QueryPerformanceCounter` — independent of whether gate timing is on — and apply it at three
waits: the pending and complete waits in `InvokeOnHostThread` and the pending wait in
`WaitAndPumpHostCommands`. **A successful spin still takes the lock and re-checks the original
predicate**; the atomics are hints only. The budget lives in
`REPIU_GLIDE_RENDEZVOUS_SPIN_US`, default **20**, `0` restoring the old behaviour, resolved
once rather than per call. Count spin hits and misses separately for the guest and host sides
and log them beside the gate timing block. **Do not** replace the condition-variable predicates
with the atomic mirrors — that is how lost wakeups happen.

## 2-3. Build and A/B

Syntax-check edited translation units with `cl /Zs` before the 40-minute Release build. Then,
in one session with a normal window, EEPROM isolated per run, 60 seconds and
`REPIU_EXECUTION_TIME_PROFILE=1`: three pumpit3 runs at `REPIU_GLIDE_RENDEZVOUS_SPIN_US=0`,
three at the default, alternating, plus one pumpit1 control. Exclude boot crashes from the
sample but record how many occurred.

## 4-5. Lines and verdict

Read the gate timing, cycles, share and mean lines, the new spin counters, the
`_GRBUFFERSWAP@4` count, and the quarantine line. The **primary endpoint is frames**: the
pumpit3 median must reach **2,620**, five percent over the 2,477-2,515 baseline. Secondary,
`wake + complete` must fall below **30%**. Correctness requires unchanged ordinal counts,
`frame-errors=0` and no Glide failures; pumpit1 must stay in its control range. **If the
primary fails, set the default to `0` and record the negative** — a secondary-only pass means
the latency went away without buying frames, which is itself the evidence for choosing the next
axis, as in Tasks 365 and 368.

## 6. Done when

The build passes, seven A/B runs produce every line above, the verdict table is filled, the
work log is written, and both the frontier's item 1 and the Glide gate attribution's section
15-1 carry the result — including the chosen default in the frontier's environment table.
