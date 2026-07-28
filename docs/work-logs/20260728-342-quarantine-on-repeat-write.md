# 20260728-342 작업 로그: 반복 쓰기에만 quarantine / Work log

설계: [20260728-342-quarantine-on-repeat-write.md](../design/20260728-342-quarantine-on-repeat-write.md)

## 한국어

### 결론 요약

**프레임이 2.2배가 됐습니다.** 60초 Release 3회 중앙값으로 `grBufferSwap`
**1,579 → 3,485(+121%)**, Glide gate 진입 `65,487 → 149,260(+128%)`,
single-step 예외 `731,132 → 237,734(-67%)`입니다.

**그리고 이 작업이 오래된 지표 오해를 드러냈습니다.** `diagnostic progress count`는
게임 진행이 아니라 **HLE가 emulate한 이벤트 수**입니다(`instruction_emulation.cpp`에서
증가). 이번에 `107,572 → 30,591(-72%)`로 떨어진 것은 퇴보가 아니라 **emulate가 필요한
명령이 72% 줄었다**는 뜻입니다.

### 사전 등록 gate 판정

| gate | 조건 | 관측 | 판정 |
|---|---|---:|---|
| G1 | quarantine 이벤트 0 | 3 → 1 | 기각 |
| G2 | 복귀 success 비중 >= 50% | 13.1% → 29.7% | 기각 |
| **G3** | single-step 예외 감소 | **-67%** | **성립** |
| **G4** | 프레임 중앙값 증가 | **+121%** | **성립** |

**G1이 기각된 이유는 정책이 의도대로 동작했기 때문입니다.** 남은 1건은 페이지
`0x03033000`에 대한 **4회 이상의 자기 페이지 쓰기**입니다(`0x03033911`, 2바이트).
설계가 진짜 자기수정으로 분류하기로 한 대상이며, 나머지 두 페이지
(`0x030F5000`, `0x03034000`)는 이제 격리되지 않습니다.

### 측정 값 (Release 60초, 설정당 3회)

| 항목 | 기존(첫 쓰기 격리) | 신규(4회 이상만) | 비 |
|---|---:|---:|---:|
| **프레임(`grBufferSwap`)** | 1,533 / 1,579 / 2,029 → **1,579** | 3,372 / 3,543 / 3,485 → **3,485** | **2.21배** |
| Glide gate 진입 | 63,695 / 65,487 / 83,690 → 65,487 | 146,993 / 149,260 / 152,605 → 149,260 | 2.28배 |
| 예외 총계 | 922,384 | 462,104 | 0.50배 |
| TF single-step | 731,132 | 237,734 | 0.33배 |
| quarantine 거절 | 127,978 | 35,667 | 0.28배 |
| 복귀 success 비중 | 13.1% | 29.7% | — |
| emulate 이벤트(구 "progress") | 107,572 | 30,591 | 0.28배 |
| quarantine 이벤트 / 유예 | 3 / 0 | 1 / 6 | — |

**확인됨: 동등성 계약(Task 338 확장본)을 전부 통과합니다.** malformed 0,
`original fatal halt reached: false`, Glide 공백 0/0/0/0/0/0, 60초 정상 timeout,
**LINEXE get-proc 37 `_GRDRAWTRIANGLE@12`** (baseline과 동일), 그리고 프레임과 gate
진입은 **줄지 않고 늘었습니다.**

### 지표 정정 — `progress`는 처리량이 아닙니다

`diagnostic_progress_count`는 `instruction_emulation.cpp`의 HLE 처리 경로에서
증가합니다. 즉 **emulate된 이벤트 수**이며, emulate가 줄면 함께 줍니다.

Tasks 331~341은 이 값을 처리량 대리 지표로 인용했습니다. 그 작업들에서는 프레임과
같은 방향으로 움직여 결론이 뒤집히지는 않지만, **이번처럼 갈라지는 경우 프레임이
옳은 지표입니다.** 이후 처리량 판정은 **프레임(중앙값 3회)** 을 1차 지표로 씁니다.

### 구현

`NoteSuccessfulAotGuestWrite`에서 "쓰기 주체 페이지 == 쓰인 페이지"일 때 즉시
quarantine하던 것을, **페이지별 쓰기 횟수를 세어 4회째부터** quarantine하도록 바꿨습니다.
그 이전 쓰기는 **retire만** 합니다.

**정확성은 바뀌지 않습니다.** retire가 이미 그 페이지의 번역을 무효화하므로 캐시가 옛
바이트를 실행할 수 없고, 재번역은 항상 그 시점의 guest 바이트를 읽습니다. quarantine은
churn 방어일 뿐입니다. 페이지 기록표(64개)가 넘치면 그 페이지는 다음 쓰기에 즉시
quarantine되어 **기존 정책으로 degrade**합니다(이번 실행 overflow 0).

`REPIU_AOT_QUARANTINE_FIRST_WRITE=1`로 기존 동작을 복원할 수 있으며 A/B는 그것으로
수행했습니다.

### 검증 결과

1. Debug/Release 전체 빌드 통과, `repiu_aot_probe` 두 구성 exit 0.
2. Release 60초 3회씩, 6회 모두 정상 timeout·malformed 0·fatal 0·Glide 공백 0.
3. 신규 정책 3회 모두 get-proc 37로 baseline과 동일.

### 확인됨 / Confirmed

* 프레임 중앙값 2.21배, gate 진입 2.28배, single-step 예외 -67%.
* 남은 quarantine 1건은 4회 이상 자기 페이지에 쓰는 실제 자기수정 페이지입니다.
* `diagnostic progress count`는 emulate 이벤트 수이며 처리량 지표가 아닙니다.

### 미확정 / Unresolved

* **복귀 success 비중이 29.7%에 그칩니다.** 남은 quarantine 거절 35,667건은 페이지
  `0x03033000` 한 곳에서 나옵니다. 그 페이지를 어떻게 다룰지가 다음 후보입니다.
* 신규 정책에서 `other` 예외가 1 → 1,931로 늘었습니다. 정체를 확인하지 않았습니다.
* 임계값 4는 관측(1회 대 4회 이상)에 근거한 값이며 최적값을 탐색하지 않았습니다.

---

## English

### Summary

The median frame count over three 60-second Release runs rose from 1,579 to 3,485, a 2.21x
increase, with Glide gate entries up from 65,487 to 149,260 and single-step exceptions down 67%
from 731,132 to 237,734. The work also exposed a long-standing metric misreading: `diagnostic
progress count` increments inside the HLE emulation paths, so it counts emulated events rather than
game progress. Its fall from 107,572 to 30,591 means 72% fewer instructions needed emulation, which
is the point rather than a regression.

### Gate results

G3 and G4 hold at -67% single-steps and +121% frames. G1 is rejected because the policy worked as
designed: the one remaining quarantine is page `0x03033000` written from `0x03033911` four or more
times, which the design classifies as genuine self-modification, while `0x030F5000` and
`0x03034000` are no longer quarantined at all. G2 is rejected with the return funnel's success
share reaching only 29.7%.

### Equivalence

All six runs reached the 60-second timeout with zero malformed dispatch, no fatal halt, and no
Glide gap, and every new-policy run resolved 37 LINEXE procs ending at `_GRDRAWTRIANGLE@12`, the
same as baseline, with frames and gate entries rising rather than falling — the Task 338 contract
in the direction that matters.

### Implementation

`NoteSuccessfulAotGuestWrite` now counts writes per page and quarantines only from the fourth
same-page write; earlier writes retire only. Correctness is unchanged because retiring already
invalidates the page's translations, so the cache cannot execute stale bytes and re-translation
always reads the current guest bytes; quarantine is only a churn defence. If the 64-entry page
table overflows, that page quarantines on its next write, degrading to the old policy; this run
overflowed zero times. `REPIU_AOT_QUARANTINE_FIRST_WRITE=1` restores the old behaviour and is how
the A/B was run.

### Metric correction

Tasks 331 through 341 quoted `progress` as a throughput proxy. It moved with frames there, so their
conclusions stand, but when the two diverge — as here — frames is the correct measure, and
throughput judgements from now on use the median frame count over at least three runs.

### Unresolved

The return funnel's success share is still only 29.7%, and the remaining 35,667 quarantined
rejections all come from page `0x03033000`, which is the next candidate. The new policy also raised
`other` exceptions from 1 to 1,931, which was not investigated. The threshold of four follows the
observed split between one-shot and repeated writers and was not tuned.
