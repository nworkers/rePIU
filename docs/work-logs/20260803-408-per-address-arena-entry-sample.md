# 20260803-408 주소별 arena 진입 표본 작업 로그 / Per-Address Arena Entry Sample Work Log

설계: [20260803-408](../design/20260803-408-per-address-arena-entry-sample.md)

작업 지시: [20260803-408](../work-orders/20260803-408-per-address-arena-entry-sample.md)

## 한국어

### 작업 요약

Task 407이 놓친 것을 확보했습니다. **정상 모드에서 `0x0301DB22`의 arena 진입 신호가
4회 실행 모두 동일**하며, 그 위치는 **INT 8 타이머 핸들러 진입부**입니다.

### 0. 정정 — 아래 1절의 결론은 과했습니다 (같은 날 확인)

**"지연 루프는 타이머 핸들러 안에서 진입한다"는 첫 표본 1건에만 근거한 것이며,
모집단을 대표하지 못합니다.** 산술이 반박합니다.

| run | single-step 예외 총수 | `0x0301DB22` 진입 |
|---|---:|---:|
| 1 | 283 | 3,124 |
| 2 | 260 | 2,018 |
| 3 | 265 | 2,430 |
| 4 | 260 | 2,403 |

single-step 예외가 실행 전체에서 260~283회뿐이므로 **진입 2,018~3,124회 중 최대 약
10%만** 직전이 single-step일 수 있습니다. 나머지는 breakpoint나 access violation입니다.

설계 §"첫 1건이 맞는 선택인 이유"가 예고한 실패 모드 그대로입니다: 기전이 섞여 있으면
첫 표본과 횟수만으로는 판정할 수 없습니다. 거기 적어둔 규칙("그 경우 이 계측으로는
판정할 수 없다고 기록하고 표본 수를 늘린다")에 따라 **Task 409에서 주소별 직전 예외
4분류 히스토그램을 추가**합니다.

1절은 **첫 진입 1건의 사실**로만 읽어야 하며, 2절의 `0x0301F7CE` 해석도 그 1건에
대해서만 성립합니다.

### 1. 첫 진입 표본: 타이머 핸들러 진입부 (모집단 대표성 없음)

격리가 없는 4회 실행(`quarantines: 0`) 전부 같은 값입니다.

| run | 프레임 | `0x0301DB22` count | 진입 횟수 | prev-code | prev-eip | flags |
|---|---:|---:|---:|---|---|---|
| 1 | 188 | 1,061,800 | 3,124 | `0x80000004` | `0x0301F7CE` | `0x00` |
| 2 | 1 | 90,415 | 2,018 | `0x80000004` | `0x0301F7CE` | `0x00` |
| 3 | 1 | 136,081 | 2,430 | `0x80000004` | `0x0301F7CE` | `0x00` |
| 4 | 1 | 46,987 | 2,403 | `0x80000004` | `0x0301F7CE` | `0x00` |

`flags = 0x00`은 **직전 예외가 캐시 밖, TF 꺼짐, 재진입 예약 없음, legacy 아님,
single-step trace 꺼짐**을 뜻합니다.

### 2. `0x0301F7CE`의 정체

파일 offset `0x2A9CE`이고 그 앞이 이렇습니다.

```
0x0301F7CB  fa                    cli
0x0301F7CC  31 d2                 xor  edx,edx
0x0301F7CE  83 ba 98 ec 34 00 00  cmp  dword [edx+0x34EC98],0   <- prev-eip
```

**`CLI` 바로 다음 명령입니다.** `CLI`는 privileged 명령이라 HLE가 처리하고 EIP를
전진시키며, 그 다음 명령에서 single-step 예외가 납니다. 그 예외 처리가 **TF를 끄고
arena에 그대로 재개**하며, 이후 게스트는 타이머 핸들러 전체를 arena에서 자유 실행하고
그 안의 200회 지연 루프가 `IN`마다 fault를 냅니다.

`0x0301F827`(Task 407 신호 b의 AV 위치)도 같은 루틴 안이며 89바이트 뒤입니다.
**두 신호는 같은 INT 8 핸들러의 서로 다른 지점입니다.**

### 3. 주소별로 신호가 다릅니다 — 전역 버퍼로는 불가능했던 관측

| 주소 | count | 진입 | prev-code | prev-eip | 위치 |
|---|---:|---:|---|---|---|
| `0x0301DB22` | 1,061,800 | 3,124 | `0x80000004` | `0x0301F7CE` | arena |
| `0x030D0A1A` | 10,404 | **10,404** | `0x80000003` | `0x0C288DFB` | **캐시** |
| `0x0301F851` | 5,311 | 416 | `0xC0000005` | `0x0301F827` | arena |
| `0x0301EDC6` | 2,657 | 8 | `0x80000003` | `0x0C3F51D2` | **캐시** |
| `0x0301DB90` | 1,328 | 6 | `0x80000004` | `0x0301F7CE` | arena |

`0x030D0A1A`는 **진입 횟수가 count와 같습니다**(10,404/10,404). 매 실행이 캐시 INT3
직후이며, 캐시와 arena를 매번 왕복합니다. 반대로 `0x0301DB22`는 340:1입니다.
**주소마다 기전이 다르므로 전역 ring으로는 원리적으로 판정할 수 없었습니다.**

### 4. 설계의 예측이 틀렸습니다 — 정정합니다

설계 §판정 기준은 진입:count 비를 **약 1:200**으로 예상했습니다(루프가 200회 반복).
실측은 **1:19.6 ~ 1:340**으로 실행마다 17배 흔들립니다.

원인은 모델이 틀린 것입니다. 한 번 arena에 들어가면 게스트는 **여러 번의 지연 호출을
연속으로** 수행할 수 있고, 그 동안 직전 예외가 계속 `0xC0000096`이라 진입으로 세지지
않습니다. 즉 진입 횟수는 "지연 루프 호출 수"가 아니라 **"arena 체류 횟수"**입니다.
진입 횟수가 실행마다 2,018~3,124로 좁은 것도 이 해석과 맞습니다(체류는 타이머 주기에
묶이고, 체류당 읽기 수는 장면에 따라 다름).

### 5. 검증

* Release 빌드 성공, `repiu_aot_probe` 종료 코드 0.
* 격리 없는 실행 4회 확보(작업 지시의 최소 1회 조건 충족).
* 동작 불변: Task 405/406/407 값이 그대로입니다(`cache` 0, `reentry` 0, 최다 주소 동일).
* pumpit1 45초 **980 프레임**. 오늘 범위(700~838)보다 높으나 회귀는 아닙니다.

### 6. 다음 대상

**`0x0301F7CE`의 single-step 예외를 처리하면서 TF를 끄고 arena에 남기는 곳이
어디인가.** `aot_runtime_dispatch.cpp:1866~1913`의 세 분기 중 어느 것도 관측된 상태
(TF 꺼짐 + legacy 꺼짐 + arena EIP)와 맞지 않으므로, 다른 핸들러가 이 예외를 먼저
소비하고 있습니다. 그 지점을 찾으면 wall의 약 50%가 걸린 사슬의 시작점이 확정됩니다.

부수적으로, `CLI` 다음에 왜 single-step이 나는지도 함께 봐야 합니다 — privileged
명령 HLE가 TF를 켜 두는지, 아니면 이전 상태가 남은 것인지.

### 7. 미확정

* 위 6번.
* 재번역이 요청 진입 주소를 address map에 남기지 못하는 조건(Task 404 이월).
* 격리 발생 조건.

---

## English

### Summary

Captured what Task 407 missed: **the healthy-mode arena-entry signature for `0x0301DB22` is
identical across four runs**, and it sits at the **INT 8 timer handler prologue**.

### 0. Correction — section 1's conclusion overreached (found the same day)

**"The delay loop is entered inside the timer handler" rests on a single first sample and does
not describe the population.** The arithmetic refutes it: single-step exceptions totalled 283,
260, 265, and 260 across the four runs, while `0x0301DB22` had 3,124, 2,018, 2,430, and 2,403
entries. **At most about a tenth** of those entries can have had a single-step predecessor; the
rest followed a breakpoint or an access violation.

This is exactly the failure mode the design named under "why one sample is the right choice" —
if mechanisms are mixed, a first sample plus a count cannot decide. Following the rule written
there, **Task 409 adds a four-way predecessor histogram per address**.

Section 1 should be read as a fact about the *first* entry only, and section 2's reading of
`0x0301F7CE` holds only for that one.

### 1. First entry sample: the timer handler prologue (not representative of the population)

All four runs were quarantine-free and agree exactly:

| Run | Frames | `0x0301DB22` count | Entries | prev-code | prev-eip | flags |
|---|---:|---:|---:|---|---|---|
| 1 | 188 | 1,061,800 | 3,124 | `0x80000004` | `0x0301F7CE` | `0x00` |
| 2 | 1 | 90,415 | 2,018 | `0x80000004` | `0x0301F7CE` | `0x00` |
| 3 | 1 | 136,081 | 2,430 | `0x80000004` | `0x0301F7CE` | `0x00` |
| 4 | 1 | 46,987 | 2,403 | `0x80000004` | `0x0301F7CE` | `0x00` |

`flags = 0x00` means the previous exception was outside the cache, with the trap flag clear,
no re-entry scheduled, no legacy fallback, and no single-step trace.

### 2. What `0x0301F7CE` is

At file offset `0x2A9CE`, immediately after a `CLI`:

```
0x0301F7CB  fa                    cli
0x0301F7CC  31 d2                 xor  edx,edx
0x0301F7CE  83 ba 98 ec 34 00 00  cmp  dword [edx+0x34EC98],0   <- prev-eip
```

`CLI` is privileged, so HLE emulates it and advances EIP; a single-step exception then fires on
the following instruction, and **that handler clears the trap flag and resumes in the arena**.
From there the guest free-runs the whole timer handler natively, and the 200-iteration delay
loop inside it faults once per `IN`. `0x0301F827` — the access-violation address of Task 407's
signature (b) — is 89 bytes further into the same routine, so **both signatures are different
points in the same INT 8 handler**.

### 3. Signatures differ per address, which a global buffer could not have shown

| Address | count | Entries | prev-code | prev-eip | Where |
|---|---:|---:|---|---|---|
| `0x0301DB22` | 1,061,800 | 3,124 | `0x80000004` | `0x0301F7CE` | arena |
| `0x030D0A1A` | 10,404 | **10,404** | `0x80000003` | `0x0C288DFB` | **cache** |
| `0x0301F851` | 5,311 | 416 | `0xC0000005` | `0x0301F827` | arena |
| `0x0301EDC6` | 2,657 | 8 | `0x80000003` | `0x0C3F51D2` | **cache** |
| `0x0301DB90` | 1,328 | 6 | `0x80000004` | `0x0301F7CE` | arena |

`0x030D0A1A` has **as many entries as executions** (10,404 of 10,404): every one follows a cache
INT3, so it crosses between cache and arena each time. `0x0301DB22` is 340 to 1. The mechanisms
genuinely differ per address, which is why a global ring could not decide this.

### 4. The design's prediction was wrong — corrected

The decision rule expected an entry-to-count ratio near **1:200**, since the loop runs 200
iterations. Measured, it is **1:19.6 to 1:340**, varying seventeen-fold across runs.

The model was wrong: once in the arena the guest can perform **several consecutive delay calls**,
and throughout them the previous exception stays `0xC0000096`, so they are not counted as
entries. Entries therefore count **arena residencies**, not delay-loop calls — which also
explains why entries stay in a narrow 2,018-3,124 band while read counts vary twenty-fold.

### 5. Verification

Release build passed and the probe exited zero. Four quarantine-free runs were obtained, above
the work order's minimum of one. Behaviour is unchanged: the Task 405/406/407 values are
identical (`cache` zero, `reentry` zero, same top address). pumpit1 rendered **980 frames**,
above today's 700-838 band but not a regression.

### 6. Next target

**Which handler consumes the single-step at `0x0301F7CE`, clears the trap flag, and leaves
execution in the arena.** None of the three branches at `aot_runtime_dispatch.cpp:1866-1913`
matches the observed state — trap flag clear, legacy clear, EIP in the arena — so something
else takes that exception first. Finding it fixes the head of the chain that holds roughly half
of wall clock. Alongside it, why a single step follows the `CLI` at all deserves checking:
whether the privileged-instruction HLE leaves the trap flag set, or whether it is left over
from an earlier state.

### 7. Unresolved

The item above; the condition under which a re-translation omits its requested entry from the
address map (carried from Task 404); and what decides whether quarantine fires.
