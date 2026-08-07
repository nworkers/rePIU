# Task 440 작업 로그 — 비동기 present (**측정 확정, 구현 미완**)

설계: [20260807-440](../design/20260807-440-glide-async-present.md) ·
작업 지시: [20260807-440](../work-orders/20260807-440-glide-async-present.md)

## 1. 결론 두 줄

**측정은 확정입니다.** vsync가 켜진 실제 플레이 구성에서 `grBufferSwap`은 guest-run의
**32.8%** 이고, 그중 **99.95%가 `SDL_GL_SwapWindow` 대기**입니다. 이 프로젝트에서 잰
Glide 항목 중 가장 큽니다.

**구현은 동작하고 크래시는 해결됐습니다**(§4). 다만 **프레임은 개선되지 않습니다** —
대기가 `grBufferSwap`에서 `grDepthMask`로 옮겨갈 뿐입니다(§4''). 승격 근거가 아직
없으므로 opt-in 유지이며, 다음 증분은 void setter까지 post하는 것입니다.

## 2. 측정 (Release, attract, 20초, `REPIU_GLIDE_SWAP_TIME_PROFILE=1`)

| 지표 | **vsync ON(플레이 기본값)** | interval 0 |
|---|---:|---:|
| swap / fps | 752 / 37.6 | 8,130 / 406 |
| **swap ÷ guest-run** | **32.8%** | 2.9% |
| glide-gate 전체 | 45.7% | 19.6% |
| **present 1회당** | **32.36M cycle ≈ 10.8 ms** | 214K ≈ 0.07 ms |
| present ÷ swap host work | **99.95%** | 98.8% |

프레임당 게스트 작업이 약 17.9 ms인데 그 위에 **10.8 ms 순수 대기**가 얹혀 26.6 ms가
됩니다. 그리고 게임은 `grBufferSwap` 뒤에 `grBufferNumPending`을 **1:1로**(7,276:7,276)
호출합니다 — Glide swap은 원래 비동기이고 게임이 pending 개수로 스로틀링합니다.
**우리 구현이 하드웨어보다 동기적이라는 뜻이고, 이건 정확성 문제이기도 합니다.**

## 3. 구현한 것

| 요소 | 상태 |
|---|---|
| 유한 비동기 FIFO + "동기 슬롯 전에 큐를 비운다" 순서 규칙 | 동작 |
| `grBufferSwap`·`grBufferClear`·draw batch flush의 post 경로 | 동작 |
| `grBufferNumPending`이 실제 미완료 swap 수 반환 | 동작 |
| 미완료 swap 1개 상한과 역압 | 동작 |
| probe(순서·회계·종료 후 거부) | **통과** |
| 스모크에서 회계 | posted 2,226 / executed 2,225 / failures 0 / refused 0 / max-depth 2 |
| ordinal 85 호출당 | 44.5M → **약 59K cycle** (대기가 게스트 밖으로 나감) |

## 4. [해결] teardown 크래시 — 원인은 조건 변수였습니다

[Task 441](20260807-441-host-crash-report.md)의 크래시 보고를 넣자 **첫 실행에서**
지목됐습니다: `Close()`의 `notify_all()`이 `RtlWakeAllConditionVariable`에서 폴트.
timeout 경로가 게스트 스레드를 `TerminateThread`로 죽이는데 그 스레드는 동기 게이트마다
`host_command_cv_`에서 **대기 중**이고, 대기 중 살해된 스레드의 wait block이 목록에 남기
때문입니다. **뮤텍스가 아니라 조건 변수**였습니다.

`Close()`의 `notify_all`을 없앴습니다 — 역압으로 막혀 있을 수 있는 유일한 스레드가
게스트이고, 그 시점에 이미 죽어 있으므로 깨울 대상이 없습니다. **8/8 정상**이 됐습니다.

아래는 그 전에 배제했던 것들의 기록입니다.

## 4'. 배제 기록 (참고)

| 구성 | 결과 |
|---|---|
| **커밋된 main (baseline)** | **13/13 정상** |
| 이 브랜치, `=0`(비활성) | 약 50% 크래시 |
| 이 브랜치, `=1` | 약 50% 크래시 |

크래시는 **12초 timeout 직후, 요약 출력 전**에 납니다. 스위치를 꺼도 나므로 비동기
경로 자체가 아니라 **상시 경로 어딘가**입니다.

**배제한 것(각각 빌드 후 실측):**

1. attempt 구조체 크기 증가 — 제거해도 재현.
2. `Close()`의 lock 획득(종료된 게스트가 쥔 뮤텍스 문제) — lock을 전부 걷어내도 재현.
3. 게스트 스택 위의 지연 생성 정적 상태 — atomic 직접 읽기로 바꿔도 재현.
4. idle 상태의 pump drain — 아무것도 게시되지 않으면 baseline과 동일 경로로 만들어도 재현.
5. teardown 스냅샷 호출 — 제거해도 재현.

**남은 후보:** 남은 상시 차이는 backend의 새 멤버(atomic·`unique_ptr`), out-of-line
생성자/소멸자, `WaitAndPumpHostCommands`의 술어, boundary의 정적 스위치 조회입니다.

## 4''. vsync ON 실측 — 대기는 사라지지 않고 **옮겨갑니다**

Release, attract, 20초, vsync ON.

| ordinal | `=0` | `=1` |
|---|---:|---:|
| `grBufferSwap` | **35.11%** of guest-run (33.2M/call) | **0.017%** — 게스트에서 사라짐 |
| **`grDepthMask`** | 0.10% | **33.57%** (9.09M/call, 그중 `wake` 9.06M) |
| `grLfbLock` | 9.83% | 6.90% |
| glide-gate 합계 | 48.85% | **45.62%** |
| 프레임 | 785 | 782 |

**`grBufferSwap`에서 게스트를 떼어내는 데는 성공했지만 프레임은 그대로입니다.** 대기가
`grDepthMask`로 옮겨갔고, 그 비용의 **99.7%가 `wake`** — host가 `SDL_GL_SwapWindow`
안에 있어 요청을 집어 갈 수 없어서 기다린 시간입니다.

**교훈: vsync에서는 프레임 안에 동기 게이트가 하나라도 있으면 그것이 vblank 대기를
흡수합니다.** `grDepthMask`는 Task 365 batch 1이 "변동이 잦다"며 생략에서 뺀 setter라
매 프레임 3.5회 host를 건드립니다.

**다음 증분은 명확합니다** — void 반환 setter(`grDepthMask`·`grConstantColorValue`·
`grDitherMode`·combine 2종 등)도 post 대상에 넣어야 프레임 전체가 host를 기다리지 않게
됩니다. `grLfbLock`은 값을 돌려주므로 동기로 남고, 그것이 이 축의 바닥입니다.

## 5. 회고 — 방법이 틀렸습니다

두 번째 가설이 빗나간 시점에서 **디버거로 폴트 주소를 잡았어야** 했습니다. 그 대신
가설→수정→재빌드를 다섯 번 반복했고, 각 회차가 빌드 5분과 실행 2분을 먹었습니다.
실측으로 배제한 것은 남았지만, 같은 시간에 크래시 지점을 직접 알 수 있었습니다.

또 한 번의 실험은 **교란돼 있었습니다** — 스냅샷 호출을 뺐을 때 `Close()`의 lock이
아직 남아 있어서, 그 회차의 "재현됨"은 두 후보 중 무엇도 배제하지 못했습니다. 나중에
따로 다시 재서 배제했습니다.

## 6. 다음에 할 일

1. **크래시 덤프부터.** 폴트 주소와 스택 없이 더 좁히지 않습니다.
2. **채널을 분리합니다.** 비동기 큐가 `host_command_mutex_`를 공유하지 않도록, 자체
   동기화(또는 lock-free ring)를 갖게 합니다. 종료된 게스트 스레드가 그 뮤텍스를 쥔 채
   죽는 구조이므로, 공유 자체가 위험 표면입니다.
3. 그 뒤 vsync ON A/B로 §2의 32.8%가 실제로 회수되는지 확인합니다.

**이 브랜치의 코드는 그대로 두되 머지하지 않습니다.** 설계와 측정은 유효하므로 다음
시도의 출발점으로 씁니다.

---

# Task 440 Work Log — asynchronous present: measurement settled, implementation unfinished

## 1. Two lines

**The measurement stands.** With vsync on — the configuration the game is played in —
`grBufferSwap` is **32.8% of guest-run**, and **99.95% of that is waiting inside
`SDL_GL_SwapWindow`**. It is the largest single Glide item this project has measured.

**The implementation is unfinished.** The asynchronous path works when it runs, but this branch
**crashes at teardown in roughly half of all runs, with the switch off as well as on**. The cause
is not isolated, so **it is not merged**.

## 2. The measurement

Twenty seconds of attract on Release: vsync on gives 752 swaps at **32.8% of guest-run** and
**32.36M cycles (10.8 ms) per present**, of which the `SDL_GL_SwapWindow` call is **99.95%**;
interval 0 gives the same present at 214K cycles and 2.9%. The guest needs about 17.9 ms to build
a frame and then waits 10.8 ms on top. The game calls `grBufferNumPending` exactly once per swap
(7,276 to 7,276), which is Glide's asynchronous throttle — **our implementation is more
synchronous than the hardware, so this is an accuracy gap as much as a cost.**

## 3. What was built

The bounded FIFO with its drain-before-sync ordering rule, posted paths for `grBufferSwap`,
`grBufferClear` and the draw-batch flush, a truthful `grBufferNumPending`, the one-outstanding-swap
bound with back pressure, and a probe covering ordering, accounting and refusal after close — all
working. A smoke reports posted 2,226 against executed 2,225 with zero failures and zero refusals,
and ordinal 85's per-call cost falls from 44.5M to about 59K cycles, which is the wait leaving the
guest thread.

## 4. Where it stopped

Committed main is **13 for 13 clean**; this branch crashes about half the time at
**0xC0000005 immediately after the 12-second timeout, before the summary prints**, with the
switch off as well as on — so the fault is on an always-on path, not the asynchronous one.

Measured and ruled out, one build each: the attempt struct's growth; the lock `Close` took (the
terminated guest thread owns that mutex); the lazily-created state reached from the guest's own
small stack; the pump's drain while idle; and the teardown snapshot call. What remains untested
are the backend's new members, the out-of-line constructor and destructor, the
`WaitAndPumpHostCommands` predicate, and the boundary's static switch lookup.

## 5. Retrospective: the method was wrong

**After the second failed hypothesis this should have gone to a debugger** for the fault address.
Instead it ran five rounds of guess, patch, rebuild at five minutes of build and two of runtime
each. The eliminations are real, but the same time would have bought the answer directly. One of
those rounds was also **confounded** — when the snapshot call was removed, `Close` was still taking
the lock, so that round eliminated neither candidate and both had to be retested separately.

## 6. Next

Start from a **crash dump**, not another bisection. Then **give the asynchronous queue its own
synchronisation** instead of sharing `host_command_mutex_`: the guest thread is terminated while
holding that mutex, so sharing it is a hazard surface by construction. Only then repeat the
vsync-on A/B to see whether the 32.8% is actually recovered. The branch keeps its code as the
starting point; the design and the measurement remain valid.

## 7. [정정] 축 선택이 잘못됐습니다 — 측정은 vsync OFF로 합니다

사용자 지적: **vsync ON은 게임의 조건이지 측정 조건이 아닙니다.** 성능을 재거나 고칠
때는 vsync OFF로 판정합니다. 이 프로젝트는 Task 371 이래 그렇게 해 왔고, 가이드에도
`REPIU_GLIDE_SWAP_INTERVAL=0`이 필수 조건으로 적혀 있습니다. **제가 그 기준을 어기고**
"플레이 기본값이 vsync ON이니 32.8%가 최대 항목"이라며 축을 골랐습니다.

| 기준 | `grBufferSwap` ÷ guest-run | present 1회 |
|---|---:|---:|
| vsync ON (제가 쓴 조건) | 32.8~35.1% | 10.8 ms |
| **vsync OFF (프로젝트 기준)** | **2.9%** | **0.07 ms** |

vsync ON의 대기는 CPU 작업이 아니라 **디스플레이를 기다리는 유휴 시간**입니다. 실측도
같은 말을 했습니다 — 비동기화가 그 대기를 게스트에서 걷어냈는데도 **프레임은 785 → 782**
로 그대로였고, 대기는 `grDepthMask`로 옮겨갔을 뿐입니다.

**판정: 이 축은 성능 근거가 없습니다.** void setter까지 post를 확장하는 후속 증분도
같은 이유로 진행하지 않습니다 — vsync OFF에는 옮겨 다닐 vblank 대기 자체가 없습니다.

**구현은 opt-in으로 남깁니다.** 근거는 성능이 아니라 정확성입니다: 게임이 swap마다
`grBufferNumPending`을 1:1로 호출하는데 우리는 상수 0을 돌려주고 있었고, 이 경로가
그 계약을 복원합니다. 기본값 승격은 하지 않습니다.

**vsync OFF 기준의 다음 축은 `grTexSource`입니다** — gameplay 실측에서 게이트의 33.5%,
guest-run의 2.81%이고 호출당 51,865 cycle 중 **wake가 36,617(70.6%)** 입니다. 즉 GL
작업이 아니라 왕복이며, 프레임당 20.6회 bind마다 발생합니다.

---

## 7. [Correction] The axis was chosen under the wrong measurement condition

The user's correction: **vsync on is the game's condition, not the measurement condition.**
Performance is measured and judged with vsync **off** — the rule this project has followed since
Task 371, and which the guide states as a precondition. **This design broke that rule**, arguing
from the 32.8% that vsync on produces. Under the project's standard `grBufferSwap` is **2.9% of
guest-run at 0.07 ms per present**, and there is almost nothing to remove.

The vsync-on wait is not CPU work; it is idle time waiting for the display. The measurement said
the same thing: taking that wait off the guest thread moved **frames not at all — 785 to 782** —
because the wait simply relocated to `grDepthMask`.

**Verdict: this axis has no performance case**, and the follow-on increment of posting the void
setters is dropped for the same reason — with vsync off there is no vblank wait to relocate.

**The implementation stays opt-in on accuracy grounds**, not performance: the game polls
`grBufferNumPending` once per swap and we answered a constant zero, so this path restores the
contract. It is not promoted to the default.

**The next axis under the correct standard is `grTexSource`** — 33.5% of the gate and 2.81% of
guest-run in the gameplay measurement, at 51,865 cycles per call of which **36,617 (70.6%) is
`wake`**: a round trip rather than GL work, once per bind, 20.6 times per frame.

