# 작업 로그: single-step 모집단 조사 / Work log: single-step population investigation

Task 373. 설계 [20260731-373](../design/20260731-373-single-step-population-elimination.md),
작업 지시 [20260731-373](../work-orders/20260731-373-single-step-population-elimination.md)

**결과: 구현하지 않음.** 사전 등록 게이트 C(< 5%)에 해당하며, 1단계 계측은 만들 필요가
없었습니다 — Task 340이 이미 같은 질문에 답하는 계측을 갖고 있었습니다.

## 한국어

### 1. 새로 만들 것이 없었다

작업 지시 2단계는 "기존 `AotDbtDispatchFallbackReason`과 겹치면 **재사용**한다. 새
열거를 만들기 전에 연결 가능성을 먼저 확인할 것"이었습니다. 확인 결과 Task 340이
만든 **HLE 재진입 funnel**이 정확히 이 질문에 답하고 있었고, 이미 요약에 출력되고
있었습니다.

trap flag를 세우는 지점은 네 곳입니다
([execution_trampoline.cpp](../../src/platform/win32/execution/execution_trampoline.cpp)).

| 위치 | 조건 |
|---|---|
| `LeaveNativeRegion` | 네이티브 리전 이탈 |
| post-HLE | `TryResumeAotAfterHandledHle` 실패 |
| 네이티브 미진입 | single-step 유지 |
| VEH 진입 | `enable_single_step_trace` |

이 중 두 번째가 설계가 지목한 대상이고, 그 사유는 이미 이름이 붙어 있었습니다.

### 2. 측정 결과 — 설계의 전제가 틀렸다

music select 캡처(24.1초 / 793프레임):

```
hle reentry funnel not-pending/backend/segment-write/outside-arena/quarantined/span-unsafe/success/total:
                       0 /   0  /      0       /      0      /     0      /   8984    / 52617 / 61601
share: 0.00% / 0.00% / 0.00% / 0.00% / 14.58% / 85.42%
```

**AOT 재진입은 85.42% 성공합니다.** 실패는 `span-unsafe` 하나뿐이고 나머지 다섯
사유는 전부 0입니다.

설계는 "재진입 실패가 single-step을 만든다"고 전제했는데, **재진입은 대부분
성공하고 있습니다.**

### 3. 게이트 판정: C

| 항목 | 값 |
|---|---:|
| `span-unsafe` 거부 | 8,984회 |
| 회피 가능한 커널 왕복 | 8,984 × 30,611 = 0.28e9 |
| wall | 88,551,784,353 |
| **제거 상한** | **0.31%** |

| 사전 등록 게이트 | 기준 | 판정 |
|---|---|---|
| A — 구현 진행 | ≥ 10% | |
| B — 상위 사유만 표적 | 5 ~ 10% | |
| **C — 구현 안 함, 축 종결** | **< 5%** | **← 0.31%** |

Task 368의 규율대로 미달이므로 구현하지 않습니다.

참고로 `span-safety` **판정 자체의 비용**은 호출당 18,546 cycle, wall의 1.07%로,
그것이 막아주는 것(0.31%)보다 3.5배 큽니다. 다만 그것도 게이트 미달이라 별도 작업의
근거가 되지는 못합니다.

### 4. 판정보다 중요한 발견 — 계측의 사각지대

수치를 대조하다 funnel이 설명하는 범위가 훨씬 좁다는 것이 드러났습니다.

| 지표 | 값 | 비중 |
|---|---:|---:|
| single-step 예외 총계 | **266,879** | 100% |
| `single_step_trace_count` | 79,866 | **29.9%** |
| HLE 재진입 시도 | 61,601 | 23.1% |
| **어떤 계측에도 안 잡히는 나머지** | **187,013** | **70.1%** |

> **[Task 376에서 반증됨 — 이 절의 결론은 철회합니다]**
>
> 여기서 "아레나 밖 single-step 187,013건이 계측 없이 버려지고 wall의 6.5%"라고
> 결론지었습니다. **틀렸습니다.** Task 376이 버리는 지점에 계측을 넣어 직접
> 측정한 결과 **0건**입니다.
>
> 오류의 원인은 **측정 대상이 다른 두 카운터를 뺀 것**입니다.
> `veh_single_step_exception_count`는 VEH가 본 모든 `EXCEPTION_SINGLE_STEP`을 세고,
> `single_step_trace_count`는 `HandleSingleStepTrace` 안에서 **예외 종류와 무관하게**
> 증가합니다(그 함수는 `aot_reentry_pending`인 breakpoint도 처리합니다). 애초에
> 뺄 수 있는 값이 아니었고, 그 차이를 "숨은 모집단"으로 읽은 것이 근거 없는 6.5%를
> 만들었습니다.
>
> 실제 소비처는 이미 계측되어 있었습니다 — `aot reentry ... single-step`이
> **489,167건(99.98%)** 으로, `kAotReentry`의 71.73%를 차지합니다. 낭비가 아니라
> DBT의 정상 경로입니다.
>
> 상세: [Task 376 작업 로그](20260801-376-out-of-arena-single-step-attribution.md)

### 5. 다음 작업

Task 376이 위 전제를 반증했고, 그 과정에서 진짜 표적이 드러났습니다 —
`kAotReentrySingleStep`이 `kAotReentry`의 **71.73%**, `kAotReentry`가 aot transfer의
79.93%이므로 **AOT transfer 시간의 약 57%** 입니다.

---

## English

### Nothing needed building

The work order required checking whether the existing fallback-reason enumeration
could be reused before adding a new one. It could: Task 340's HLE re-entry funnel
already answers this exact question and already prints in the summary. Four sites
re-arm the trap flag — leaving a native region, a failed post-HLE resume, native
execution not entered, and the diagnostic trace mode — and the second, which the
design targeted, was already attributed by reason.

### The design's premise was wrong

Over 24.1 seconds of music select the funnel reads 0 / 0 / 0 / 0 / 0 / 8,984 /
52,617 of 61,601: **AOT re-entry succeeds 85.42% of the time**, and the only failure
reason is `span-unsafe` at 14.58%. The design assumed failed re-entry was what
produced single steps; mostly it does not fail.

### Gate C

Avoiding all 8,984 rejections would remove 8,984 × 30,611 cycles, which is **0.31%
of wall** against a pre-registered gate of 10% to implement, 5 to 10% to target the
top reasons, and below 5% to close. Task 368's discipline applies and this is not
implemented. For context the `span-safety` check itself costs 18,546 cycles per call
and 1.07% of wall — three and a half times what it prevents — but that is also below
the gate and cannot justify separate work on its own.

### A conclusion drawn here, and withdrawn

This section concluded that 187,013 single steps were being discarded uncounted at
6.5% of wall. **Task 376 measured the discard site directly and found zero.**

The error was subtracting two counters that measure different things:
`veh_single_step_exception_count` counts every `EXCEPTION_SINGLE_STEP` the VEH sees,
while `single_step_trace_count` increments inside `HandleSingleStepTrace` regardless
of exception code — and that function also serves breakpoints when
`aot_reentry_pending`. The difference was never a population, and reading it as one
manufactured a 6.5% figure with nothing behind it.

The real consumer was already instrumented: `aot reentry ... single-step` accounts
for 489,167 of 489,245, or 99.98%, and holds 71.73% of `kAotReentry`. That is the
DBT working as designed, not waste. See the Task 376 work log.
