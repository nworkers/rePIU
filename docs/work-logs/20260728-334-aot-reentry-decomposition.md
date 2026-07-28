# 20260728-334 작업 로그: AOT reentry 재분해와 역방향 색인 / Work log

설계: [20260728-334-aot-reentry-decomposition.md](../design/20260728-334-aot-reentry-decomposition.md)

작업 지시: [20260728-334-aot-reentry-decomposition.md](../work-orders/20260728-334-aot-reentry-decomposition.md)

## 한국어

### 결론 요약

**gate G1이 성립했습니다. `HandleAotReentry`의 96.00%는 `FindAotGuestAddress`의 선형
탐색이었습니다.** 호출 128,700회에 `71,024,895,923 tick`, 호출당 `551,864 tick`
(2.5GHz 기준 약 221us)입니다. 이는 Release 전체 wall-clock의 약 **44%** 입니다.

Task 324가 guest→cache 방향만 색인했고 cache→guest 방향은 선형 탐색으로 남아 있었기
때문입니다. 정렬 이진 탐색으로 교체한 결과 호출당 `551,864 → 2,075 tick`
(**-99.6%, 266배**), 60초 프레임 `891 → 1,597`(**1.79배**), progress
`86,203 → 109,158`(1.27배)입니다.

### 사전 등록 gate 판정

| gate | 조건 | 관측 | 판정 |
|---|---|---:|---|
| **G1** | `guest-lookup` >= 60% | **96.00%** | **성립** |
| G2 | `boundary-reason` >= 30% | 0.12% | 기각 |
| G3 | `native-span` >= 30% | 0.00% | 기각 |
| G4 | `retired` >= 30% | 0.99% | 기각 |
| G5 | `single-step` >= 30% | 2.42% | 기각 |
| G6 | residual >= 30% | 0.21% | 기각 |

**residual 0.21%는 분해 경계가 옳았다는 뜻입니다.** 여섯 구간이 reentry 핸들러를 거의
완전히 분할했습니다.

### 측정 값 — 수정 전

| 구간 | 비중 | 횟수 | 회당 tick |
|---|---:|---:|---:|
| **`guest-lookup`** | **96.00%** | 128,700 | **551,864** |
| `single-step` | 2.42% | 554,403 | 3,224 |
| `retired` | 0.99% | 41,556 | 17,693 |
| `provenance` | 0.26% | 128,700 | 1,512 |
| `boundary-reason` | 0.12% | 128,592 | 671 |
| `native-span` | 0.00% | 0 | — |
| residual | 0.21% | — | — |

`native-span` 0회는 이 backend 설정에서 해당 경로가 꺼져 있기 때문입니다.

### 측정 값 — 수정 후 (60초 Release A/B)

| 항목 | 수정 전 | 수정 후 | 비 |
|---|---:|---:|---:|
| `guest-lookup` 회당 | 551,864 | **2,075** | **1/266** |
| `guest-lookup` reentry 대비 | 96.00% | 6.73% | — |
| progress | 86,203 | 109,158 | **1.27배** |
| 프레임(`grBufferSwap`) | 891 | **1,597** | **1.79배** |
| VEH의 wall-clock 비중 | 64.07% | **34.13%** | — |
| AOT 캐시 내 guest 실행 | 35.93% | **65.87%** | — |
| Glide gate | 9.99% | 17.14% | — |

**확인됨:** 동등성 유지 — 60초 정상 timeout, `malformed 0`,
`original fatal halt reached: false`, `Glide implementation issues 0/0/0/0/0/0`.

**확인됨:** 이제 wall-clock의 **65.87%가 AOT 캐시 안의 실제 guest 실행**입니다.
Task 323이 Debug에서 약 12.4%로 추정했던 값입니다. VEH는 34.13%로 내려왔습니다.

**확인됨:** 수정 후 reentry 내부 순위가 완전히 바뀌었습니다. `single-step` 64.61%,
`retired` 15.52%, `guest-lookup` 6.73%, `provenance` 6.38%, `boundary-reason` 2.73%,
residual 4.03%입니다.

### 구현

* `Win32AotCacheAddressIndex`에 `cache_offset_sorted`와 `max_emitted_length`를 추가하고
  entry가 색인될 때마다 갱신합니다. **정렬 여부는 가정하지 않고 관측합니다.**
* `LookupAotGuestAddressIndex`는 `upper_bound`로 후보를 찾은 뒤, 더 이른 긴 entry가
  덮을 수 있는 범위(`max_emitted_length`)만큼만 뒤로 걸어 **가장 낮은 map index의
  일치**를 반환합니다. 이것이 기존 선형 탐색의 "첫 일치" 규칙과 동일합니다.
* `FindAotGuestAddress`는 색인이 쓸 수 없으면(미정렬이거나 낡음) 기존 선형 탐색을
  그대로 실행합니다. Task 324와 같은 정책이라 **느려질 뿐 틀리지 않습니다.**

### 검증 결과

1. Release 전체 빌드 통과, Debug 전체 빌드 통과.
2. `repiu_aot_probe` 두 구성 exit 0.
3. 차등 검증: 기존 선형 탐색을 oracle로 두고 **entry의 모든 바이트와 양끝 경계
   ±1**을 질의해 일치를 확인했습니다(`aot_cache_address_index_reverse`). 범위 사이
   구멍과 뒤 entry보다 긴 entry를 포함한 배치(`_reverse_ranges`), 그리고 역순
   cache offset에서 색인이 스스로 사용 불가를 보고하고 선형 탐색이 도는 경우
   (`_reverse_unsorted`)도 확인했습니다.
4. `execution_time_profile_stable_indices`로 기존 축 인덱스가 밀리지 않았음을
   확인했습니다(reentry 축은 끝에 추가).
5. Release 60초 A/B 2회 모두 정상 timeout, malformed 0, fatal 0, Glide 공백 0.

### 확인됨 / Confirmed

* `HandleAotReentry`의 96.00%가 cache→guest 선형 탐색이었고, 이는 Release 전체의
  약 44%였습니다.
* 이진 탐색 교체로 호출당 266배, 프레임 1.79배, progress 1.27배입니다.
* 실행 축이 뒤집혔습니다: VEH 64.07% → 34.13%, AOT 캐시 실행 35.93% → 65.87%.

### 미확정 / Unresolved

* 남은 reentry 내부 1위는 `single-step` 64.61%입니다. 이는 재개 경로 전체를 하나로
  묶은 구간이므로 더 나눠야 의미가 있습니다.
* Glide gate 비중이 9.99% → 17.14%로 **올라간** 것은 절대 비용 증가가 아니라 다른
  구간이 줄어든 결과로 보이나, 절대값 비교는 하지 않았습니다.
* A/B는 조건당 1회 표본입니다.
* handler 축 표시에서 `return` 484.73%처럼 100%를 넘는 값은 중첩 때문이며 재분해가
  필요합니다(기존 문제).

---

## English

### Summary

Gate G1 holds: 96.00% of `HandleAotReentry` was the linear scan in `FindAotGuestAddress` —
`71,024,895,923` ticks across 128,700 calls, `551,864` each, about 221us at 2.5GHz, and roughly 44%
of all Release wall clock. Task 324 had indexed only the guest-to-cache direction and left this one
scanning. Replacing it with a sorted binary search cut the per-call cost from `551,864` to `2,075`
ticks (-99.6%, 266x), raised the 60-second frame count from 891 to 1,597 (1.79x), and progress from
86,203 to 109,158 (1.27x).

### Gate results

G1 holds at 96.00%. G2 is rejected at 0.12%, G3 at 0.00%, G4 at 0.99%, G5 at 2.42%, and G6 at
0.21%, and that small residual is itself the evidence that the six intervals partitioned the
handler correctly. `native-span` recorded no calls because that path is off in this backend
configuration.

### Measurements

Before the fix the intervals were `guest-lookup` 96.00% over 128,700 calls, `single-step` 2.42%
over 554,403, `retired` 0.99% over 41,556, `provenance` 0.26%, `boundary-reason` 0.12%, and a 0.21%
residual. After it, `guest-lookup` costs 2,075 ticks per call and holds 6.73%, while the ranking
inverts to `single-step` 64.61%, `retired` 15.52%, `provenance` 6.38%, `boundary-reason` 2.73%, and
a 4.03% residual. At the whole-run level the VEH fell from 64.07% to 34.13% of wall clock while AOT
cache execution rose from 35.93% to 65.87% — the majority of the run is now the guest's own code,
against the roughly 12.4% Task 323 estimated in Debug. Both runs reached the 60-second timeout with
zero malformed dispatch, no fatal halt, and no Glide gap.

### Implementation

The index carries `cache_offset_sorted` and `max_emitted_length`, both maintained as entries are
indexed, so sortedness is observed rather than assumed. `LookupAotGuestAddressIndex` finds the
candidate with `upper_bound` and walks back only as far as `max_emitted_length` allows an earlier,
longer entry to still cover the offset, returning the lowest matching map index — exactly the
linear scan's first-match rule. When the index is unusable, `FindAotGuestAddress` runs the original
scan unchanged, so it degrades to slow rather than wrong, following Task 324's policy.

### Verification

Full Release and Debug builds passed and `repiu_aot_probe` exited 0 in both configurations. The
differential check queries every byte of every entry plus one past each end against the old scan as
oracle, and additionally covers a layout with a gap between ranges and an entry longer than the
distance to its successor, and an out-of-order cache offset where the index must report itself
unusable and the scan must run. `execution_time_profile_stable_indices` confirms the existing axes
did not shift, since the reentry axis is appended at the end.

### Unresolved

The largest remaining interval inside reentry is `single-step` at 64.61%, which lumps the whole
resumption path together and needs splitting before it means anything. The Glide gate's share rose
from 9.99% to 17.14%, which looks like other intervals shrinking rather than an absolute increase,
but the absolute values were not compared. The A/B is a single sample per condition, and the
handler axis still reports overlapping values past 100% (`return` at 484.73%) because it nests,
which is a pre-existing decomposition problem.
