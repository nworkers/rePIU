# 작업 로그: 아레나 밖 single-step 귀속 / Work log: attributing out-of-arena single steps

Task 376. 설계 [20260801-376](../design/20260801-376-out-of-arena-single-step-attribution.md),
작업 지시 [20260801-376](../work-orders/20260801-376-out-of-arena-single-step-attribution.md)

**결과: 전제가 반증됨. 게이트 C.** 억제할 모집단이 존재하지 않습니다.

## 한국어

### 1. 반증 — 버려지는 것은 0건이다

Task 373이 "아레나 밖 single-step 187,013건(70.1%)이 계측 없이 버려지고 wall의
6.5%"라고 결론지었습니다. 그 버리는 지점
([execution_trampoline.cpp](../../src/platform/win32/execution/execution_trampoline.cpp))에
계측을 넣어 직접 측정했습니다.

```
Win32 out-of-arena step total/aot-cache/other: 0/0/0
Win32 out-of-arena step trace-on/reentry-pending: 0/0
Win32 single-step disposition trace-on/trace-off: 0/0
```

같은 실행의 대조군:

```
exception census single-step/breakpoint/...: 489245/578428/...
single-step trace count: 119159
```

**489,245건의 single-step 중 이 경로로 버려지는 것은 0건입니다.**

### 2. 오류의 원인 — 뺄 수 없는 두 값을 뺐다

| 카운터 | 세는 것 |
|---|---|
| `veh_single_step_exception_count` | VEH가 본 **모든** `EXCEPTION_SINGLE_STEP` |
| `single_step_trace_count` | `HandleSingleStepTrace` 안에서, 아레나 안, **예외 종류 무관** |

두 번째가 결정적입니다. `HandleSingleStepTrace`는 `aot_reentry_pending`인
**breakpoint도 처리**하므로, 이 카운터는 single-step 전용이 아닙니다. 이름이
오해를 부릅니다.

**측정 대상이 다른 두 값의 차이를 "숨은 모집단"으로 읽었고**, 거기서 근거 없는
6.5%가 나왔습니다. Task 373 작업 로그와 `current-execution-frontier.md` 두 곳에
그 주장을 기록했고, 이번에 철회했습니다.

### 3. 실제 소비처는 이미 계측되어 있었다

새 캡처가 필요 없었습니다. 같은 로그에 답이 있었습니다.

```
aot reentry count guest-lookup/provenance/retired/boundary-reason/native-span/single-step:
                   576346 / 576346 / 16501 / 559845 / 0 / 489167
aot reentry share ...single-step/residual: ... 71.73% / 2.32%
```

**single-step 489,245건 중 489,167건(99.98%)이 `kAotReentrySingleStep` 단계에서
소비됩니다.** 낭비가 아니라 DBT가 설계대로 동작하는 정상 경로입니다.

single-step 예외는 VEH census 지점(2921) 이후 AOT transfer 핸들러 체인에서 소비되고,
제가 계측을 넣은 지점(3156, 3265)까지 도달하지 않습니다.

### 4. `kAotReentrySingleStep`이 무엇인지 — 표적이 아니다

처음에 이 단계를 "다음 표적(AOT transfer의 57%)"으로 적었으나 **부정확한
표현이었습니다.** 코드를 확인했습니다
([aot_runtime_dispatch.cpp:1812](../../src/platform/win32/aot/aot_runtime_dispatch.cpp#L1812)).

```cpp
if (code != EXCEPTION_SINGLE_STEP || !context->aot_reentry_pending) return false;
// Task 334 interval 6: the single-step resumption path in full.
const ExecutionTimeScope single_step_scope(..., kAotReentrySingleStep);
```

**이것은 "AOT 캐시로 돌아가는 재진입 경로 전체"이며 DBT의 핵심 동작 그 자체입니다.**
경계마다 반드시 거쳐야 하고, 없애면 AOT가 동작하지 않습니다. 그 안의 분기는 세
가지입니다 — EIP가 이미 캐시면 TF를 끄고 완료, quarantine이면 single-step 유지,
그 외에는 `ResolveAotTransferTarget`으로 게스트→캐시 주소를 변환해 점프.

따라서 "`kAotReentry`의 71.73%"는 **재진입 시간의 대부분이 재진입 본체**라는
동어반복에 가깝고, 부대 비용(guest-lookup 5.05%, provenance 4.99%, retired 13.71%,
boundary-reason 2.19%)이 작다는 뜻으로 읽는 편이 맞습니다.

**줄일 수 있는 것은 이 구간의 내용이 아니라 횟수입니다.** 재진입 489,167회 ×
왕복 약 30,650 cycle ≈ **wall의 9%** 이고, 이를 줄이려면 **경계 발생 자체를 줄여야**
합니다. 그 원인 분포는 `aot boundary effective opcodes`가 이미 세고 있습니다.

### 5. 게이트 판정: C

| 등급 | 기준 | 판정 |
|---|---|---|
| A | 단일 무장 지점이 70% 이상 | |
| B | 상위 두 지점이 70% 이상 | |
| **C** | **지배적 원인 없음 / 전제 반증** | **← 버릴 모집단이 0** |

억제 구현을 하지 않습니다.

### 6. 계측은 남깁니다

값이 전부 0이지만 되돌리지 않습니다. **버림 경로가 실제로 0이라는 것 자체가 이번에
확인된 결론**이고, 누가 그 경로를 살리는 변경을 하면 요약에 즉시 드러납니다. 회귀
감지용으로 유지합니다.

`single-step disposition trace-on/trace-off` 줄도 같은 이유로 남깁니다 — 0이라는
사실이 "single-step은 이 지점에 오지 않는다"를 상시 확인해 줍니다.

### 7. 검증

* Debug/Release 빌드 성공, `repiu_aot_probe` 양 구성 **exit 0**, 신규 probe **5개 항목
  전부 true**(위치 분류, 최초/최후 EIP, 주소 집계, 오버플로 계수, `nullptr` 무해).
* hot path에 clock read 추가 **0회** — 카운터 증가만.
* 기존 동작 **불변** — TF 클리어 후 continue 그대로.

### 8. 교훈

이 세션에서 세 번째로 같은 실수를 했습니다.

| Task | 잘못된 전제 | 반증 |
|---|---|---|
| 370 | present가 44 µs이므로 GPU가 안 밀림 | 371: 인과가 거꾸로, 디스플레이 제한 |
| 372 | Task 368이 커널 전이를 못 봤다 | 근거 문서를 끝까지 안 읽음 |
| 373 | 두 카운터의 차이 = 숨은 모집단 | 376: 뺄 수 없는 값이었음 |

공통점은 **계측값의 정의를 확인하지 않고 산술을 했다**는 것입니다. 카운터를 빼거나
나누기 전에 각각이 무엇을 세는지 코드에서 확인하는 절차를 앞으로 지킵니다.

---

## English

### Disproved: nothing is discarded

Task 373 concluded that 187,013 single steps — 70.1% of the population and 6.5% of
wall — were discarded uncounted. Instrumenting that discard site directly measured
**zero**, against 489,245 single-step exceptions in the same run.

### The error

`veh_single_step_exception_count` counts every `EXCEPTION_SINGLE_STEP` the VEH sees;
`single_step_trace_count` increments inside `HandleSingleStepTrace` regardless of
exception code, and that function also serves breakpoints when `aot_reentry_pending`.
Subtracting two counters that measure different things produced a population that
does not exist, and a 6.5% figure with nothing behind it. Both documents carrying
that claim are corrected.

### The real consumer was already instrumented

No new capture was needed; the same log already answered it. `aot reentry ...
single-step` accounts for **489,167 of 489,245 (99.98%)** and holds 71.73% of
`kAotReentry`. Single steps are consumed by the AOT transfer handler chain well
before reaching the sites instrumented here — this is the DBT working as designed,
not waste.

### What `kAotReentrySingleStep` actually is

Called a target at first, which was imprecise. The scope opens only for
`EXCEPTION_SINGLE_STEP` with `aot_reentry_pending` and covers the resumption path in
full: clear the trap flag when EIP already sits in the cache, stay stepping when the
page is quarantined, otherwise resolve the guest address to a cache address and jump.
It is the DBT's core operation, not overhead, so "71.73% of kAotReentry" mostly says
the incidental costs around it are small. What can be reduced is the count, not the
body: 489,167 re-entries at roughly 30,650 cycles of kernel round trip each is about
9% of wall, and cutting it means producing fewer boundaries — a distribution the
existing `aot boundary effective opcodes` census already records.

### Gate C, and why the instrument stays

No suppression is implemented: there is no population to suppress. The counters are
kept anyway, all reading zero, because zero is itself the result and any change that
revives that path will show up immediately in the summary. Both configurations
build, all five probe assertions pass, no clock read entered the hot path, and
behaviour is unchanged.

### Lesson

Three times this session a conclusion rested on arithmetic over instrument values
whose definitions had not been checked — Task 370's causality, Task 372's reading of
Task 368, and Task 373's counter subtraction. The rule going forward is to confirm
in code what each counter counts before subtracting or dividing them.
