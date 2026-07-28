# 20260728-336 작업 로그: VEH residual 귀속과 예외 전이 가격 재측정 / Work log

작업 지시: [20260728-336-veh-residual-and-exception-price.md](../work-orders/20260728-336-veh-residual-and-exception-price.md)

## 한국어

### 결론 요약

**두 가지가 확인됐고 하나는 오래된 결론을 뒤집습니다.**

1. **VEH residual은 미계측 구간이 아니었습니다.** `HandleSingleStepTrace`의 단계
   profile이 별도 opt-in(`REPIU_SINGLE_STEP_HOTSPOT_PROFILE`)이라 꺼져 있었을 뿐입니다.
   켜자 residual이 **36.56% → 3.26%** 로 떨어지고 `single-step`이 VEH의 **33.68%**
   로 나타났습니다. **코드 변경 없이 귀속됐습니다.**

2. **커널 예외 전이는 1.20%가 아니라 전체의 약 27.7~30.4%입니다.** 따라서 TF/`INT3`
   제거 상한은 `1.012배`가 아니라 **약 1.38~1.44배**입니다. 오래 보류해 온 로드맵을
   **재개 후보로 되돌립니다.**

### 1. VEH residual = single-step 핸들러

| VEH 내부 | profile OFF | profile ON |
|---|---:|---:|
| `single-step` | 0.00%(미집계) | **33.68%** |
| `aot-transfer` | 18.55% | 16.80% |
| residual | **36.56%** | **3.26%** |
| prologue / telemetry / gates / hle-chain | 2.01% | 2.32% |
| Glide gate | 42.89% | 43.94% |

single-step 핸들러 내부(Release, 60초, 표본 942,160):

| 단계 | tick | 비중 | 전체 대비 |
|---|---:|---:|---:|
| **`hle`** | 11,430,472,341 | **66.4%** | **7.02%** |
| `native` | 3,099,828,038 | 18.0% | 1.90% |
| `prologue` | 1,475,706,177 | 8.6% | 0.91% |
| `aot-resume` | 1,105,601,349 | 6.4% | 0.68% |
| `timer` | 101,713,386 | 0.6% | 0.06% |

**확인됨:** Task 322가 Debug에서 `aot-resume` 74.05%로 측정했던 순위는 Release에서
완전히 뒤집혔습니다. 지금은 `hle`가 66.4%이고 `aot-resume`는 6.4%입니다. Task 324의
해시 색인이 그 경로를 이미 없앴기 때문입니다.

### 2. 예외 전이 가격 — 구성과 무관합니다

`repiu_aot_probe`의 calibration을 두 구성에서 읽었습니다.

| 전이 종류 | Debug | Release |
|---|---:|---:|
| `INT3` | 34,608 tick | 34,521 tick |
| single-step | 37,519 tick | 37,885 tick |

**확인됨:** 차이는 1% 미만입니다. 커널 비용이므로 최적화 수준과 무관하며, Task 329의
스냅샷 대역폭과 같은 성격입니다. 2.5GHz 기준 전이 1회는 약 **13.8~15.2us**입니다.

### 3. 그래서 상한이 바뀝니다

같은 60초 Release 실행에서 VEH 진입은 **1,307,096회**, guest-run은
`162,768,499,179 tick`입니다.

| 항목 | 값 |
|---|---:|
| 전이 총비용(하한, `INT3` 가격) | 45,123,254,616 tick = **27.72%** |
| 전이 총비용(상한, single-step 가격) | 49,519,331,960 tick = **30.42%** |
| `unaccounted` | 111,659,052,894 tick = 68.60% |
| 따라서 AOT 캐시 내 **실제 guest 실행** | **38.2~40.9%** |

`unaccounted`(68.60%) 안에 전이(27.7~30.4%)가 들어가고 남는 값이 양수이므로 유도는
모순이 없습니다.

**TF/`INT3`를 모두 제거했을 때의 상한은 `1/(1-0.277) ~ 1/(1-0.304)`, 즉 약
1.38~1.44배입니다.**

### 4. 왜 1.20%가 아니었나 — 숫자가 아니라 세상이 바뀌었습니다

Task 323의 1.20%는 **틀린 측정이 아니었습니다.** 전이 가격은 그때도 지금과 같았고,
달라진 것은 **횟수**입니다. 당시 실행은 모든 것이 느려 예외가 드물었고(그 시기 progress는
`8,199` 수준), 지금은 같은 60초에 VEH 진입이 1,307,096회입니다.

즉 **고정 커널 비용은 그대로인데 주변이 빨라져 비중이 커졌습니다.** Task 329가 스냅샷을
두고 "대역폭 비용은 구성과 무관하다"고 했던 것과 같은 구조이며, 여기서는 시간이 아니라
**속도 향상**이 그 비중을 키웠습니다.

**따라서 방법론 규칙을 하나 더 추가합니다.** 고정 비용(커널 전이, syscall, 대역폭)의
**비중**은 다른 곳을 최적화할 때마다 재계산해야 합니다. 한 번 "작다"고 판정한 항목도
전체가 빨라지면 지배 항목이 될 수 있습니다.

### 확인됨 / Confirmed

* VEH residual의 정체는 `HandleSingleStepTrace`이며 계측은 이미 있었습니다.
* single-step 내부 1위는 Release에서 `hle` 66.4%(전체의 7.02%)입니다.
* 예외 전이 1회는 Debug/Release 모두 약 34.5~37.9k tick으로 구성 독립입니다.
* 커널 전이는 현재 전체의 **27.7~30.4%** 이며 TF/VEH 제거 상한은 약 **1.4배**입니다.

### 미확정 / Unresolved

* 전이 가격은 probe의 최소 핸들러 기준입니다. 로더는 등록된 핸들러가 더 많으므로 실제
  비용은 **더 클 수는 있어도 작지는 않습니다.** 실행 중 직접 재지는 않았습니다.
* `INT3`와 single-step의 실제 혼합 비율을 세지 않아 27.7~30.4% 구간으로만 말합니다.
* 예외를 없애면 핸들러가 하던 일은 다른 형태로 해야 하므로, 1.4배는 **전이만 없앨 때의
  상한**입니다. 핸들러 본문(31.40%)까지 싸게 만드는 설계라면 상한은 더 높습니다.
* 이번 실행은 progress 143,818 / 프레임 2,569로 지금까지 중 가장 빨랐습니다. 계측을
  더 켰는데도 그렇습니다. Task 335가 기록한 실행 간 편차 문제가 계속됩니다.

---

## English

### Summary

Two findings, one of which overturns a long-standing conclusion. First, the VEH residual was never
uninstrumented: `HandleSingleStepTrace` has a stage profile behind its own opt-in
(`REPIU_SINGLE_STEP_HOTSPOT_PROFILE`), and enabling it drops the residual from 36.56% to 3.26% of
the VEH while `single-step` appears at 33.68% — attributed with no code change. Second, kernel
exception transition is not 1.20% of wall clock but roughly 27.7-30.4%, so the bound on removing TF
and `INT3` is not 1.012x but about 1.38-1.44x, which returns that shelved roadmap to the candidate
list.

### The residual is the single-step handler

Inside the single-step handler over 942,160 samples, `hle` holds 66.4% (7.02% of the whole run),
`native` 18.0%, `prologue` 8.6%, `aot-resume` 6.4%, and `timer` 0.6%. Task 322 measured
`aot-resume` at 74.05% in Debug; that ranking is fully inverted in Release because Task 324's hash
index removed that path.

### The transition price is configuration-independent

The probe's calibration reads 34,608 ticks for `INT3` and 37,519 for single-step in Debug against
34,521 and 37,885 in Release — under 1% apart, as expected for kernel cost, the same character as
Task 329's bandwidth finding. One transition is about 13.8-15.2us at 2.5GHz.

### Which changes the bound

The same 60-second Release run took 1,307,096 VEH entries against a guest run of
`162,768,499,179` ticks, so transitions cost between `45,123,254,616` and `49,519,331,960` ticks,
or 27.72% to 30.42% of wall clock. That fits inside `unaccounted` at 68.60% and leaves 38.2-40.9%
for real guest execution inside the AOT cache, so the derivation is consistent. Removing every TF
and `INT3` therefore bounds improvement at about 1.38-1.44x.

### Why it was not 1.20%: the world changed, not the number

Task 323's 1.20% was not a bad measurement. The price per transition was the same then as now; what
changed is the count. That run was slow enough that exceptions were rare, and the same 60 seconds
now takes 1,307,096 VEH entries. A fixed kernel cost stayed fixed while everything around it got
faster, so its share grew — the same structure as Task 329's bandwidth argument, driven here by
speedups rather than by configuration. This adds a method rule: the share of any fixed cost —
kernel transitions, syscalls, bandwidth — must be recomputed after every optimization elsewhere,
because an item once judged small can become dominant.

### Unresolved

The transition price comes from the probe's minimal handler; the loader registers more, so the real
cost can only be higher, and it was not measured in situ. The actual mix of `INT3` against
single-step exceptions was not counted, hence the 27.7-30.4% range. Removing exceptions still
requires doing the handlers' work another way, so 1.4x bounds removing the transition alone; a
design that also makes the 31.40% handler body cheaper would have a higher ceiling. This run was
also the fastest yet at progress 143,818 and 2,569 frames despite more instrumentation being on,
so Task 335's run-to-run variance problem persists.
