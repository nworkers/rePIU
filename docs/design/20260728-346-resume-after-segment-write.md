# 20260728-346 설계: 세그먼트 쓰기 이후에도 캐시로 복귀 / Design: Resuming after a segment write

## 한국어

### 1. 왜 필요한가

Task 344 이후 post-HLE 복귀 funnel의 최대 거절 사유는 **`segment-write` 39.4%**
(시도 39,246건 중 15,473건)입니다. 규칙은 단순합니다. 방금 처리한 HLE 명령이 세그먼트
레지스터를 쓰면 **무조건 캐시 복귀를 포기**하고 guest를 TF로 걷게 둡니다.

### 2. 그 규칙이 지키려는 것

AOT 캐시는 세그먼트 override 접근의 **base를 displacement에 접어 넣습니다**(Task 264).
따라서 guest가 세그먼트 레지스터를 바꾸면 접힌 base가 낡을 수 있고, 그 상태로 캐시에
복귀하면 잘못된 주소를 읽게 됩니다.

### 3. 그런데 이미 두 겹의 방어가 있습니다

1. **재접기.** `ReResolveAotSegmentOverrides`는 세그먼트 해석이 실제로 바뀌었을 때만
   캐시의 접힌 site를 다시 패치합니다. 그리고 **세그먼트 적재 HLE 자신이 이미 그것을
   호출합니다**(`RecordGuestSegmentLoad` 끝). 즉 이 HLE를 거쳐 온 경우 캐시는 이미
   최신입니다.
2. **런타임 guard.** 접힌 각 site는 emit 시점에 `guard_selector`/`guard_address`를
   갖고, 현재 selector가 접을 때와 다르면 **고정 fallback `INT3`로 빠집니다.** 즉
   낡은 fold는 조용히 잘못 실행되지 않고 **트랩합니다.**

**따라서 무조건 거절은 이 두 방어 위에 얹힌 세 번째 보수 조치입니다.**

### 4. 바꾸는 것

세그먼트 쓰기 명령 이후에도 복귀를 시도하되, 그 전에 **재접기를 보장**합니다.

| 판정 | 기존 | 변경 |
|---|---|---|
| 명령을 읽거나 decode 못 함 | 거절 | **거절(유지)** — fail closed |
| 세그먼트 레지스터를 씀 | 거절 | **재접기 후 복귀 시도** |
| 그 외 | 복귀 시도 | 복귀 시도 |

**정확성 근거:** 재접기가 캐시를 최신화하고, 그래도 어긋나면 site별 selector guard가
`INT3`로 트랩합니다. 조용한 오실행 경로가 없습니다.

### 5. 사전 등록 gate

| gate | 조건 |
|---|---|
| **G1** | `segment-write` 거절 수가 크게 감소 |
| **G2** | 복귀 success 비중 증가 |
| G3 | 프레임 중앙값(3회) 증가 |
| G4 | selector guard fallback(`INT3`) 수가 폭증하지 않음 — 재접기가 실제로 유효한지 |

동등성 계약은 Task 338 확장본을 그대로 씁니다(malformed·fatal·Glide 공백 0,
`grBufferSwap`·gate 진입·get-proc 유지).

### 6. 되돌릴 수단

`REPIU_AOT_SEGMENT_WRITE_BLOCKS_RESUME=1`이면 기존 무조건 거절로 돌아갑니다.

---

## English

Since Task 344 the largest rejection in the post-HLE return funnel is `segment-write` at 39.4%,
15,473 of 39,246 attempts: whenever the HLE instruction just handled writes a segment register, the
return to the cache is abandoned and the guest walks under TF.

The rule protects folded segment bases — the cache folds a segment's base into the displacement of
override accesses (Task 264) — but two defences already cover that. `ReResolveAotSegmentOverrides`
re-patches the folded sites when the resolution actually changed, and the segment-load HLE itself
calls it, so the cache is already current on that path; and every folded site carries a
selector guard that falls through to a fixed `INT3` when the current selector differs from the one
folded in, so a stale fold traps instead of executing silently. The blanket rejection is a third,
conservative layer on top.

The change keeps failing closed when the instruction cannot be read or decoded, and otherwise
ensures re-folding and then attempts the return. Pre-registered gates: G1, the `segment-write`
rejection count falls sharply; G2, the funnel's success share rises; G3, the median frame count over
three runs rises; and G4, selector-guard fallbacks do not explode, which is what would show
re-folding failing to keep up. The equivalence contract is Task 338's, and
`REPIU_AOT_SEGMENT_WRITE_BLOCKS_RESUME=1` restores the old behaviour.
