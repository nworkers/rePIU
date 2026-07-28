# 20260728-338 작업 로그: 예외 축소 opt-in 두 개의 Release A/B / Work log

> **정정(Task 339):** 아래 2절의 "`SUPERBLOCK`이 Glide gate 경계를 함께 없앤다"는
> **틀렸습니다.** gate 진입 급감은 원인이 아니라 증상입니다. 실제 원인은 inline HLE
> 처리 후 캐시로 복귀하지 못해 실행이 TF walk로 퇴화하는 것이며(single-step 구간
> 평균 4 → 113), 그래서 게임이 60초 안에 Glide 초기화를 끝내지 못합니다.
> [Task 339 작업 로그](20260728-339-hle-reentry-blocker.md)

## 한국어

### 결론 요약

**둘 다 채택할 수 없습니다. 그리고 하나는 위험합니다.**

* `REPIU_AOT_DBT_POST_HLE_TRANSLATE=1` — **무효.** 해당 경로에 진입조차 하지
  않았습니다(`posthle=0/0`). 처리량 변화 없음.
* `REPIU_AOT_DBT_SUPERBLOCK=1`(Task 308의 exception-free HLE) — **기각.** progress가
  3.15배로 뛰지만 **게임이 렌더링을 멈춥니다.** Glide gate 진입이
  `67,108 → 74`, `grBufferSwap` **0회**입니다.

### 측정 값 (Release 60초, 설정당 3회 중앙값)

| 설정 | progress 중앙값 | 프레임 중앙값 | 예외 총계 | Glide gate 진입 | 판정 |
|---|---:|---:|---:|---:|---|
| base | 105,203 | 1,583 | 939,349 | 67,108 | — |
| `POST_HLE_TRANSLATE=1` | 104,986 | 1,574 | 906,695 | 64,571 | 무효 |
| `SUPERBLOCK=1` | **330,903** | **0** | 1,469,861 | **74** | **기각** |

실행별 progress: base `109,480 / 105,203 / 104,411`,
posthle `104,986 / 105,451 / 104,288`, super `327,364 / 346,926 / 330,903`.

### 1. post-HLE 번역 opt-in은 경로에 진입하지 않았습니다

`PostHleTranslationEnabled()`는 `TryResumeAotAfterHandledHle`의 **cache miss 분기**
에서만 읽힙니다. 실행 내내 `posthle=0/0`이었으므로 그 분기가 한 번도 실행되지
않았습니다.

**따라서 이 A/B는 아이디어를 기각한 것이 아니라 무효입니다.** 다만 부수적으로 하나가
확인됩니다. **HLE 처리 후 재개할 때 대상은 이미 캐시에 있습니다.** 즉 Task 337이
발견한 5~8개짜리 TF walk는 HLE 재개 지점에서 생기는 것이 아닙니다.

### 2. exception-free HLE는 Glide gate를 함께 없앱니다

`SUPERBLOCK=1`에서 두 실행 모두 동일하게:

| 항목 | base | super |
|---|---:|---:|
| Glide gate 진입 | 67,108 | **74** |
| `grBufferSwap` | 1,548~1,585 | **0** |
| LINEXE get-proc | 37 | 33 |
| `INT3` 예외 | 183,303 | **24,781** |
| TF single-step | 745,012 | 1,434,217 |
| Glide gate wall-clock 비중 | 12.20% | 0.44% |

**확인됨:** `INT3`가 7.4배 줄어든 것은 의도된 효과지만, **그 `INT3` 중에 Glide gate
경계가 포함돼 있었습니다.** gate가 트랩하지 않으니 게임은 그래픽 호출을 잃고,
그 결과 "progress"만 3.15배로 뛰었습니다.

**progress는 정당성 지표가 아닙니다.** 렌더링을 하지 않으면 guest는 더 빨리
"진행"합니다.

### 3. 동등성 계약을 고칩니다

이 실행들은 기존 동등성 축을 **전부 통과했습니다**: 60초 정상 timeout,
`malformed 0`, `original fatal halt reached: false`,
`Glide implementation issues 0/0/0/0/0/0`, 창도 열렸습니다(`1/640x480`).

**그런데 게임은 아무것도 그리지 않고 있었습니다.**

따라서 이후 모든 성능 A/B의 동등성 계약에 다음을 추가합니다.

* **`grBufferSwap` 횟수가 baseline의 합리적 범위 안에 있을 것**
* **Glide gate 진입 횟수가 baseline과 같은 자릿수일 것**
* LINEXE get-proc 개수 동일

### 확인됨 / Confirmed

* post-HLE 번역 opt-in은 이 시나리오에서 경로 미진입이며 판정 불가입니다.
* HLE 재개 시 대상은 이미 캐시에 있습니다(부수 확인).
* `SUPERBLOCK=1`은 현재 형태로 Glide gate 경계를 함께 제거하며 사용할 수 없습니다.
* malformed/fatal/Glide-공백만으로는 렌더링 정지를 잡지 못합니다.

### 미확정 / Unresolved

* `SUPERBLOCK`이 gate 경계를 삼키는 **정확한 지점**(planner가 gate 주소를 HLE
  boundary로 분류해 thunk로 emit하는지, 아니면 gate 진입 자체가 다른 이유로 사라지는지)은
  확인하지 않았습니다. 이것이 다음 작업의 첫 질문입니다.
* Task 337이 발견한 5~8개 TF walk의 정체도 여전히 미확정입니다.

---

## English

### Summary

Neither opt-in can be adopted, and one is actively dangerous.
`REPIU_AOT_DBT_POST_HLE_TRANSLATE=1` is void: its path was never entered (`posthle=0/0`) and
throughput did not move. `REPIU_AOT_DBT_SUPERBLOCK=1`, Task 308's exception-free HLE, is rejected:
progress triples to 330,903 but the game stops rendering, with Glide gate entries falling from
67,108 to 74 and zero `grBufferSwap` calls.

### Measurements

Median of three 60-second Release runs each: base at progress 105,203, 1,583 frames, 939,349
exceptions, and 67,108 gate entries; post-HLE at 104,986, 1,574, 906,695, and 64,571; superblock at
330,903, zero frames, 1,469,861 exceptions, and 74 gate entries. Individual progress values were
109,480 / 105,203 / 104,411, then 104,986 / 105,451 / 104,288, then 327,364 / 346,926 / 330,903.

### The post-HLE opt-in never engaged

`PostHleTranslationEnabled()` is read only in the cache-miss branch of
`TryResumeAotAfterHandledHle`, and `posthle=0/0` throughout means that branch never ran. The A/B is
therefore void rather than a rejection of the idea, and it establishes one thing in passing: when
the HLE resume path runs, its target is already in the cache, so the five-to-eight-step TF walks
Task 337 found do not originate there.

### Exception-free HLE removes the Glide gate with it

Under `SUPERBLOCK=1`, reproducibly across runs, `INT3` exceptions fall 7.4x from 183,303 to 24,781
— the intended effect — but Glide gate entries collapse from 67,108 to 74, buffer swaps to zero,
resolved LINEXE procs from 37 to 33, and the gate's share of wall clock from 12.20% to 0.44%. The
gate boundaries were among the `INT3`s removed, so the game loses its graphics calls and "progress"
triples because it is no longer doing the rendering work. Progress alone is not a validity metric.

### The equivalence contract needs fixing

Those runs passed every existing equivalence axis: a normal 60-second timeout, zero malformed
dispatch, no fatal halt, no Glide implementation gap, and a window opened at 640x480 — while the
game drew nothing. Every performance A/B from here therefore also requires the `grBufferSwap` count
to stay in the baseline's range, the Glide gate entry count to stay within an order of magnitude of
baseline, and the resolved LINEXE proc count to match.

### Unresolved

Exactly where `SUPERBLOCK` swallows the gate boundary — whether the planner classifies gate
addresses as HLE boundaries and emits thunks for them, or gate entry disappears for another reason
— was not established, and it is the first question for the next task. What the five-to-eight-step
TF walks are also remains open.
