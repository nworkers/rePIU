# Task 747 작업 로그 — Linux x64의 HLE 재진입 뒤 tick 주입

설계: [20260927-747](../design/20260927-747-inject-ticks-after-hle-on-x64.md)
작업 지시: [20260927-747](../work-orders/20260927-747-inject-ticks-after-hle-on-x64.md)

## 요약

swap 페이싱(Task 745)을 켜자 노트 점프와 노이즈가 돌아온 원인은 tick 전달이었습니다. 페이싱은 프레임마다
약 16 ms를 게이트 안에서 자고, 그동안 밀린 240 Hz tick 4개를 게이트 뒤에서 빠르게 빼야 하는데, Linux
x64는 tick을 **메인 루프의 safe point**와 **ISR의 `sti`**에서만 주입해 프레임당 2개밖에 빼지 못했습니다
(HLE 뒤 주입 0회). backlog가 상한 64에 닿아 14초에 936개를 버렸습니다. `HandleAotReentry`의 planner-HLE
분기에 legacy 체인과 같은 `InjectPendingInterrupts` 호출 한 줄을 넣자 `iret` 직후 다음 tick이 중첩 없이
주입되어 dropped가 62(0.4%)로, max-backlog가 13으로 내려갔습니다. 페이싱 플레이 60초에서 MP3 정상
(multi 0, pcm-empty 0), 폴트 0입니다.

## 과정

1. 사용자 보고 뒤 페이싱 실행의 최종 보고: `timer tick delivery … due/injected/dropped/max-backlog:
   2191/1219/913/64`. 페이싱 없는 실행은 dropped 45.
2. A/B: `REPIU_PIC_TIMER_IN_SERVICE=0`이면 dropped 44 — in-service 모델의 `sti` 연쇄 금지가 드레인을
   막는 것처럼 보였지만, 주입 지점 통계(injected 1,728 = safe point 974 + `sti` 754)가 진짜 원인을
   가렸습니다: **HLE 뒤 주입이 0**. x64 cache의 privileged 명령(`iret`, `out`, `cli`)은
   `HandleAotReentry`의 planner-HLE 분기로 처리되는데 그 분기만 legacy 체인의 주입 시도를 빠뜨리고
   있었습니다.
3. 수정 한 줄. 기존 규칙(IF, in-service, `sti` 연쇄 금지)은 그대로이고, `iret` 뒤는 EOI로 in-service가
   풀리고 IF가 복원된 상태라 다음 tick이 중첩 없이 들어갑니다. 주입이 EIP를 ISR 입구로 바꾸면
   `TryResumeAotAfterHandledHle`가 거기로 cache 재진입합니다.
4. in-service 모델을 끄고 페이싱 60초를 돌려도 Task 736 크래시는 재발하지 않았지만(attract는 240 Hz
   단계), 모델은 그대로 둡니다.

## 검증

| 검증 | 결과 |
|---|---|
| 페이싱 attract 14초 (수정 전 → 후) | dropped 936 → 62, max-backlog 64 → 13, injected 1,728 → 2,615 (due 2,677) |
| 페이싱 플레이 60초(합성 키, 단독) | 곡 `39.AUD` 도달, MP3 multi 0·pcm-empty 0, tick dropped 53/14,160, 폴트 0, 3,257 frame |
| 페이싱 attract 60초 | dropped 59/14,041, 폴트 0 |
| 비페이싱(`REPIU_GLIDE_SWAP_INTERVAL=0`) attract 14초·pumpit2a 25초 | dropped 59/2,700, 37/5,753, 폴트 0, 1,600·3,780 frame |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug 빌드 + core probe | 아래 English 절과 같음 |

로그: `build/task747-*.err.log`, `build/task747b-*.err.log`.

## 남은 것

* 게이트 대기 **중**에는 tick이 전달되지 않습니다(실제 기계는 vblank 대기 중에도 IRQ가 옵니다).
  게이트 뒤 ISR 연쇄로 따라잡으므로 시계는 맞지만, 대기 안에서 일어나야 할 폴링(MP3 status, 입력)은
  뒤로 몰립니다. 현재 MP3 큐(0.25초)와 입력에는 여유가 있습니다.
* HLE마다 주입을 시도해 `deferred` 카운터가 크게 늘었습니다(17,034/14초). 카운터일 뿐 비용은 없습니다.

---

# English

# Task 747 work log — injecting ticks after an HLE reentry on Linux x64

Design: [20260927-747](../design/20260927-747-inject-ticks-after-hle-on-x64.md)
Work order: [20260927-747](../work-orders/20260927-747-inject-ticks-after-hle-on-x64.md)

## Summary

The arrow jumps and noise that came back with swap pacing (Task 745) were tick delivery. Pacing
sleeps about 16 ms per frame inside the gate, and the four 240 Hz ticks owed by then have to drain
quickly afterwards, but Linux x64 injected ticks only at the **main loop's safe points** and at the
**ISR's `sti`**, two a frame (zero after an HLE). The backlog hit its cap of 64 and dropped 936 ticks
in 14 s. One line, the same `InjectPendingInterrupts` call the legacy chain has, in
`HandleAotReentry`'s planner-HLE branch injects the next tick right after `iret` without nesting:
dropped fell to 62 (0.4%) and max-backlog to 13. A paced 60 s play has clean MP3 (multi 0,
pcm-empty 0) and no faults.

## Steps

1. The paced run's final report after the user's report: `timer tick delivery …
   due/injected/dropped/max-backlog: 2191/1219/913/64`; unpaced, 45 dropped.
2. A/B: `REPIU_PIC_TIMER_IN_SERVICE=0` gave 44 dropped, which looked like the in-service model's
   `sti` chain rule, but the injection-site counts (1,728 injected = 974 at safe points + 754 at
   `sti`) named the real cause: **none after an HLE**. The x64 cache handles the privileged
   instructions (`iret`, `out`, `cli`) in `HandleAotReentry`'s planner-HLE branch, the one path
   that lacked the legacy chain's injection attempt.
3. The one-line fix. The rules (IF, in-service, no `sti` chaining) stay; after an `iret` the EOI has
   cleared the in-service bit and IF is restored, so the next tick goes in without nesting; when the
   injection moves EIP to the ISR entry, `TryResumeAotAfterHandledHle` reenters the cache there.
4. A paced 60 s run with the in-service model off did not bring back the Task 736 crash (attract is
   the 240 Hz stage), but the model stays on.

## Verification

| Check | Result |
|---|---|
| Paced 14 s attract (before → after) | dropped 936 → 62, max-backlog 64 → 13, injected 1,728 → 2,615 (2,677 due) |
| Paced 60 s play (synthetic keys, standalone) | reaches song `39.AUD`, MP3 multi 0 and pcm-empty 0, 53/14,160 ticks dropped, no faults, 3,257 frames |
| Paced 60 s attract | 59/14,041 dropped, no faults |
| Unpaced (`REPIU_GLIDE_SWAP_INTERVAL=0`) 14 s attract and 25 s pumpit2a | 59/2,700 and 37/5,753 dropped, no faults, 1,600 and 3,780 frames |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug build + core probe | build succeeded, `core_probe_all=true` |

Logs: `build/task747-*.err.log`, `build/task747b-*.err.log`.

## What remains

* No tick is delivered **during** a gate wait (the real machine takes IRQs during its vblank wait).
  The clock catches up through the ISR chain after the gate, but polling that would have happened
  inside the wait (MP3 status, input) bunches up after it; the MP3 queue (0.25 s) and the input
  have the slack today.
* Attempting an injection after every HLE inflated the `deferred` counter (17,034 in 14 s); it is a
  counter, not a cost.
