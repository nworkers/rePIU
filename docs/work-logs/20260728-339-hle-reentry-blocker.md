# 20260728-339 작업 로그: HLE 이후 캐시 복귀가 막히는 지점 / Work log: The HLE reentry blocker

## 한국어

### 결론 요약

**Task 338의 인과 지목을 정정합니다.** `SUPERBLOCK=1`이 Glide gate 경계를 없애는 것이
아닙니다. **HLE를 인라인으로 처리한 뒤 캐시로 복귀하지 못해 실행이 TF walk로
퇴화하는 것**이고, gate 진입 급감은 그 증상입니다.

**그리고 그 복귀가 어디서 막히는지 확정했습니다.** `TryResumeAotAfterHandledHle`의
단계별 호출 수가 두 구성에서 서로 다른 지점에서 끊깁니다.

| 단계 | baseline | `SUPERBLOCK=1` |
|---|---:|---:|
| `aot-resume` 진입 | 206,345 | 1,186,516 |
| → seg-write 프로브 | 206,345 (100%) | **15,980 (1.3%)** |
| → quarantine/guest-IP 검사 | 190,874 | 649 |
| → cache lookup | **21,561 (10.4%)** | 641 |
| → span safety | 21,561 | 641 |

**확인됨(baseline): 88.7%가 quarantine/guest-IP 검사에서 거절됩니다.**
`190,874 → 21,561`. 그리고 검사를 통과해 lookup까지 간 21,561건은 **전부 캐시에
적중**합니다(21,561/21,561).

**확인됨(`SUPERBLOCK=1`): 98.7%가 함수 첫 guard에서 즉시 거절됩니다.**
`1,186,516 → 15,980`. 첫 guard의 조건 중 backend와 placement는 성립하므로 남는 것은
**`aot_reentry_pending`이 false**인 경우입니다. inline HLE thunk는 `INT3` 경계를 거치지
않으므로 그 플래그가 설정되지 않습니다.

**따라서 두 구성 모두 같은 결과에 이릅니다. HLE 처리 후 캐시로 돌아가지 못하고
guest가 TF로 걷습니다.** Task 337이 발견한 5~8개 구간과 33+ 꼬리의 정체가 이것입니다.

### 부수 확인 — `posthle`가 0/0인 이유

post-HLE 번역 분기는 **cache lookup이 실패했을 때만** 실행됩니다. 그런데 baseline에서
lookup까지 도달한 21,561건은 100% 적중합니다. 즉 그 분기는 **구조적으로 도달할 수
없습니다.** Task 338이 "무효"로 기록한 이유가 이것입니다.

`SUPERBLOCK`과 함께 켜도 같습니다(`posthle=0/0`, single-step 구간 평균 115).

### `SUPERBLOCK=1`의 실제 거동

| 항목 | baseline | `SUPERBLOCK=1` |
|---|---:|---:|
| `INT3` 예외 | 183,303 | **24,790** |
| TF single-step | 745,012 | **2,839,637** |
| single-step 구간 평균 / 최대 | 4 / 337 | **113 / 3,941** |
| 예외 중 single-step 비중 | 79.24% | **98.76%** |
| Glide gate 진입 | 67,108 | 74 |
| get-proc 마지막 | 37 `_GRDRAWTRIANGLE@12` | 33 `_GRBUFFERCLEAR@12` |

**확인됨:** `INT3` 7.4배 감소는 의도대로 일어납니다. 그러나 그 자리를 **3.8배 늘어난
single-step**이 대신합니다. guest는 Glide 초기화 도중(0x03086FBE~0x03087401 부근
루프) 60초를 다 씁니다.

**확인됨:** 따라서 `progress` 3.15배는 실제 진행이 아니라 **예외 수 증가의 부산물**
입니다. heartbeat도 같은 이유로 커집니다.

### 그래서 다음 작업이 정해집니다

exception-free HLE 기계장치 자체는 문제가 아닙니다. **복귀 경로가 없습니다.**
따라서 순서는 다음과 같습니다.

1. baseline의 88.7% 거절이 `IsGuestInstructionPointer`인지 quarantine인지 가른다.
   둘은 원인도 처방도 다르다(전자는 EIP가 arena 밖, 후자는 페이지 격리).
2. `SUPERBLOCK` 경로에서 inline HLE 처리 후 복귀에 필요한 상태를 갖추게 한다.
3. 그 다음에야 `SUPERBLOCK`을 다시 판정한다. 복귀가 되기 전에는 재판정할 의미가 없다.

### 확인됨 / Confirmed

* Task 338의 "gate 경계를 삼킨다"는 인과 지목은 **기각**되고, 원인은 복귀 실패입니다.
* baseline에서 HLE 복귀 시도의 88.7%가 quarantine/guest-IP 검사에서 거절됩니다.
* `SUPERBLOCK`에서는 98.7%가 `aot_reentry_pending` 미설정으로 즉시 거절됩니다.
* lookup까지 도달하면 캐시 적중률은 100%이므로 post-HLE 번역 분기는 도달 불가입니다.

### 미확정 / Unresolved

* 88.7% 거절의 내역(`IsGuestInstructionPointer` 대 quarantine)을 아직 나누지
  않았습니다. 계수기 하나면 갈립니다.
* guest가 Glide 초기화 중 도는 루프(0x03086FBE~0x03087401)가 무엇을 기다리는지는
  확인하지 않았습니다. `SUPERBLOCK`이 고쳐지면 자연히 사라질 수 있습니다.

---

## English

### Summary

Task 338's attribution is corrected: `SUPERBLOCK=1` does not remove the Glide gate boundary. It
removes the `INT3` boundaries as intended, but nothing brings execution back into the cache after
an inline HLE, so the run degenerates into TF walking and the collapse in gate entries is a
symptom. The blocker is now located exactly, and it differs by configuration.

`TryResumeAotAfterHandledHle` is entered 206,345 times in baseline and reaches the segment-write
probe every time, then the quarantine and guest-IP check cuts it from 190,874 to 21,561 — 88.7%
rejected — while every one of those 21,561 that reaches the cache lookup hits. Under `SUPERBLOCK=1`
the function is entered 1,186,516 times but only 15,980 reach the segment-write probe, so 98.7% are
rejected by the function's first guard; with the backend and placement conditions satisfied, the
remaining possibility is `aot_reentry_pending` being false, which is exactly what an inline HLE
thunk leaves unset because it never passes through an `INT3` boundary.

Both configurations therefore end in the same place: after HLE the guest cannot return to the cache
and walks under TF. That is what Task 337's five-to-eight-step mode and long tail are.

### Why `posthle` is always 0/0

The post-HLE translation branch runs only when the cache lookup misses, and every lookup that
happens hits, so the branch is structurally unreachable. Enabling it alongside `SUPERBLOCK` changes
nothing either — `posthle=0/0` with a single-step run mean of 115.

### What `SUPERBLOCK=1` actually does

`INT3` falls 7.4x from 183,303 to 24,790 as intended, but single-steps rise 3.8x from 745,012 to
2,839,637, run length goes from a mean of 4 and max 337 to a mean of 113 and max 3,941, and
single-steps become 98.76% of all exceptions. The guest spends the whole 60 seconds in a loop
around 0x03086FBE-0x03087401 during Glide initialization, having resolved 33 procs against
baseline's 37. Its 3.15x "progress" is a by-product of counting more exceptions, not of doing more
work.

### Next

The exception-free HLE machinery is not the problem; the return path is missing. First split the
baseline's 88.7% rejection between `IsGuestInstructionPointer` and quarantine, since those have
different causes and fixes; then give the `SUPERBLOCK` path the state its return requires; and only
then re-judge `SUPERBLOCK`, because judging it before the return works measures nothing.

### Unresolved

The 88.7% rejection has not been split between its two conditions, which one counter would settle,
and what the Glide-initialization loop waits on is unknown, though it may disappear once the return
path works.
