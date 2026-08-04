# Tasks 421~423 작업 로그 — 음악 위치는 정확했고, **정지는 우리 감시였습니다**

설계: [421](../design/20260805-421-cd-audio-position-reporting.md) ·
[422](../design/20260805-422-mscdex-command-trace.md) ·
[423](../design/20260805-423-cd-audio-stop-idempotency.md)

## 1. 한 줄 결과

사용자가 보고한 **노트·BGA 점프**의 원인은 음악 위치 보고가 아닙니다 — 위치는 gameplay
구간에서도 **오차 없이 정확**합니다. 조사 중 **"플레이 직전 종료"의 정체가 게임의 정지가
아니라 우리 실행 감시**임이 확인됐고, 감시를 끄자 같은 빌드가 **174초·8,023프레임으로
gameplay까지 진행**했습니다.

## 2. 무엇을 만들었나

| Task | 계측/변경 | 상태 |
|---|---|---|
| 421 | CD 오디오 위치 census(`REPIU_CD_AUDIO_POSITION_CENSUS`) — poll 스레드 표본, worker 반복 횟수·underrun 동반 | 동작 불변 |
| 422 | MSCDEX 명령 trace(`REPIU_MSCDEX_COMMAND_TRACE`) — 명령·인자·**우리가 준 응답**·그 순간 위치 | 동작 불변 |
| 423 | `Stop()` 멱등화 | 동작 변경(수정) |

## 3. 확인됨 — 음악 위치는 정확합니다

감시를 끈 174초 실행(gameplay 도달)에서 **generation이 같은 연속 표본 1,440개**:

| 지표 | 값 |
|---|---|
| 평균 `delta_lba` | **8.21** |
| 기대값 | **8.21** |
| 범위 | 7~9 (이상치 0) |
| 역행 | **0** |

**기대값 정정:** 설계와 가이드에 "100 ms당 7~8"이라고 적었으나, `GetTickCount` 분해능
때문에 실제 표본 간격이 **109.5 ms**입니다. 75 프레임/초 × 0.1095초 = **8.21**이 정확한
기대값이며, 관측값이 여기에 일치합니다. **7~8이라는 기준 자체가 틀렸습니다.**

underrun은 174초 동안 미미했고 worker 반복도 정상 범위였습니다. 설계 §2의 후보 A~E는
**전부 기각**입니다.

## 4. 확인됨 — 명령 폭주의 정체는 `stop`이고, 거기서 결함 하나가 나왔습니다

Task 422 trace가 잡은 순서입니다.

```
3125  stop → ioctl 11 ×3 → play(2952, 1811)     ← 정상 프리뷰
5063  stop → ioctl 11 ×3 → play(4763, 1583)     ← 정상 프리뷰
7078  stop
7453~ stop, stop, stop … 16 ms마다, 다른 명령 없음
```

전부 `success=1`이었고, **게스트는 이 구간에서 CD 상태를 한 번도 조회하지 않습니다**
(IOCTL 12·15 모두 없음). 따라서 사전 등록한 판정표의 "응답 내용 불만" 분기는 성립하지
않으며, **Stop은 루프의 탈출 조건이 아니라 루프가 매 반복 습관적으로 부르는 것**입니다.

다만 그 폭주가 실제 결함 하나를 드러냈습니다 — `Stop()`이 매번 `paused`를 재계산해
**두 번째 Stop이 첫 Stop이 세운 pause를 지우고** 있었습니다(Task 423). 수정 후
`paused`는 1로 유지되고 `generation`은 5에 고정됩니다(이전 62).

## 5. **정정 — 정지는 게임 버그가 아니라 우리 감시입니다**

게스트 코드를 읽어 루프를 특정했습니다.

```
0x0302C588:  call 0x0303F3F4        ; return [0x0328FA18]
0x0302C58D:  cmp  eax, 100
0x0302C590:  jl   0x0302C588        ; 100틱까지 대기
```

그 카운터를 올리는 곳은 **INT 8 타이머 ISR**입니다.

```
0x0303F1A3:  call [0x0328FA08]      ; 이전 핸들러 체인
0x0303F1A9:  inc  [0x0328FA18]      ; 게임이 기다리는 카운터
0x0303F1AF:  inc  [0x0328FA14]
```

즉 게임은 **정상적인 100틱(약 1.4초) 대기** 중이었습니다. 그 대기 동안 예외·single
step·AOT 경계가 발생하지 않으므로 `PollThreadUntilExit`의 **1초 무진행 감시**
(`live_telemetry_snapshot.cpp`, `quiet_timeout_milliseconds = 1000`)가 실행을 종료시킵니다.

`REPIU_EXECUTION_TIMEOUT_MS=0`은 `INFINITE`로 해석되어 그 감시를 끕니다. 같은 빌드로 A/B:

| 조건 | 실행 시간 | 프레임 | 도달 지점 |
|---|---:|---:|---|
| 감시 켬 | 9.5초 | 363 | 곡 선택 |
| **감시 끔** | **174초** | **8,023** | **gameplay**(`b15.mcf`, `b14~16.ski`, `rage.con`) |

**따라서 frontier 항목 1′는 게임의 정지가 아니라 계측 도구의 결함입니다.**

**정정 2:** 여러 Task에서 "정지 지점"으로 불러 온 `0x030F2786`은 실은 **`STI`(`fb c3` =
`STI; RET`)** 이며, 대기 중 **가장 자주 트랩되는 특권 명령**일 뿐입니다. Task 420이
이 주소를 결정적 정지 지점으로 읽은 것과, 이 세션 초반의 제 같은 해석을 여기서
정정합니다.

**정정 3:** single-step hotspot census에는 이 루프가 **나오지 않습니다** — 그 census는
single-step 경계만 담는데 루프는 AOT 캐시에서 돌기 때문입니다. 루프를 찾은 것은
**guest position census**(예외와 무관한 5 ms 표본)였습니다.

## 6. 새 유력 후보 — 노트 점프는 타이머 손실입니다

음악은 실시간으로 정확히 흐르는데(§3), **게스트의 틱 카운터는 손실됩니다.**

```
timer tick delivery due/injected/coalesced/dropped: 41531/39830/1677/24   (174초)
```

174초 동안 **1,677회 coalesced** — 여러 due 틱을 한 번으로 합쳐 전달합니다. 게스트
시간이 실제보다 느리게 흐르다가 몰아서 따라잡으므로, 노트·BGA가 틱 기반이라면 **음악
대비 밀렸다가 튀는** 정확히 그 증상이 됩니다. **미측정**이며 다음 세션의 대상입니다.

## 7. 다음 세션이 할 일

1. **무진행 감시 수정(선행).** 지금은 정상 실행을 죽이므로 **모든 프레임 기반 측정이
   왜곡**됩니다. 진행 판정에 HLE 활동(MSCDEX·DOS 서비스)을 포함시키거나, 게스트가 틱
   대기 중임을 인정해야 합니다. 그 전까지 측정은 `REPIU_EXECUTION_TIMEOUT_MS=0`로
   우회하고 하니스에서 시간을 제한하십시오.
2. **틱 coalescing 조사.** §6이 노트 점프의 직접 후보입니다. 시작점은 coalesce가
   언제·왜 일어나는지와, 게스트 틱 카운터가 실시간 대비 얼마나 뒤처지는지입니다.
3. **Task 420 회귀 확인 재실행.** 감시 때문에 미완으로 남아 있습니다.
4. **가이드 기대값 정정 반영.** `delta_lba` 기대값은 7~8이 아니라 **8.21**입니다(§3).

## 8. 회고

* **"멈췄다"를 의심한 것이 맞았습니다.** DOS trace 수를 비교해 "덜 간 것이 아니라 더 간
  것"임을 본 것이 출발점이었고, 마지막엔 아예 "멈추지도 않았다"로 끝났습니다.
* **판정표를 미리 쓴 것이 두 번 값을 했습니다.** 위치 census는 A~E를 한 번에 기각했고,
  명령 trace는 "응답 불만" 가설을 스스로 반증했습니다.
* **기대값을 틀리게 적어 뒀던 것을 측정이 잡았습니다.** 8.21이 정확한 값인데 7~8로
  적어 두었으므로, 만약 관측이 8.2였다면 "약간 빠름"으로 오독할 뻔했습니다.
* **도구의 한계를 문서가 이미 경고하고 있었습니다.** hotspot census가 single-step
  경계만 담는다는 것은 가이드에 적혀 있었고, 그것을 잊고 한 번 헛짚었습니다.

---

# Tasks 421-423 Work Log — the position was right; **the stall was our own watchdog**

## 1. Result in one line

The reported **note and BGA jumping** is not caused by music position reporting — the position
is **exact**, even in gameplay. Along the way the **"termination right before gameplay" turned
out to be our execution watchdog rather than a stall in the game**: with it disabled, the same
build ran **174 seconds and 8,023 frames, reaching gameplay**.

## 2. What was built

Task 421 added a CD audio position census sampled on the poll thread with worker iterations and
underruns; Task 422 added an MSCDEX command trace carrying each command, its arguments, **the
answer we returned**, and the position at that moment; both change no behaviour. Task 423 fixed
`Stop()`.

## 3. Confirmed — the position is exact

Over 1,440 consecutive same-generation samples in the 174-second gameplay run, the mean
`delta_lba` is **8.21** against an expected **8.21**, ranging 7 to 9 with no outliers and **zero
regressions**. **The expectation itself needed correcting**: the design and guide said "7 to 8
per 100 ms", but `GetTickCount` resolution makes the real interval **109.5 ms**, so 75 frames
per second gives **8.21**. Candidates A through E are all rejected.

## 4. Confirmed — the storm is `stop`, and it exposed one real defect

The trace shows two healthy previews (`stop → ioctl 11 ×3 → play`) and then, from 7.453 s,
`stop` every 16 ms with no other command — all answered `success=1`, and **the guest never
queries CD state in that window**, so the pre-registered "unsatisfied with our answer" branch
does not apply: Stop is not the loop's exit condition but something the loop calls every
iteration. It did expose a defect: `Stop()` recomputed `paused` each call, so the second Stop
cleared the first one's pause. After Task 423, `paused` holds at 1 and `generation` stays at 5
against 62 before.

## 5. **Correction — the stall is our watchdog, not the game**

Reading the guest code found the loop: `call 0x0303F3F4; cmp eax, 100; jl` — a wait for
`[0x0328FA18]` to reach 100, a counter incremented by the **INT 8 timer ISR** at
`0x0303F1A9`. It is an ordinary **100-tick (~1.4 s) wait**, during which no exception, single
step or AOT boundary occurs, so `PollThreadUntilExit`'s **one-second no-progress watchdog**
ends the run. `REPIU_EXECUTION_TIMEOUT_MS=0` maps to `INFINITE` and disables it: the same build
then ran **174 s and 8,023 frames into gameplay** against 9.5 s and 363 frames. **Frontier item
1' is an instrument defect, not a game stall.**

Two further corrections. `0x030F2786`, called the stall site across several tasks, is `STI`
(`fb c3`) — merely the privileged instruction trapped most often during the wait; Task 420's
reading and my own earlier one are corrected here. And the single-step hotspot census **cannot
see this loop**, because it samples only single-step boundaries while the loop runs from the
AOT cache; the **guest position census** found it.

## 6. New leading candidate — the note jumping is timer loss

Music advances in real time and correctly, but the guest's tick counter loses ticks:
`due/injected/coalesced/dropped: 41531/39830/1677/24` over 174 seconds. **1,677 coalesced**
deliveries mean guest time runs slow and then catches up in bursts, which is exactly what
note and BGA timing driven by ticks would show. **Unmeasured**, and the next session's target.

## 7. Next session

Fix the no-progress watchdog **first**, since it currently kills healthy runs and distorts every
frame-based measurement — until then, measure with `REPIU_EXECUTION_TIMEOUT_MS=0` and bound the
run from the harness. Then investigate tick coalescing as the direct candidate for the note
jumping, re-run Task 420's regression check, and carry the corrected `delta_lba` expectation of
**8.21** into the guide.

## 8. Retrospective

Doubting "it stalled" paid twice: first the DOS trace counts showed the game getting *further*,
and finally that it had not stalled at all. Pre-registered readings paid twice as well — the
census rejected five candidates at once, and the trace refuted its own leading hypothesis. The
measurement also caught an expectation that had been written down wrong, which would have made
a correct 8.21 look "slightly fast". And the guide had already warned that the hotspot census
only holds single-step boundaries; forgetting that cost one wrong turn.
