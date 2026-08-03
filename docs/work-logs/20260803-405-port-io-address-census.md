# 20260803-405 Port I/O 주소 census 작업 로그 / Port I/O Address Census Work Log

설계: [20260803-405](../design/20260803-405-port-io-address-census.md)

작업 지시: [20260803-405](../work-orders/20260803-405-port-io-address-census.md)

## 한국어

### 작업 요약

port I/O 주소 census를 추가하고 pumpit3 4회, pumpit1 1회를 측정했습니다. 설계가 세운
두 가설 중 **B가 확정**됐고, 최다 주소도 확정됐습니다.

### 1. 판정: 가설 B — 캐시가 아니라 arena에서 실행됩니다

**모든 실행, 모든 항목에서 `cache_count`가 0입니다.** pumpit3 4회와 pumpit1 1회를
합쳐 port I/O가 AOT 캐시 안에서 실행된 경우는 **한 번도 없습니다.**

예외 없는 dispatch slot이 적용되지 않는 이유는 planner나 emitter의 구멍이 아닙니다.
**그 코드가 캐시에서 실행되지 않기 때문**입니다. slot 기구 자체는 정상 동작합니다 —
outside-veh port I/O 15,560건이 dispatch thunk 진입 15,560건과 정확히 일치합니다.

### 2. 최다 주소는 `0x0301DB22`입니다

| run | 프레임 | census 합계 | #1 주소 | #1 횟수 | 비중 |
|---|---:|---:|---|---:|---:|
| 1 | 1 | 91,746 | `0x0301DB22` | 78,795 | 85.9% |
| **2** | **125** | **1,040,393** | **`0x0301DB22`** | **1,011,000** | **97.2%** |
| 3 | 1 | 101,177 | `0x0301DB22` | 88,054 | 87.0% |
| 4 (격리) | 0 | 214,702 | `0x0301DB22` | 202,997 | 94.5% |

`0x0301DB22`는 200회 I/O 지연 루프의 `in ax,dx`입니다. **Task 404는 격리 실행에서만
이 루프를 확인했는데, 이제 격리 없는 경로에서도 같은 명령이 원인임이 확정됐습니다.**
설계 §판정 기준의 "최다 주소가 다른 곳이면 지연 루프 가설 기각" 조건은 발동하지
않았습니다.

2위 이하는 두 자릿수 이상 작습니다(`0x030D0A1A` 10,404, `0x0301F851` PIC EOI 5,057).

### 3. 비용

125 프레임 실행(run-02, wall 122.17G cycle) 기준입니다.

| 항목 | 값 | wall 대비 |
|---|---:|---:|
| VEH gap `other` (1,045,380 × 58,126) | 60.76G | **49.7%** |
| port I/O 핸들러 본체 | 7.50G | 6.1% |
| HLE reentry funnel 총계 | 44,589 | — |

reentry funnel이 44,589건뿐인데 port I/O 예외는 1,034,948건입니다. 즉 **예외의 대부분은
캐시로 복귀를 시도조차 하지 않고 arena에서 그대로 재개**합니다. 가설 B와 일치합니다.

### 4. 검증

* Release 빌드 성공, `repiu_aot_probe` 종료 코드 0.
* pumpit1 45초 1회 **834 프레임**(Task 404 838, 그 전 700~749). 회귀 없습니다.
* overflow는 pumpit3 4회 전부 0입니다(항목 29~30 / 용량 32).

### 5. 작업 지시의 검증 기준이 틀렸습니다 — 정정합니다

지시서는 "census 합계 + overflow == profiled port-io count"를 통과 조건으로 뒀는데,
**성립하지 않으며 코드가 아니라 기준이 틀렸습니다.**

| run | profiled | census | 차이 |
|---|---:|---:|---:|
| 1 | 101,775 | 91,746 | 10,029 |
| 2 | 1,050,552 | 1,040,393 | 10,159 |
| 3 | 111,219 | 101,177 | 10,042 |
| 4 (격리) | 869,289 | 214,702 | **654,587** |

`ExecutionTimeScope`는 함수 진입 시점에 생성되므로 **opcode 검사에서 빠져나가는 호출도
전부 셉니다.** `HandlePortIoInstruction`은 단일 스텝 HLE 체인에서 port I/O가 아닌
명령에도 시도되므로, 차이는 single-step 횟수를 따라갑니다 — run-4의 single-step이
840,798로 다른 실행(258~286)보다 세 자릿수 크고 차이도 그만큼 큽니다.

**따라서 profiled `kPortIoDevice` count와 cycles는 port I/O를 과대 계상합니다.
실제 port I/O 횟수는 census 쪽입니다.** 위 3절의 비용은 gap 기준이라 영향받지 않습니다.

### 6. 한계

용량 32는 pumpit3에는 충분하지만 **pumpit1에서는 넘칩니다**(항목 32, overflow 23,990,
합계 1,897). pumpit1을 이 census로 분석하려면 용량을 늘려야 합니다.

### 7. 다음 대상

**`0x0301DB22`가 왜 AOT 캐시가 아니라 arena에서 실행되는가.** 여기에 wall의 약 50%가
걸려 있습니다. 확인할 것은 이 주소가 excluded range에 있는지, 블록이 번역된 적이 있는지,
아니면 런타임이 의도적으로 arena 실행을 택하는지입니다.

### 8. 덤으로 확보한 것: Task 404의 사유 문자열

run-4에서 격리가 발생해 Task 404 계측이 잡았습니다.

```
0x0301DFFE / page 0x0301D000 / quarantined=true / terminal=false
"dynamic AOT entry was not active in the new image"
```

격리된 페이지가 `0x0301D000`으로 확인되어 Task 404의 추론이 실측이 됐고, 사유는 여섯
후보 중 **배치 계열**입니다. 상세는
[Task 404 작업 로그 §7](20260803-404-aot-generation-failure-attribution.md).

### 9. 미확정

* 위 7절의 다음 대상.
* 재번역이 요청 진입 주소를 address map에 남기지 못하는 조건(Task 404에서 이월).
* 격리 발생 조건.

---

## English

### Summary

Added the port I/O address census and measured four pumpit3 runs and one pumpit1 run. Of the
design's two hypotheses, **B is confirmed**, and the dominant address is settled.

### 1. Verdict: hypothesis B — execution is in the arena, not the cache

**`cache_count` is zero in every entry of every run.** Across four pumpit3 runs and one
pumpit1 run, port I/O never executed from inside the AOT cache.

The exception-free dispatch slot is not bypassed because of a hole in the planner or the
emitter. It is not reached because **that code does not execute from the cache**. The slot
mechanism itself works: the 15,560 outside-VEH port I/O calls equal the 15,560 dispatch-thunk
entries exactly.

### 2. The dominant address is `0x0301DB22`

| Run | Frames | Census total | Top address | Count | Share |
|---|---:|---:|---|---:|---:|
| 1 | 1 | 91,746 | `0x0301DB22` | 78,795 | 85.9% |
| **2** | **125** | **1,040,393** | **`0x0301DB22`** | **1,011,000** | **97.2%** |
| 3 | 1 | 101,177 | `0x0301DB22` | 88,054 | 87.0% |
| 4 (quarantined) | 0 | 214,702 | `0x0301DB22` | 202,997 | 94.5% |

`0x0301DB22` is the `in ax,dx` of the 200-iteration I/O delay loop. **Task 404 had confirmed
that loop only under quarantine; the same instruction is now confirmed as the cause on the
non-quarantine path too**, so the design's "if the top address is elsewhere, reject the
delay-loop explanation" branch did not fire. Everything below it is two orders of magnitude
smaller (`0x030D0A1A` at 10,404 and the PIC EOI at `0x0301F851` at 5,057).

### 3. Cost

In the 125-frame run (wall 122.17G cycles), the `other` VEH gap is 1,045,380 events at a
58,126-cycle mean — 60.76G cycles, **49.7% of wall** — with another 7.50G (6.1%) in the port
I/O handler body. The HLE re-entry funnel recorded only 44,589 attempts against 1,034,948
port I/O exceptions, so **most exceptions never even try to return to the cache and simply
resume in the arena**, which is what hypothesis B predicts.

### 4. Verification

The Release build passed and `repiu_aot_probe` exited zero. One 45-second pumpit1 run
rendered **834 frames** (838 in Task 404, 700-749 before), so no regression. Overflow was
zero in all four pumpit3 runs, at 29-30 entries against a capacity of 32.

### 5. The work order's verification gate was wrong — corrected here

The order required `census total + overflow == profiled port-io count`. It does not hold, and
the gate rather than the code is what is wrong: profiled counts were 101,775 / 1,050,552 /
111,219 / 869,289 against census totals of 91,746 / 1,040,393 / 101,177 / 214,702.

`ExecutionTimeScope` is constructed on function entry, so it **counts every call including
those that bail at the opcode check**, and `HandlePortIoInstruction` is tried against
non-port instructions from the single-step HLE chain. The gap therefore tracks the
single-step count: run 4 single-stepped 840,798 times against 258-286 elsewhere, and its gap
is 654,587 against roughly 10,000.

**So the profiled `kPortIoDevice` count and cycles over-count port I/O, and the census is the
accurate count.** The cost figures in section 3 are gap-based and unaffected.

### 6. Limitation

Capacity 32 suffices for pumpit3 but **overflows on pumpit1** (32 entries, 23,990 overflow,
1,897 counted). Analysing pumpit1 with this census would need a larger table.

### 7. Next target

**Why `0x0301DB22` executes in the arena instead of the AOT cache.** About 50% of wall clock
rests on it. The things to check are whether the address falls in an excluded range, whether
its block was ever translated, and whether the runtime deliberately chooses arena execution.

### 8. Captured along the way: Task 404's reason string

Run 4 quarantined, and Task 404's instrumentation caught it:
`0x0301DFFE`, page `0x0301D000`, quarantined, not terminal, **"dynamic AOT entry was not
active in the new image"**. That confirms by measurement the page Task 404 had only inferred,
and puts the cause in the placement family rather than capacity or translation. Detail is in
[Task 404's work log §7](20260803-404-aot-generation-failure-attribution.md).

### 9. Unresolved

The next target in section 7; the condition under which a re-translation omits its requested
entry from the address map (carried from Task 404); and what decides whether quarantine
fires.
