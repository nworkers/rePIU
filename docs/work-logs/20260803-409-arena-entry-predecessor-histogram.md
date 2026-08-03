# 20260803-409 arena 진입 직전 예외 히스토그램 작업 로그 / Arena Entry Predecessor Histogram Work Log

설계: Task 408 설계 [20260803-408](../design/20260803-408-per-address-arena-entry-sample.md)
§"첫 1건이 맞는 선택인 이유"가 예고한 실패 모드에 대한 후속 조치입니다.

## 한국어

### 작업 요약

Task 408이 첫 표본 1건으로 모집단을 판정한 오류를 고치기 위해 **주소별 직전 예외
4분류 히스토그램**을 추가했습니다. 다른 주소들은 깨끗하게 갈렸지만, **정작 목표였던
`0x0301DB22`는 아직 확정하지 못했습니다.** 이유는 4절에 있습니다.

### 1. Task 408 오류의 산술적 근거

| run(Task 408) | single-step 예외 총수 | `0x0301DB22` 진입 |
|---|---:|---:|
| 1 | 283 | 3,124 |
| 2 | 260 | 2,018 |
| 3 | 265 | 2,430 |
| 4 | 260 | 2,403 |

single-step이 실행 전체에서 260~283회뿐이므로 진입의 **최대 약 10%**만 직전이
single-step일 수 있었습니다. "4회 실행 모두 동일"은 **첫 진입 지점이 항상 같다**는
뜻이지 모든 진입이 그렇다는 뜻이 아니었습니다.

### 2. 히스토그램은 다른 주소에서 정상 동작합니다

Task 409 실행 3회(모두 격리 없음)입니다.

| 주소 | count | 진입 | step | **bp** | **av** | other | 지배 |
|---|---:|---:|---:|---:|---:|---:|---|
| `0x030D0A1A` | 10,404 | 10,404 | 0~4 | **10,249~10,319** | 85~155 | 0 | breakpoint |
| `0x0301F851` | 4,732 | 359 | 0 | 1 | **358** | 0 | access violation |
| `0x030D0A0F` | 1,152 | 1,152 | 0 | **1,152** | 0 | 0 | breakpoint |

`0x030D0A1A`와 `0x030D0A0F`는 **진입 횟수가 count와 같습니다.** 매 실행이 캐시 INT3
직후이며 캐시와 arena를 매번 왕복합니다. `0x0301F851`은 AV가 지배합니다.
**주소마다 기전이 다르다는 Task 408의 관찰은 유지되며, 이제 분포로 뒷받침됩니다.**

### 3. `0x0301DB22`는 이번 3회에서 진입이 **1회**입니다

| run | 프레임 | count | 진입 | step/bp/av/other |
|---|---:|---:|---:|---|
| 1 | 134 | 946,114 | **1** | 1/0/0/0 |
| 2 | 73 | 889,039 | **1** | 1/0/0/0 |
| 3 | 1 | 42,906 | **1** | 1/0/0/0 |

이 실행들에서는 **첫 표본이 곧 모집단 전체**입니다. 게스트가 이 주소에서 arena 체류에
한 번 들어간 뒤 실행이 끝날 때까지 나오지 않았고, 직전 예외는 세 번 모두
`0x0301F7CE`(CLI 다음 명령)의 single-step입니다. **이 실행들에 한해 Task 408의 해석은
옳습니다.**

### 4. 그런데 확정할 수 없습니다 — 진입 횟수가 실행 간 3자릿수로 흔들립니다

| 측정 | `0x0301DB22` 진입 |
|---|---:|
| Task 408, 4회 | 2,018 ~ 3,124 |
| Task 409, 3회 | **1** |

**같은 판정식인데 세 자릿수 차이입니다.** Task 409 빌드의 변경은 히스토그램 카운터
추가뿐이므로 동작 차이가 아니라 **실행 간 차이**입니다. 그리고 Task 408 실행들의 분류
분포는 측정되지 않았습니다(그때는 히스토그램이 없었음).

가능한 해석은 "다른 예외가 지연 루프 사이에 끼어드는 빈도가 장면에 따라 다르다"는
것이지만, **측정으로 확인하지 않았으므로 추정입니다.** 진입이 1회인 실행과 3,000회인
실행이 무엇이 다른지 모르는 채로 `0x0301DB22`의 진입 기전을 확정할 수 없습니다.

### 5. 검증

* Release 빌드 성공, `repiu_aot_probe` 종료 코드 0.
* 동작 불변: `cache` 0, `reentry` 0, 최다 주소 동일 등 Task 405/406 값 유지.
* pumpit1 45초 **912 프레임**. 오늘 범위 안입니다.

### 6. 다음 대상

**진입 횟수가 1회인 실행과 수천 회인 실행의 차이를 먼저 설명해야 합니다.** 그 전에는
`0x0301DB22`의 진입 기전을 하나로 말할 수 없습니다. 히스토그램은 이미 들어가 있으므로
추가 실행만으로 두 모드가 다시 나오면 그 실행들의 분포를 직접 비교할 수 있습니다.

그 다음이 원래 질문입니다 — `0x0301F7CE`의 single-step을 처리하며 TF를 끄고 arena에
남기는 곳. 코드 읽기로 배제한 것은 다음과 같습니다.

* `HandleTimerInterruptChainBoundary`: `PUSHFD; CALL FAR [mem]` 패턴에서만 동작하며
  `0x0301F7CE`의 바이트(`83 ba ...`)와 맞지 않고, EFlags를 건드리지 않습니다.
* `execution_trampoline.cpp`의 TF 해제 지점 10곳: arena 밖 EIP용, terminal failure용,
  native region용, 미처리 종료용이라 관측 상태(정상 진행 + arena EIP + TF 꺼짐)와
  맞는 것이 없습니다.
* `aot_runtime_dispatch.cpp:1866~1913`의 세 분기: 각각 캐시 EIP로 이동, 격리 유지,
  legacy fallback(TF 유지)이라 셋 다 맞지 않습니다.

### 7. 미확정

* 진입 횟수의 실행 간 3자릿수 편차.
* `0x0301F7CE` single-step을 소비하는 지점.
* 재번역이 요청 진입 주소를 address map에 남기지 못하는 조건(Task 404 이월).
* 격리 발생 조건.

---

## English

### Summary

Added a **four-way predecessor histogram per address** to repair Task 408's error of reading a
population from a single first sample. It resolves the other addresses cleanly, but **the
target address `0x0301DB22` is still not settled**, for the reason in section 4.

### 1. The arithmetic behind Task 408's error

Across Task 408's four runs, single-step exceptions totalled 283, 260, 265, and 260 while
`0x0301DB22` recorded 3,124, 2,018, 2,430, and 2,403 entries, so **at most about a tenth** of
those entries could have had a single-step predecessor. "Identical across four runs" meant the
*first* entry point is always the same, not that every entry is.

### 2. The histogram works on the other addresses

Across three quarantine-free Task 409 runs:

| Address | count | Entries | step | **bp** | **av** | other | Dominant |
|---|---:|---:|---:|---:|---:|---:|---|
| `0x030D0A1A` | 10,404 | 10,404 | 0-4 | **10,249-10,319** | 85-155 | 0 | breakpoint |
| `0x0301F851` | 4,732 | 359 | 0 | 1 | **358** | 0 | access violation |
| `0x030D0A0F` | 1,152 | 1,152 | 0 | **1,152** | 0 | 0 | breakpoint |

`0x030D0A1A` and `0x030D0A0F` have **as many entries as executions**: every one follows a cache
INT3, so they cross between cache and arena each time. Task 408's observation that mechanisms
differ per address stands, now supported by a distribution rather than one sample.

### 3. `0x0301DB22` shows exactly one entry in these three runs

Frames 134, 73, and 1; counts 946,114, 889,039, and 42,906; entries **1, 1, 1**; histogram
1/0/0/0 each time, the predecessor being the single step at `0x0301F7CE`, the instruction after
`CLI`. **For these runs the first sample is the whole population, so Task 408's reading is
correct for them.**

### 4. It still cannot be settled — entry counts vary by three orders of magnitude

Task 408's four runs recorded 2,018-3,124 entries; Task 409's three recorded **1**. The test is
identical and the only code change was adding counters, so this is **run-to-run variation, not
a behaviour change** — and the class distribution of Task 408's entries was never measured,
because the histogram did not exist yet.

A plausible reading is that how often another exception interleaves between delay-loop faults
depends on the scene, but **that was not measured, so it is a guess.** Until the difference
between a one-entry run and a three-thousand-entry run is explained, the entry mechanism for
`0x0301DB22` cannot be stated as one thing.

### 5. Verification

Release build passed and the probe exited zero; Task 405/406 values are unchanged (`cache`
zero, `reentry` zero, same top address); pumpit1 rendered **912 frames**, inside today's range.

### 6. Next target

**Explain the difference between one-entry and thousands-of-entries runs first.** The histogram
is already in place, so more runs that reproduce both modes would let their distributions be
compared directly.

After that comes the original question — which handler consumes the single step at
`0x0301F7CE`, clears the trap flag, and leaves execution in the arena. Reading has ruled out:
`HandleTimerInterruptChainBoundary`, which matches only a `PUSHFD; CALL FAR [mem]` shape that
`0x0301F7CE`'s bytes (`83 ba ...`) do not fit and which never touches EFlags; the ten
trap-flag-clearing sites in `execution_trampoline.cpp`, which serve out-of-arena EIPs, terminal
failure, native regions, and unhandled termination, none matching a normally progressing run
with an arena EIP; and the three branches at `aot_runtime_dispatch.cpp:1866-1913`, which
respectively move to a cache EIP, hold the quarantine, or take the legacy fallback with the
trap flag left set.

### 7. Unresolved

The three-orders-of-magnitude variation in entry counts; which site consumes the
`0x0301F7CE` single step; the condition under which a re-translation omits its requested entry
from the address map (carried from Task 404); and what decides whether quarantine fires.
