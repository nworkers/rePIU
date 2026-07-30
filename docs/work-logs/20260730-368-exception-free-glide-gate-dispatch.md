# 20260730-368 예외 없는 Glide gate dispatch 작업 로그 / Work log

* 설계: [20260730-368-exception-free-glide-gate-dispatch.md](../design/20260730-368-exception-free-glide-gate-dispatch.md)
* 근거: [Task 367 작업 로그](20260730-367-hle-boundary-opcode-attribution.md)
* **결과: 구현하지 않음.** 1단계 비용 분해가 사전 등록 gate에 미달했고, 그 유일한
  불확실성을 측정으로 해소한 뒤에도 미달했습니다.

## 한국어

### 결론

**예외 없는 Glide gate dispatch의 제거 상한은 wall의 3.25%, 프레임 약 1.034배입니다.**
전이 가격을 세션 변동폭 최상단(+46%)으로 잡아도 4.51%, 1.047배로 사전 등록
**B1(+5%)에 미달합니다.** 추정이 아니라 측정입니다.

### 왜 작은가 — 구조

Glide 호출 1회의 비용 구조입니다.

| 구간 | 호출당 cycle | 예외 제거로 사라지는가 |
|---|---:|---|
| **gate 본체 (`kGlideGate`)** | **약 235,000** | **아니오** |
| 커널 전이 (VEH scope 밖) | 34,521 | 예 |
| VEH 진입 → gate scope (실측) | **6,523** | 예 |

**gate가 하는 일 자체가 비용의 대부분이고, 예외로 도달하는 비용은 그 위의 얇은
층입니다.** 예외를 없애도 rendezvous·OpenGL·ordinal dispatch는 그대로 남습니다.

### 측정 방법 — 새 clock 없이

`kVehTotal` scope의 진입 timestamp를 profile에 저장하고, `kGlideGate` scope가 열릴 때
그 차이를 누적했습니다. **두 timestamp 모두 기존 scope가 이미 읽던 값이므로 새 clock
read를 만들지 않았습니다**(Task 353 규칙). 커널 전이는 `kVehTotal` scope 바깥(핸들러
진입 전)이므로 Task 336 가격으로 따로 더했습니다.

| 항목 | 값 |
|---|---:|
| gate prologue 총량 | 840,810,195 cycle |
| 표본 | 128,897회 |
| **호출당** | **6,523 cycle** |
| clamped | 0 |

### 사전 추정이 틀린 방향 — 기록

1단계(§5.1)는 gate boundary가 예외당 평균 transfer resolution(33,305 cycle)만큼
낸다고 가정해 호출당 34,609 cycle을 잡았습니다. **실측은 6,523으로 10.6배 과대평가**
였고, 평균의 **0.20배**입니다.

B1을 넘으려면 평균의 **2배 이상**이어야 했는데 실제는 **0.2배**입니다. 즉 이 작업을
살릴 수 있었던 유일한 가정이 **정반대 방향으로 기각**됐습니다.

중앙 추정 3.67%와 실측 3.25%가 가까운 것은 우연입니다. 구성이 크게 달랐고, 결론이
같아진 이유는 gate가 평균보다 훨씬 **싸기** 때문입니다. **추정이 우연히 맞았다는
사실이 추정을 신뢰할 근거가 되지는 않습니다.**

### 감사로 남긴 사실 (구현하지 않아도 유효)

* **예외 없는 dispatch 기계는 이미 존재합니다.** `EmitHleDispatchSlot`이 dispatch
  주소와 guest 주소를 push하고 thunk로 `JMP rel32`하며 INT3 fallback을 남깁니다.
* **그 기계는 `REPIU_AOT_DBT_SUPERBLOCK`에 묶여 있습니다.** SUPERBLOCK은 렌더링을
  중단시키므로, **예외 없는 dispatch는 지금까지 독립적으로 평가된 적이 없습니다.**
* **지금 상태로는 flag를 켜도 Glide gate엔 적용되지 않습니다.** `IsHleBoundary`가
  privileged/segment 속성만 boundary로 보는데 UD2는 둘 다 아니라 `kOther`로
  떨어집니다.
* gate stub은 `gate_code_base + first_gate_offset + ordinal * stride`의 연속 구간
  이므로, 필요해지면 **opcode가 아니라 주소로** 인식하는 것이 옳습니다.

### 검증

* 계측만 추가했고 동작은 바꾸지 않았습니다. 새 clock read 0.
* `scripts/build_win32_x86_release.bat`: 통과
* clamped 0 — 두 timestamp의 순서가 항상 옳았습니다.
* `VERSION`: `0.0.113` 유지

### 미확정

* segment register move(20.11%)와 port I/O(13.15%) 인구의 제거 가능성. 다만 이번
  결과가 **예외 제거 일반에 대한 경고**입니다. gate 본체가 지배한다면 다른 인구도
  같은 구조일 수 있습니다.
* safe-point 상시 arming의 단독 비용(Task 366).
* LFB 없는 gameplay 장면의 축.

### 다음 작업 제안

**예외 축은 여기서 닫습니다.** 최대 인구(55.21%)를 제거해도 1.034배이므로, 나머지
작은 인구는 더 작습니다. Task 336의 상한 재계산(1.07배)과도 일치합니다.

**진짜 덩어리는 gate 본체입니다** — 호출당 약 235,000 cycle, wall의 18.7%. Task 365가
그중 rendezvous를 건드렸고 Glide 비중을 5.13%p 내렸지만 프레임은 안 늘었습니다.
ordinal별로 다시 쪼개면 이 장면에서 무엇이 지배적인지 나옵니다(LFB lock이 유력).

다만 **이 장면 기준으로 세 번 연속(365·366·368) "이득이 작다"가 나왔고, 정작 문제가
보고된 gameplay 장면은 아직 한 번도 측정되지 않았습니다.** 사용자 캡처가 다음 대상
선정의 가장 큰 정보입니다.

---

## English

### Result

**Exception-free Glide gate dispatch bounds at 3.25% of wall, about 1.034x on
frames** — 4.51% and 1.047x even with the transition price at the top of its 46%
session variance, so it misses the pre-registered B1 gate of +5%. This is measured,
not estimated, and the work was therefore **not implemented**.

### Why it is small

Per Glide call, the gate body costs roughly 235,000 cycles, the kernel transition
34,521, and the measured path from VEH entry to the gate scope 6,523. **The gate's
actual work dominates and the cost of reaching it by exception is a thin layer on
top**; removing the exception leaves the rendezvous, OpenGL, and ordinal dispatch
untouched.

### Method, with no new clock

The `kVehTotal` scope's entry timestamp is stored and differenced against the
`kGlideGate` scope's opening timestamp. **Both were already being read by existing
scopes, so no clock read was added** (Task 353's rule). The kernel transition sits
outside the `kVehTotal` scope, before the handler runs, so it is added separately at
Task 336's price. Measured: 840,810,195 cycles over 128,897 samples, 6,523 per call,
zero clamped.

### Recording that the estimate was wrong, and in which direction

Stage one assumed a gate boundary pays the per-exception average transfer resolution
of 33,305 cycles, giving 34,609 per call. The measurement is 6,523 — a **10.6x
overestimate**, and **0.20x the average**. Clearing B1 needed 2x or more, so the one
assumption that could have rescued this work is **refuted in the opposite
direction**.

The estimate's 3.67% and the measurement's 3.25% being close is a coincidence: the
components differed greatly, and they agree only because the gate is far *cheaper*
than average. **An estimate happening to land near the measurement is not grounds
for trusting the estimate.**

### Audit facts that stand regardless

The exception-free machinery already exists in `EmitHleDispatchSlot`; it is bolted
to `REPIU_AOT_DBT_SUPERBLOCK`, which breaks rendering, so it has never been
evaluated on its own; and even enabled it would not reach the Glide gate, because
`IsHleBoundary` recognises only privileged or segment-attributed instructions while
UD2 is neither. Should this be revisited, gate stubs occupy a contiguous range at
`gate_code_base + first_gate_offset + ordinal * stride` and should be recognised by
address rather than opcode.

### Next

**The exception axis closes here.** Removing the largest population, 55.21% of
boundary samples, buys 1.034x, so the smaller populations buy less — consistent with
Task 336's recomputed 1.07x bound.

The real mass is the gate body at roughly 235,000 cycles per call and 18.7% of wall.
Task 365 attacked its rendezvous and cut the Glide share 5.13 points without moving
frames; splitting it by ordinal would show what dominates in this scene, LFB lock
being the likely answer. But this scene has now produced "the gain is small" three
times running (365, 366, 368) while the gameplay scene the problem was reported from
has never been measured, so the user's capture is the largest single piece of
information for choosing what comes next.
