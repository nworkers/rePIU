# 20260803-409 arena 진입 직전 예외 히스토그램 설계 / Arena Entry Predecessor Histogram Design

선행: [Task 408 설계](20260803-408-per-address-arena-entry-sample.md)

## 한국어

### 배경

Task 408은 주소별로 **진입 전이 첫 1건**만 보관하고 나머지는 횟수만 셌습니다. 그리고
그 1건을 근거로 "지연 루프는 타이머 핸들러 안에서 진입한다"를 결론으로 적었습니다.

**산술이 그 결론을 반박합니다.**

| run | single-step 예외 총수 | `0x0301DB22` 진입 |
|---|---:|---:|
| 1 | 283 | 3,124 |
| 2 | 260 | 2,018 |
| 3 | 265 | 2,430 |
| 4 | 260 | 2,403 |

single-step이 실행 전체에서 260~283회뿐이므로, 진입 2,018~3,124회 중 **최대 약 10%**만
직전이 single-step일 수 있습니다. 4회 실행이 모두 같았던 것은 "첫 진입 지점이 항상
같다"는 뜻이지 모든 진입이 그렇다는 뜻이 아닙니다.

Task 408 설계는 이 실패 모드를 이미 예고했습니다 — "기전이 섞여 있다면
`entry_transition_count`와 첫 표본만으로는 알 수 없으므로, 그 경우 이 계측으로는
판정할 수 없다고 기록하고 표본 수를 늘립니다." 이 Task가 그 규칙의 집행입니다.

### 설계

전체 표본을 보관하지 않고 **분류 계수기 4개**만 둡니다. 진입 전이 1건마다 직전 예외
코드를 네 가지로 나눠 셉니다.

```
std::uint32_t entry_prev_single_step;        // 0x80000004
std::uint32_t entry_prev_breakpoint;         // 0x80000003
std::uint32_t entry_prev_access_violation;   // 0xC0000005
std::uint32_t entry_prev_other;              // 그 외
```

* Task 408의 첫 표본 필드(`entry_previous_code/eip/flags`)는 **예시로 남깁니다.**
  분포는 히스토그램이, 구체적 상태는 표본이 답하므로 서로 대체하지 않습니다.
* 기록 위치는 Task 408과 같은 `ApplyPortIoEntrySample` 한 곳이며 `switch` 하나가
  전부입니다. 주소는 이미 조회된 뒤이므로 추가 탐색이 없습니다. **상시 ON.**

### 왜 4분류인가

관측된 예외 인구가 네 종류이기 때문입니다(single-step, breakpoint, access violation,
그리고 port I/O를 포함한 privileged). 진입 판정이 이미 `0xC0000096`을 제외하므로
`other`에 남는 것은 드문 종류이며, 크면 분류를 늘려야 한다는 신호입니다.

### 판정 기준

| 관측 | 결론 |
|---|---|
| 한 분류가 지배적 | 그 예외 경로의 복귀 처리를 본다 |
| 분류가 섞임 | 진입 기전이 여럿이므로 각각 따로 추적 |
| 진입 횟수가 실행마다 크게 다름 | **분포 비교 전에 그 편차부터 설명해야 한다** |

마지막 행은 Task 408에서 이미 진입 횟수가 실행 간 17배 흔들린 것을 보고 넣었습니다.

### 검증

* Release 빌드와 `repiu_aot_probe` 통과.
* pumpit3 45초 3회 이상, 격리 없는 실행 포함.
* Task 405/406/407/408 값 불변 확인.
* pumpit1 회귀 1회.

### 이 Task가 하지 않는 것

동작을 바꾸지 않습니다. 진입을 막거나 복귀를 추가하지 않습니다.

---

## English

### Background

Task 408 kept only the **first entry transition** per address and counted the rest, then drew a
conclusion from that single sample. The arithmetic refutes it: single-step exceptions totalled
260-283 per run against 2,018-3,124 entries at `0x0301DB22`, so **at most about a tenth** could
have had a single-step predecessor. Four identical runs meant the first entry point is always
the same, not that every entry is.

Task 408's own design named this failure mode — if mechanisms are mixed, a count plus one
sample cannot show it, and the rule was to record that the instrument cannot decide and raise
the sample count. This task executes that rule.

### Design

Rather than keeping every sample, add **four classification counters** per address for the
predecessor exception code: single step (`0x80000004`), breakpoint (`0x80000003`), access
violation (`0xC0000005`), and other. Task 408's first-sample fields stay as an example, since
the histogram answers distribution while the sample answers concrete state, and neither
substitutes for the other. Recording happens at the same single site with one `switch`, after
the address is already resolved, so there is no extra search. **Always on.**

### Why four classes

The observed exception population has four kinds, and because the entry test already excludes
`0xC0000096`, anything landing in `other` is rare — a large `other` is itself the signal that
the classification needs extending.

### Decision rule

One dominant class points at that exception path's resumption handling; a mixed distribution
means several entry mechanisms to track separately; and if entry counts differ sharply between
runs, **that variation must be explained before comparing distributions at all** — added
because Task 408 already saw entry counts swing seventeen-fold across runs.

### Verification

Release build and probe; at least three 45-second pumpit3 runs including quarantine-free ones;
Task 405/406/407/408 values unchanged; and one pumpit1 regression run.

### Out of scope

No behaviour change, nothing blocking entry, no return path.
