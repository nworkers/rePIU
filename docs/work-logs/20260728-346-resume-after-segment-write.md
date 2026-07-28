# 20260728-346 작업 로그: 세그먼트 쓰기 이후 캐시 복귀 / Work log

설계: [20260728-346-resume-after-segment-write.md](../design/20260728-346-resume-after-segment-write.md)

## 한국어

### 결론 요약

**사전 등록 gate 네 개가 모두 성립했습니다.** 복귀 funnel의 최대 거절 사유였던
`segment-write` 15,473건이 **0**이 되고, 복귀 success 비중이 **55.6% → 95.1%**,
프레임 중앙값이 **3,125 → 3,456(+10.6%)** 입니다.

**그리고 실행 범위가 겹치지 않습니다**(3,094~3,265 대 3,405~3,463). Task 335가 정한
3회 중앙값 규칙에서 **판정 가능한 개선**입니다.

### 사전 등록 gate 판정

| gate | 조건 | 관측 | 판정 |
|---|---|---:|---|
| **G1** | `segment-write` 거절 급감 | 15,473 → **0** | **성립** |
| **G2** | 복귀 success 비중 증가 | 55.6% → **95.1%** | **성립** |
| **G3** | 프레임 중앙값 증가 | **+10.6%** | **성립** |
| **G4** | selector guard mismatch 폭증 없음 | **0 → 0** | **성립** |

**G4가 이 변경의 안전성 근거입니다.** 접힌 세그먼트 site의 selector guard mismatch는
양쪽 구성 모두 **0**입니다. 즉 재접기가 캐시를 계속 최신으로 유지했고, 낡은 fold로
트랩한 경우가 한 번도 없습니다.

### 측정 값 (Release 60초, 설정당 3회)

| 항목 | 기존(무조건 거절) | 신규(재접기 후 복귀) |
|---|---:|---:|
| **프레임 중앙값** | **3,125** | **3,456** |
| 프레임 실행별 | 3,094 / 3,125 / 3,265 | 3,405 / 3,456 / 3,463 |
| Glide gate 진입 | 130,728~137,425 | 148,955~150,647 |
| `segment-write` 거절 | 15,473 | **0** |
| 복귀 success | 21,783 (55.6%) | **37,256 (95.1%)** |
| `span-unsafe` 거절 | 1,944 | 1,926 |
| selector guard mismatch | 0 | **0** |
| 예외 총계 | 373,511 | 389,587 |

**확인됨:** `segment-write resumed` 계수기가 정확히 **15,473**입니다. 이전에 거절되던
그 수만큼이 이제 캐시로 복귀합니다.

**확인됨:** 동등성 계약을 전부 통과합니다. malformed 0, 정상 timeout,
**get-proc 37**, 프레임과 gate 진입은 **늘었습니다.**

**주의:** 예외 총계는 오히려 4.3% 늘었습니다(`INT3` 199,364 → 215,894). 캐시로 더 자주
복귀하니 경계 트랩도 더 자주 만납니다. **그럼에도 프레임이 늘었으므로**, 예외 수는
그 자체로 목표가 아니라는 점이 다시 확인됩니다.

### 바꾼 내용

`TryResumeAotAfterHandledHle`의 첫 검사에서 "세그먼트 레지스터를 쓰는 명령이면 무조건
거절"을 없앴습니다. 대신:

* 명령을 읽거나 decode할 수 없으면 **여전히 거절**(fail closed).
* 세그먼트 레지스터를 쓰면 **재접기(`ReResolveAotSegmentOverrides`) 후 복귀 시도**.

**정확성 근거는 두 겹입니다.** 재접기는 세그먼트 해석이 실제로 바뀐 경우에만 캐시의
접힌 site를 다시 패치하고(세그먼트 적재 HLE 자신이 이미 호출합니다), 접힌 각 site는
현재 selector가 접을 때와 다르면 고정 fallback `INT3`로 트랩합니다. 실측 mismatch 0이
그 두 겹이 실제로 작동함을 보여줍니다.

`REPIU_AOT_SEGMENT_WRITE_BLOCKS_RESUME=1`로 기존 동작을 복원할 수 있고 A/B는 그것으로
수행했습니다.

### 검증 결과

1. Debug/Release 전체 빌드 통과, `repiu_aot_probe` 두 구성 exit 0.
2. Release 60초 6회 모두 정상 timeout, malformed 0, fatal 0, Glide 공백 0, get-proc 37.

### 확인됨 / Confirmed

* 프레임 중앙값 +10.6%, 범위 비중첩.
* 복귀 success 55.6% → 95.1%, `segment-write` 거절 0.
* selector guard mismatch 0 — 재접기가 캐시를 최신으로 유지합니다.

### 미확정 / Unresolved

* 남은 거절은 `span-unsafe` 1,926(4.9%) 하나입니다.
* 예외 총계가 4.3% 늘어난 것의 순효과는 프레임으로만 판단했습니다.
* 재접기 호출 비용을 따로 재지 않았습니다(변경이 없으면 비교 6회로 끝나므로 작을 것으로
  보이나 미측정).

---

## English

All four pre-registered gates hold. The `segment-write` rejection, previously the funnel's largest
at 15,473, falls to zero; the return success share rises from 55.6% to 95.1%; and the median frame
count rises from 3,125 to 3,456, up 10.6%, with non-overlapping run ranges (3,094-3,265 against
3,405-3,463), which makes it a resolvable improvement under Task 335's median-of-three rule.

G4 is the safety evidence: selector-guard mismatches are zero in both configurations, so re-folding
kept the cache current and no stale fold ever trapped. The `segment-write resumed` counter reads
exactly 15,473, the number previously rejected.

The change removes the blanket refusal in `TryResumeAotAfterHandledHle`: an unreadable or
undecodable instruction still fails closed, while an instruction that writes a segment register now
triggers a re-fold and then proceeds. Correctness rests on two layers — the re-fold, which the
segment-load HLE already performs, and the per-site selector guard that traps to a fixed `INT3` on
a mismatch. `REPIU_AOT_SEGMENT_WRITE_BLOCKS_RESUME=1` restores the old behaviour and is how the A/B
was run.

Equivalence holds across all six runs: normal timeouts, zero malformed dispatch, no fatal halt, no
Glide gap, 37 resolved procs, and both frames and gate entries rising. Total exceptions rose 4.3%
(`INT3` from 199,364 to 215,894) because returning to the cache more often also meets more boundary
traps, which is another reminder that the exception count is not itself the goal. Unresolved: the
only remaining rejection is `span-unsafe` at 1,926 (4.9%), and the cost of the extra re-fold calls
was not measured separately.
