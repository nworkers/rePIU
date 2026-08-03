# 20260803-407 Arena 실행 진입 추적 작업 로그 / Arena Execution Entry Trace Work Log

설계: [20260803-407](../design/20260803-407-arena-execution-entry-trace.md)

작업 지시: [20260803-407](../work-orders/20260803-407-arena-execution-entry-trace.md)

## 한국어

### 작업 요약

arena 진입 추적을 넣었습니다. **arena 진입 신호 두 가지를 확보했고, 두 실행 모드의
차이를 확정**했습니다. 다만 **정상 모드에서 지연 루프가 진입하는 순간은 잡지 못했고**,
그 이유는 계측 설계의 한계입니다. 아래 4절에 기록합니다.

### 1. 확정: 두 모드는 서로 다른 상태입니다

Task 404가 나눈 두 갈래가 재진입 관점에서 정확히 반대임이 확인됐습니다.

| | 격리 실행 | 정상 실행 |
|---|---|---|
| census `reentry` | `0x0301DB22`의 **93~98%** | **0%** |
| 진입 시 `prev_code` | `0x80000004` single-step | `0xC0000005` access violation |
| 진입 시 `prev_eip` | `0x0301DB20` (arena) | `0x0301F827` (arena) |
| 진입 시 TF | **켜짐** | 꺼짐 |
| 진입 시 reentry/step | **켜짐** | 꺼짐 |

**격리 실행에서는 런타임이 복귀를 시도하고 있습니다.** TF를 켜고 재진입을 예약한 채
매 명령을 single-step하는데, 페이지가 격리돼 있어 복귀가 매번 거부됩니다. Task 404가
재진입 거부 수로 추론했던 것을 반대편에서 확인한 셈입니다.

**정상 실행에서는 시도조차 하지 않습니다.** TF가 꺼져 있고 예약도 없어 자유 실행이며,
Task 406의 `reentry` 0과 일치합니다.

### 2. 확정: arena 진입 신호가 둘입니다

**(a) 부팅기 — 캐시 INT3 이후.** 첫 16건이 3회 실행에서 **완전히 동일**했습니다.

```
0x0301F516 / prev 0x80000003 @ 0x0C2870EC (캐시) / tf=false reentry=false
0x0301F54A / prev 0x80000003 @ 0x0C2870EC (캐시) / tf=false reentry=false
0x0301F559 / prev 0x80000003 @ 0x0C28548E (캐시) / tf=false reentry=false
0x0301F597 / prev 0x80000003 @ 0x0C2466DE (캐시) / tf=false reentry=false
0x0301F5A6 / prev 0x80000003 @ 0x0C2934F8 (캐시) / tf=false reentry=false
0x030D0A1A / prev 0x80000003 @ 0x0C298DFB (캐시) x11
```

직전 예외가 **캐시 주소의 INT3**인데 그 다음 port I/O는 arena이고 TF가 꺼져 있습니다.
`aot_runtime_dispatch.cpp:1791`의 경계 경로는 TF를 켜고 재진입을 예약하므로,
**이 INT3들은 그 경로가 처리한 것이 아닙니다.**

**(b) 정상 상태 — arena access violation 이후.** 정상 실행의 마지막 16건은 전부
`0x0301F851`(PIC EOI `out`)이고 직전이 `0x0301F827`의 access violation입니다. 둘 다
arena이며 TF·예약 모두 꺼져 있습니다. 인터럽트 핸들러 영역도 arena에서 자유 실행
중입니다.

**공통점:** 두 신호 모두 **TF 꺼짐 + 재진입 예약 없음**으로 끝납니다. 즉 여러 HLE
경로가 실행을 arena에 남겨 두고, 남은 뒤에는 자기 유지됩니다.

### 3. 규모

진입 전이는 실행당 **11,597 ~ 239,423회**입니다. 일회성 사고가 아니라 상시 동작입니다.

### 4. 계측의 한계 — 목표 하나를 달성하지 못했습니다

**정상 모드에서 `0x0301DB22`가 진입하는 순간은 잡지 못했습니다.**

* 첫 시도는 **선두 16건**만 남겨 부팅기에 다 소진됐습니다(overflow 11,596~13,635).
* ring으로 고쳐 **최신 16건**을 남기게 했더니, 정상 실행에서는 마지막 16건이 전부
  PIC EOI였고, `0x0301DB22`가 채운 실행은 두 번 다 격리 모드였습니다.

즉 전역 ring으로는 "특정 주소의 진입"을 겨냥할 수 없습니다. **필요한 계측은
주소별 표본**입니다 — port I/O census 항목마다 진입 전이 1건을 보관하면 모드와
무관하게 각 주소의 진입 신호가 남습니다.

### 5. 검증

* Release 빌드 성공, `repiu_aot_probe` 종료 코드 0.
* 동작 불변: 같은 실행의 Task 405/406 census 구조가 그대로입니다
  (`0x0301DB22` 최다, `cache` 0).
* pumpit1 45초: 선두 16 버전 **825 프레임**, ring 버전 **838 프레임**. 오늘 범위
  700~838 안이므로 두 버전 모두 회귀 없습니다.

### 6. 다음 대상

1. **주소별 진입 표본**(4절). 정상 모드에서 지연 루프의 진입 신호를 확보한다.
2. **(a) 신호의 INT3 정체.** 캐시 INT3인데 경계 경로가 아닌 것이 무엇인지.
3. **(b) 신호의 access violation.** `0x0301F827`의 AV 처리가 왜 arena에 남기는지.

### 7. 미확정

* 정상 모드 지연 루프의 진입 신호.
* 재번역이 요청 진입 주소를 address map에 남기지 못하는 조건(Task 404 이월).
* 격리 발생 조건.

---

## English

### Summary

Added the arena entry trace. It **captured two distinct arena-entry signatures and settled the
difference between the two execution modes**, but it **did not capture the delay loop entering
arena mode in the healthy runs**, for a reason that is a limitation of the instrument itself,
recorded in section 4.

### 1. Confirmed: the two modes are opposite states

| | Quarantined run | Healthy run |
|---|---|---|
| Census `reentry` | **93-98%** at `0x0301DB22` | **0%** |
| `prev_code` at entry | `0x80000004` single step | `0xC0000005` access violation |
| `prev_eip` at entry | `0x0301DB20` (arena) | `0x0301F827` (arena) |
| Trap flag | **set** | clear |
| reentry / step | **set** | clear |

**In the quarantined runs the runtime is actively trying to return**: the trap flag is on,
re-entry is scheduled, every instruction single-steps, and the quarantined page refuses the
return each time — Task 404's chain confirmed from the opposite side. **In the healthy runs it
never tries**: no trap flag, nothing scheduled, free-running, matching Task 406's zero.

### 2. Confirmed: two arena-entry signatures

**(a) Boot phase, after a cache INT3.** The first sixteen entries were **identical across
three runs**: port I/O at `0x0301F516`, `0x0301F54A`, `0x0301F559`, `0x0301F597`, `0x0301F5A6`
and eleven at `0x030D0A1A`, each preceded by an `0x80000003` breakpoint at a **cache** address
(`0x0C2870EC`, `0x0C28548E`, `0x0C2466DE`, `0x0C2934F8`, `0x0C298DFB`), with the trap flag
clear and nothing scheduled. Since the boundary path at `aot_runtime_dispatch.cpp:1791` always
sets the trap flag and schedules re-entry, **these breakpoints were not handled by that path**.

**(b) Steady state, after an arena access violation.** In the healthy run the last sixteen
entries are all `0x0301F851` (the PIC EOI `out`) preceded by an access violation at
`0x0301F827` — both in the arena, trap flag and scheduling clear. The interrupt-handler region
free-runs in the arena too.

**What they share:** both end with **no trap flag and no scheduled re-entry**, so several HLE
paths leave execution in the arena, and once left it sustains itself.

### 3. Scale

Entry transitions number **11,597 to 239,423 per run**. This is routine behaviour, not a
one-off.

### 4. Limitation — one goal was not met

**The healthy-mode entry at `0x0301DB22` was not captured.** The first version kept the
*earliest* sixteen, which boot consumed entirely (overflow 11,596-13,635). Rebuilt as a ring
keeping the *newest* sixteen, the healthy run's last sixteen were all the PIC EOI, and the two
runs whose ring was filled by `0x0301DB22` were both in quarantined mode.

A global ring cannot target a specific address. The instrument this needs is **one sample per
address** — keeping a single entry transition per port I/O census entry would preserve each
address's signature regardless of mode.

### 5. Verification

Release build passed and the probe exited zero. Behaviour is unchanged: the Task 405 and 406
census structure is identical in the same runs, with `0x0301DB22` on top and `cache` at zero.
pumpit1 rendered **825 frames** on the prefix version and **838** on the ring version, both
inside today's 700-838 range.

### 6. Next targets

The per-address entry sample from section 4, to get the healthy-mode signature for the delay
loop; the identity of signature (a)'s breakpoint, which sits at a cache address but is not the
boundary path; and why signature (b)'s access-violation handling leaves execution in the arena.

### 7. Unresolved

The healthy-mode entry signature for the delay loop; the condition under which a re-translation
omits its requested entry from the address map (carried from Task 404); and what decides
whether quarantine fires.
