# Task 510 작업 지시 — Linux 속도의 첫 갈래

설계: [20260828-510](../design/20260828-510-linux-speed-first-split.md) ·
작업 로그: [20260828-510](../work-logs/20260828-510-linux-speed-first-split.md)

## 0. 코드를 쓰지 마십시오

이 작업은 **측정만** 합니다. 귀속 계측을 만들고 싶어지면 그것이 이 작업의 결론이지 수단이
아닙니다 — 먼저 공짜인 것을 다 써 보고, 남으면 그때 만듭니다.

## 1. 벽을 먼저 확인하십시오

```bash
grep -n "guest thread has stopped" src/platform/win32/telemetry/live_telemetry_snapshot.cpp
```

`CopyThreadObservationToAttempt`가 귀속 계측 전부를 `attempt`로 옮기고, 그 주석이 전제를
적어 두었습니다. Linux에서 렌더까지 간 실행은 `stopped=0`이라 거기에 닿지 못합니다.
**이것을 확인하지 않고 프로파일을 켜면 빈 로그를 보고 원인을 찾게 됩니다.**

## 2. 이미 있는 노브만으로 A/B 하십시오

기준선은 509의 값입니다. 판정 도구는 `[repiu-shutdown]` 줄의 `frames=`·`span_ms=`입니다.

| 변인 | 무엇을 바꾸나 |
|---|---|
| `REPIU_GLIDE_RENDEZVOUS_SPIN_US=2000` | rendezvous 대기 방식 |
| `REPIU_GLIDE_ASYNC_PRESENT=1` | present를 임계 경로에서 뺌 |
| 위 둘 동시 | 가산성 |
| `REPIU_GLIDE_SETTER_ELIDE=0` | 호스트 왕복 **횟수** |
| `REPIU_GLIDE_DRAW_BATCH=0` | 게이트 **크로싱 횟수** |

조건은 509의 것을 그대로 씁니다 — Release, vsync OFF, 감시견 OFF, `pumpit1`, 90초, 3회.

## 3. 널 결과를 만나면 대조군을 돌리십시오

**이것이 이 작업에서 가장 중요한 지시입니다.**

노브를 바꿨는데 아무 일도 없으면 두 가지 뜻이 있습니다 — **축이 아니거나, 노브가 이 장면에서
아무 일도 안 했거나.** Linux에서는 그 둘을 가를 census가 위의 벽 뒤에 있습니다.

그러면 **같은 노브를 Windows에서 돌리십시오.** Windows는 요약이 나오고, 무엇보다 fps가
움직이는지로 노브의 작동 여부를 알 수 있습니다. 움직이면 노브는 살아 있고 Linux의 널 결과가
의미를 갖습니다.

## 4. fps가 아니라 프레임당 ms로 비교하십시오

배율이 26.8배이므로 **같은 절대 비용이 한쪽에서는 37%, 다른 쪽에서는 1%로 보입니다.** 백분율만
보면 "Linux에서는 공짜"라는 잘못된 결론에 이릅니다. 항상 `1000/fps`로 바꿔 **ms 차이**를
보십시오.

## 5. 문서

* frontier 4절의 "그다음은 귀속입니다"에 결과를 넣고, 다음 단위를 명명하십시오.
* 배제된 후보를 **배제됐다고** 적으십시오. 다음 사람이 같은 A/B를 다시 돌리지 않도록.

---

# Task 510 work order — the first split of Linux's speed

Design: [20260828-510](../design/20260828-510-linux-speed-first-split.md) ·
Work log: [20260828-510](../work-logs/20260828-510-linux-speed-first-split.md)

## 0. Do not write code

This task **only measures**. If you find yourself wanting to build an attribution instrument, that is
this task's conclusion rather than its method -- exhaust what is free first, and build only if
something is left over.

## 1. Confirm the wall first

```bash
grep -n "guest thread has stopped" src/platform/win32/telemetry/live_telemetry_snapshot.cpp
```

`CopyThreadObservationToAttempt` moves every attribution profile into `attempt`, and its comment
states the premise. A Linux run that reaches rendering has `stopped=0` and never gets there.
**Turning a profile on without checking this means hunting a cause in an empty log.**

## 2. A/B with the knobs that already exist

The baseline is 509's. The instrument is `frames=` and `span_ms=` on the `[repiu-shutdown]` line.

| Variable | What it changes |
|---|---|
| `REPIU_GLIDE_RENDEZVOUS_SPIN_US=2000` | how the rendezvous waits |
| `REPIU_GLIDE_ASYNC_PRESENT=1` | takes present off the critical path |
| both together | additivity |
| `REPIU_GLIDE_SETTER_ELIDE=0` | the **number** of host round trips |
| `REPIU_GLIDE_DRAW_BATCH=0` | the **number** of gate crossings |

Conditions are 509's, unchanged: Release, vsync off, watchdog off, `pumpit1`, 90 seconds, three runs.

## 3. When you get a null result, run the control

**This is the most important instruction in this order.**

A knob that changes nothing means one of two things -- **it is not the axis, or the knob did nothing
in this scene.** On Linux the census that would separate them is behind the wall above.

So **run the same knob on Windows.** Windows prints its summary, and more to the point its fps moves
if the knob is doing anything at all. If it moves there, the knob is alive and the Linux null result
means something.

## 4. Compare in ms per frame, not in fps

The factor is 26.8x, so **the same absolute cost shows up as 37% on one host and 1% on the other.**
Reading percentages alone leads to "it is free on Linux", which is wrong. Always convert with
`1000/fps` and compare the **millisecond** difference.

## 5. Documentation

* Put the result into the frontier's "After this, attribution" section and name the next unit.
* Write down what was **ruled out**, as ruled out, so the next person does not repeat the same A/B.
