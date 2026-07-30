# 20260730-368 예외 없는 Glide gate dispatch 설계 / Exception-free Glide gate dispatch

## 한국어

### 1. 목적

[Task 367](20260730-367-hle-boundary-opcode-attribution.md)이 확인한 최대 예외 인구를
제거합니다. Glide gate trap(`0F 0B` UD2)은 boundary 표본의 **55.21%**, 전체 예외의
약 **24.9%** 이며 **Glide API 호출 1회당 예외 1회**입니다.

목표는 guest 호출 규약과 렌더링 의미를 그대로 두고 **trap 방식만** 바꾸는 것입니다.

### 2. 현재 기계 — 코드 감사 결과

`BuildGlideGatePlan`은 export ordinal마다 5바이트 stub을 합성합니다.

| offset | 바이트 | 의미 |
|---:|---|---|
| +0 | `0F 0B` | **UD2 — 우리가 넣은 trap** |
| +2 | ordinal lo/hi | gate 식별자 |
| +4 | `C3` | `RET` (미처리 시 fallback) |

guest가 Glide export를 호출하면 이 stub으로 점프하고, UD2가 예외를 일으키며,
`HandleGlideGateBoundary`가 EIP에서 gate offset을 복원해 dispatch한 뒤 EIP를 반환
주소로, ESP를 인수만큼 되돌립니다.

```mermaid
flowchart LR
    G["guest Glide 호출"] --> S["gate stub<br/>0F 0B + ordinal + C3"]
    S --> X["<b>예외</b> (호출당 1회)"]
    X --> V["VEH → HandleGlideGateBoundary"]
    V --> D["ordinal dispatch"]
    D --> R["EIP=반환주소, ESP+=인수"]
```

### 3. 감사로 확인된 세 가지 — 이것이 작업의 형태를 정합니다

**확인됨 1: 예외 없는 dispatch 기계가 이미 존재합니다.** `EmitHleDispatchSlot`이
dispatch 주소와 guest 주소를 push하고 thunk로 `JMP rel32`하며 INT3 fallback을 남깁니다.
Task 308이 이 계열의 host-call thunk가 GPR/EFLAGS·x87/MMX/SSE·host stack/TIB 경계를
보존하고 60초 실게임을 exception 0, fallback 0, EEPROM 일치로 완료함을 이미
검증했습니다. **새 기계를 만들 필요가 없습니다.**

**확인됨 2: 그 기계는 `REPIU_AOT_DBT_SUPERBLOCK`에 묶여 있습니다.**
`enable_dbt_hle_dispatch`는 backend가 `aot-dbt`이고 **SUPERBLOCK이 켜졌을 때만**
`true`가 됩니다. SUPERBLOCK은 현재 렌더링을 중단시키므로 켤 수 없고, 따라서 **예외 없는
dispatch는 지금까지 단 한 번도 독립적으로 평가된 적이 없습니다.** 측정 로그도
`superblock HLE dispatch enabled: false`, host dispatch 계수 전부 0입니다.

**확인됨 3: 지금 상태로는 flag를 켜도 Glide gate에는 적용되지 않습니다.**
`IsHleBoundary`는 privileged 속성이나 segment 속성이 있는 명령만 boundary로 봅니다.
UD2는 둘 다 아니므로 boundary로 분류되지 않고 `kOther`로 떨어집니다(boundary reason
census `other 137,023`이 이를 확인). 즉 dispatch slot이 emit될 수 없습니다.

### 4. 설계 — 세 조각으로 분리

이 작업이 Task 308과 다른 점은 **범위**입니다. Task 308은 일반 HLE 예외 제거를
시도해 progress `+1.64%`에 그쳤습니다. 이번에는 **하나의 인구, 우리가 만든 stub,
알려진 주소 구간**만 대상으로 합니다.

#### 4.1 gate stub을 opcode가 아니라 **주소**로 인식

gate stub은 `gate_code_base + first_gate_offset + ordinal * stride`에 있는 **우리가
합성한** 연속 구간입니다. opcode를 sniffing하지 않고 **주소 구간으로 판정**합니다.

* guest 코드에 우연히 있는 UD2를 잘못 잡을 수 없습니다.
* ordinal이 주소에서 바로 나오므로 stub 바이트를 다시 읽을 필요가 없습니다.
* 구간이 유효하지 않거나 plan이 없으면 기존 경로로 degrade합니다.

#### 4.2 SUPERBLOCK에서 분리

`enable_dbt_hle_dispatch`를 SUPERBLOCK과 분리해 **Glide gate 구간에만** 적용하는
별도 opt-in(`REPIU_AOT_GLIDE_GATE_DISPATCH`)을 만듭니다. SUPERBLOCK의 다른 효과
(연속 번역 등)는 켜지 않습니다. **이 분리 자체가 이번 작업의 핵심 산출물**이며, 실패해도
"예외 없는 dispatch를 독립 평가했다"는 결과가 남습니다.

#### 4.3 의미 보존 조건

* gate 진입 계수, `glide_gate_handled_count`, ordinal dispatch 순서, 인수 mirror,
  stdcall 정리(`ESP += 4 + 인수`), 반환 주소를 모두 유지합니다.
* dispatch 실패·미지원 ordinal은 **기존 INT3 fallback**으로 내려가며, 그 횟수를 셉니다.
* Task 365의 setter 생략과 **독립적으로** 동작해야 합니다(두 기능이 서로를 가리지
  않도록 A/B에서 생략은 고정합니다).
* 원본 EXE와 gate stub 이미지는 바꾸지 않습니다. 바꾸는 것은 **번역 결과**뿐입니다.

### 5. 1단계 — 구현 전 비용 분해 (필수)

Task 308의 재현을 피하기 위해 **구현 전에** UD2 예외 1회의 비용을 분해합니다.
기존 `ExecutionTimeScope` 구간을 재사용하고 새 clock을 만들지 않습니다.

| 구간 | 질문 |
|---|---|
| 커널 전이 | Task 336 가격이 지금도 유효한가 |
| VEH dispatch~gate 진입 | `HandleGlideGateBoundary` 도달까지의 비용 |
| gate 처리 | Task 353이 이미 잰 gate 구간 |

**이 값이 있어야 제거 이득을 사전에 계산할 수 있고, 사후에 "왜 안 늘었지"를 반복하지
않습니다.** Task 365와 366이 각각 그 경험이었습니다.

#### 5.1 결과 (2026-07-30 측정) — **구현 보류 판정**

기존 bucket만으로 계산했고 새 계측은 추가하지 않았습니다. Task 367의 60초 Release
실행(guest_run `162,845,067,020` cycle, Glide gate 진입 86,352회, 예외 302,000회)
기준입니다.

| 항목 | 호출당 cycle | 제거 가능 |
|---|---:|---|
| **gate 본체(`kGlideGate`)** | **357,657** | **아니오 — 일은 그대로 남음** |
| 커널 전이(Task 336 가격) | 34,521 | 예 |
| VEH prologue(예외당 평균) | 1,304 | 예 |
| AOT transfer resolve(예외당 평균) | 33,305 | 예 |
| **제거 가능 합계** | **69,129** | — |

**제거 가능 총량은 `5,969,451,726` cycle = wall의 3.67%이고 프레임 상한은 약
1.038배입니다.** 사전 등록 gate **B1(+5%)에 미달합니다.**

**구조적 이유:** Glide 호출 1회 비용의 **84%가 gate 본체**이고 예외 overhead는
**16.2%** 뿐입니다. 예외를 없애도 gate가 하는 일은 그대로 남습니다.

민감도 — gate boundary가 평균 transfer resolution의 N배를 낸다고 가정하면:

| 배수 | wall 대비 | 프레임 상한 |
|---:|---:|---:|
| 1.0x (평균 가정) | 3.67% | 1.038배 |
| 1.5x | 4.55% | 1.048배 |
| 2.0x | 5.43% | 1.057배 |
| 3.0x | 7.20% | 1.078배 |

**B1을 넘으려면 gate boundary가 평균의 2배 이상을 내야 합니다.** 그 값은 아래 §5.2에서
측정했습니다.

#### 5.2 그 불확실성을 측정함 — **결론 확정, 구현 안 함**

`kVehTotal` scope의 진입 timestamp를 저장하고 `kGlideGate` scope가 열릴 때의 차이를
누적했습니다. 두 timestamp 모두 기존 scope가 이미 읽던 값이므로 **새 clock read는
없습니다**(Task 353 규칙).

| 항목 | 값 |
|---|---:|
| gate prologue 총량 | 840,810,195 cycle |
| 표본 | 128,897회 |
| **호출당 실측** | **6,523 cycle** |
| clamped | 0 |

**§5.1의 가정이 10.6배 과대평가였습니다.** VEH 진입부터 gate scope까지의 실제 비용은
6,523 cycle이며, 이는 예외당 평균 transfer resolution(33,305)의 **0.20배**입니다.
**B1을 넘기 위해 필요했던 2배와 정반대 방향입니다.**

커널 전이는 `kVehTotal` scope **밖**(핸들러 진입 전)이므로 따로 더합니다.

| 전이 가격 | 호출당 제거 가능 | wall 대비 | 프레임 상한 |
|---|---:|---:|---:|
| 34,521 (Task 336) | 41,044 | **3.25%** | **1.034배** |
| 50,401 (+46% 상단) | 56,924 | 4.51% | 1.047배 |

**전이 가격을 세션 변동폭 최상단으로 잡아도 B1(+5%)에 미달합니다.** 추정이 아니라
측정으로 확정됐으므로 **구현하지 않습니다.**

§5.1의 중앙 추정 3.67%와 실측 3.25%는 가깝지만, 구성은 크게 달랐습니다. 결론이 같은
것은 우연이며, gate가 평균보다 훨씬 **싸다**는 사실이 그 이유입니다.

**따라서 §6의 사전 등록에 따라 구현 전에 멈추고 보고합니다.** 중앙 추정 1.038배는
번역기 옵션 경로를 건드리는 회귀 위험에 비해 작습니다.

**정정 — 탄력성 -2.4는 쓸 수 없습니다.** Task 367 로그가 Task 366의 한 쌍에서 유도한
탄력성 약 -2.4를 인용했으나, **Task 366 자신의 결론이 그 프레임 손실의 원인을 예외
횟수가 아니라 safe point 상시 arming으로 지목했습니다.** 두 기전이 섞인 값이므로
예외 제거 이득 추정에 쓸 수 없습니다. 이 비용 모델(3.67%)이 더 타당하며, 두 값이
크게 어긋난다는 사실 자체가 탄력성 인용이 틀렸다는 증거입니다.

### 6. 사전 등록 판정

**Task 308 선례를 명시적으로 반영합니다.** Task 308은 progress `+1.64%`로 5배 gate에
실패했습니다. 그때와 다른 점은 (a) 대상이 일반 HLE가 아니라 단일 최대 인구이고,
(b) 축이 3.5배 빨라졌으며, (c) 지표가 `progress`가 아니라 프레임이라는 것입니다.

* **B1 채택:** 프레임 3회 중앙값 **+5% 이상** 이고 의미 gate 전부 통과 → 기본값 전환을
  검토합니다.
* **B2 부분:** 프레임 +5% 미만이지만 예외가 예측대로 감소 → 기능은 opt-in으로 남기고,
  1단계 비용 분해와 실측 차이를 기록해 **예외 축 전체를 재판정**합니다.
* **B3 기각:** 예외는 줄었는데 프레임이 감소 → Task 366과 같은 형태이므로 되돌리고
  원인(경계 기계 자체)을 다음 대상으로 삼습니다.
* **B4 불가:** gate 구간 인식이나 SUPERBLOCK 분리가 의미를 보존하지 못함 → 구현을
  중단하고 감사 결과만 남깁니다.

### 7. 검증 계약

| gate | 기준 |
|---|---|
| C1 예외 감소 | UD2 계열 예외가 dispatch 성공 수만큼 감소 |
| C2 호출 보존 | gate 진입/handled/ordinal별 호출 수가 OFF/ON에서 동일 범위 |
| C3 ABI | `completed ordinal <= handled gate`, ESP/반환 주소 계약 유지 |
| C4 안정성 | malformed/fatal/implementation issue/fallback 이유별 계수 기록, fatal 0 |
| C5 시각 | Task 365의 swap별 통계 phase offset 대응으로 렌더 시퀀스 동일성 확인 |
| C6 성능 | 프레임 3회 중앙값과 예외 축을 함께 보고 |
| C7 EEPROM | 격리 seed, 실행 후 hash 일치 |

### 8. 금지 사항

* 원본 EXE, gate stub 이미지, 렌더링 의미를 바꾸지 않습니다.
* SUPERBLOCK의 다른 효과를 함께 켜지 않습니다. 분리가 목적입니다.
* opcode sniffing으로 gate를 인식하지 않습니다(주소 구간만).
* fallback 경로를 제거하지 않습니다. dispatch 실패는 반드시 기존 경로로 갑니다.
* 프레임 수치를 위해 gate 진입을 생략하거나 합치지 않습니다.

---

## English

### Objective

Remove the largest exception population Task 367 identified: the Glide gate trap
(`0F 0B`, UD2) at 55.21% of boundary samples and about 24.9% of all exceptions, one
per Glide API call. Only the trap mechanism changes; the guest calling convention
and rendering semantics do not.

### The current mechanism

`BuildGlideGatePlan` synthesises a five-byte stub per export ordinal: `0F 0B`
(UD2), the ordinal, then `C3`. A guest Glide call jumps there, UD2 raises, and
`HandleGlideGateBoundary` recovers the gate offset from EIP, dispatches, and
restores EIP and ESP.

### Three audit findings that shape the work

**The exception-free machinery already exists.** `EmitHleDispatchSlot` pushes a
dispatch address and the guest address, jumps to a thunk, and leaves an INT3
fallback, and Task 308 already validated that this family of host-call thunks
preserves GPR/EFLAGS, x87/MMX/SSE, and host stack/TIB boundaries across a
60-second run with zero exceptions, zero fallbacks, and a matching EEPROM. Nothing
new needs building.

**It is bolted to `REPIU_AOT_DBT_SUPERBLOCK`.** `enable_dbt_hle_dispatch` is true
only when the backend is `aot-dbt` *and* superblock is on — and superblock
currently breaks rendering, so **exception-free dispatch has never been evaluated
on its own**. The logs confirm it: superblock HLE dispatch disabled, every host
dispatch counter zero.

**Even with the flag on it would not reach the Glide gate.** `IsHleBoundary`
recognises only privileged or segment-attributed instructions; UD2 is neither, so
it falls into `kOther` — which the boundary reason census confirms — and no
dispatch slot can be emitted for it.

### Design

What separates this from Task 308, which attempted general HLE exception removal
and gained 1.64%, is **scope**: one population, a stub we synthesised ourselves, at
an address range we control.

Gate stubs are recognised **by address**, not by opcode: they occupy a contiguous
region at `gate_code_base + first_gate_offset + ordinal * stride`. That cannot
misfire on a UD2 that happens to exist in guest code, yields the ordinal directly
from the address, and degrades to the existing path when the plan or range is
unavailable. Exception-free dispatch is then decoupled from superblock behind its
own opt-in applying **only to that range**, without enabling superblock's other
effects. **That decoupling is the deliverable even if the performance result is
negative**, because it is what finally evaluates exception-free dispatch alone.

Gate entry counts, handled counts, dispatch order, the argument mirror, the stdcall
cleanup, and the return address are all preserved; failed or unsupported dispatch
falls back to the existing INT3 path with the reason counted; and the original
executable and stub image are untouched — only the translation changes.

### Stage one: cost decomposition before implementing

To avoid repeating Task 308, the per-exception cost is decomposed **before**
implementing, reusing existing `ExecutionTimeScope` intervals rather than adding
clocks: the kernel transition (is Task 336's price still valid), the VEH dispatch
up to gate entry, and the gate interval Task 353 already measures. Without this the
gain cannot be predicted, and Tasks 365 and 366 were both cases of discovering that
after the fact.

### Pre-registered decisions

Task 308's precedent is written in deliberately: it failed its gate at 1.64% on
`progress`. What differs now is that the target is the single largest population
rather than HLE in general, the axis is 3.5x faster, and the metric is frames.
**B1** adopts on a three-run median frame gain of 5% or more with all semantic
gates passing. **B2**, exceptions falling as predicted without the frames,
keeps the feature opt-in and re-judges the whole exception axis against the stage-one
cost model. **B3**, exceptions down and frames down, mirrors Task 366 and reverts,
making the boundary machinery itself the next target. **B4** stops implementation
if address recognition or the superblock decoupling cannot preserve semantics,
leaving the audit as the result.

### Gates and prohibitions

C1 exceptions falling by the dispatch success count; C2 gate entry, handled, and
per-ordinal call counts unchanged; C3 the ABI and ESP/return contract; C4 zero
fatal with per-reason fallback counts recorded; C5 the Task 365 phase-offset
sequence-identity check; C6 three-run median frames reported with the exception
axis; C7 EEPROM isolation. The work changes no original executable, stub image, or
rendering semantics, enables no other superblock effect, recognises gates only by
address, never removes the fallback path, and never elides or merges a gate entry
to improve a number.
