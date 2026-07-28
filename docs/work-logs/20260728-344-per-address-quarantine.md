# 20260728-344 작업 로그: 주소별 반복 쓰기로 quarantine 판정 / Work log

## 한국어

### 결론 요약

**quarantine이 0이 되고 복귀 funnel이 열렸습니다. 그러나 프레임은 움직이지
않았습니다.**

Task 342는 **페이지별 쓰기 횟수**로 판정했습니다. 그래서 한 페이지 위에 서로 다른
1회성 패치가 여러 개 있으면(페이지 `0x03033000`에는 `0x030334C6`과 `0x03033911`이
있습니다) 합산되어 결국 격리됐습니다.

churn의 진짜 신호는 **같은 주소가 반복해서 덮어써지는 것**입니다. 판정을 그렇게 바꿨고,
병적인 경우를 막기 위해 페이지당 총 쓰기 32회 상한을 함께 뒀습니다.

### 측정 값 (Release 60초, 3회)

| 항목 | Task 342 (페이지별) | Task 344 (주소별) |
|---|---:|---:|
| **프레임 중앙값** | **3,485** | **3,325** |
| 프레임 실행별 | 3,372 / 3,485 / 3,543 | 3,501 / 3,325 / 3,293 |
| quarantine 이벤트 | 1 | **0** |
| quarantine 복귀 거절 | 35,667 | **0** |
| 복귀 success 비중 | 29.7% | **55.5%** |
| TF single-step | 237,734 | 181,879 | 
| 예외 총계 | 462,104 | 417,288 |

**확인됨: gate G1(quarantine 0)과 G2(success >= 50%)가 이제 성립합니다.**
Task 342에서 기각됐던 두 gate입니다.

**확인됨: 그러나 프레임은 개선되지 않았습니다.** 중앙값 `3,485 → 3,325`는 4.6%
감소이지만 실행별 범위가 겹치므로(3,293~3,501 대 3,372~3,543) **개선도 퇴보도 주장할
수 없습니다.** Task 335가 정한 3회 중앙값 규칙으로도 판정 불가입니다.

**즉 남아 있던 quarantine 1건은 프레임 비용을 내고 있지 않았습니다.** 큰 이득은 Task
342에서 이미 다 나왔습니다.

### 그래도 채택하는 이유

* 마지막 quarantine이 사라져 **복귀 경로에 구조적 막힘이 없습니다**(거절 0).
* single-step 예외가 24%, 전체 예외가 10% 더 줍니다.
* 프레임 비용은 측정되지 않았습니다(범위 중첩).

**반대로, 이 변경만으로 성능이 좋아졌다고 기록하지 않습니다.**

### 새로 드러난 거절 사유

quarantine이 사라지자 `span-unsafe`가 484 → **1,990**으로 올라와 다음 순위가 됐습니다.
전체 시도 39,246 중 5.1%이며, 남은 최대 거절 사유는 여전히 `segment-write`
15,473(39.4%)입니다.

### 검증 결과

1. Debug/Release 전체 빌드 통과, `repiu_aot_probe` 두 구성 exit 0.
2. Release 60초 3회 모두 정상 timeout, malformed 0, fatal 0, Glide 공백 0,
   **get-proc 37**, gate 진입 144,098~154,666.

### 확인됨 / Confirmed

* 주소별 판정으로 quarantine 이벤트와 quarantine 거절이 모두 0이 됩니다.
* 복귀 success 비중 29.7% → 55.5%.
* 프레임 변화는 판정 불가(범위 중첩).

### 미확정 / Unresolved

* 남은 거절 1위는 `segment-write` 39.4%입니다. 세그먼트 레지스터를 쓰는 명령 직후에는
  캐시 복귀를 막는 규칙인데, 그 제약이 정말 필요한 범위인지 재검토 대상입니다.
* `span-unsafe` 1,990의 내용.
* 페이지당 32회 상한은 임의값이며 도달하지 않았습니다(overflow 0, 유예 9).

---

## English

Task 342 counted writes per page, so a page holding several distinct one-shot patches — page
`0x03033000` holds both `0x030334C6` and `0x03033911` — accumulated to the threshold and was
quarantined anyway. The real churn signal is the same address being rewritten, so the rule now
counts repeats per address, with a per-page ceiling of 32 writes to bound the pathological case.

Over three 60-second Release runs, quarantine events and quarantined return rejections both reach
zero, the return funnel's success share rises from 29.7% to 55.5%, single-steps fall from 237,734
to 181,879, and total exceptions from 462,104 to 417,288 — so gates G1 and G2, rejected in Task
342, now hold. Frames do not move: the median goes from 3,485 to 3,325, a 4.6% fall whose run
ranges overlap (3,293-3,501 against 3,372-3,543), so under Task 335's median-of-three rule neither
an improvement nor a regression can be claimed. The remaining quarantine was not costing frames;
Task 342 had already taken the gain.

It is adopted anyway because the return path now has no structural blocker and exceptions fall
further, but no performance improvement is recorded for it. With quarantine gone, `span-unsafe`
rises from 484 to 1,990 and becomes the second rejection reason, while `segment-write` at 15,473
(39.4% of 39,246 attempts) is now the largest.

All three runs reached the 60-second timeout with zero malformed dispatch, no fatal halt, no Glide
gap, 37 resolved procs, and 144,098 to 154,666 gate entries, and both configurations' probe suites
passed. Unresolved: whether the `segment-write` restriction is needed as broadly as it is applied,
what the 1,990 `span-unsafe` rejections are, and that the 32-write ceiling is arbitrary and was
never reached.
