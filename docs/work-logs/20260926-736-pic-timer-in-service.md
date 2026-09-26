# Task 736 작업 로그 — 타이머 IRQ0 in-service와 `sti` 시점 전달

설계: [20260926-736](../design/20260926-736-pic-timer-in-service.md)
작업 지시: [20260926-736](../work-orders/20260926-736-pic-timer-in-service.md)

## 요약

Task 735 뒤 pumpitea가 Linux x64에서 약 23초 만에 segfault로 끝나던 원인을 찾아 고쳤습니다.
게임이 PIT를 잠시 51.9 kHz로 설정하는 동안, 엔진이 타이머 ISR 안에 다음 tick을 계속 주입해
guest stack이 넘쳤습니다. 엔진이 ISR의 `cli`를 볼 수 없었고(user mode는 IF를 끌 수 없음), PIC의
in-service 비트를 흉내 내지 않았기 때문입니다. in-service 비트, `sti` 시점 전달(연쇄 금지 포함),
주입 frame의 TF 제거를 넣은 뒤 pumpitea는 폴트 없이 240 Hz 단계에 도달하고, pumpit2a의 tick
전달률은 두 host에서 이전 수준입니다.

## 원인 분석

* 사용자 로그: `divisor=23 frequency=51881.739130Hz`(generation 2) 뒤 240 Hz 전환 없이 23초에
  `signal=0xb`. fault stack은 60바이트 frame(`0x0102AB07 0x24 0x200206`, pusha 32 + 세그먼트 16 +
  iret frame 12)의 반복.
* pumpitea ISR `0x0102AAE4`: `pusha`, 세그먼트 push, helper 호출, **`cli`**, 다섯 슬롯 루프
  (`0x0102AB07`이 back-edge), 필요하면 이전 핸들러 chain, EOI `0x0102AB7D`, `sti`, `iret`.
* 반복 frame의 EIP가 `cli` 뒤의 back-edge이고, 저장된 EFLAGS에 IF=1(`0x200206`)이 있습니다. 주입
  판정이 host context의 IF만 봤기 때문입니다.
* 앞서 제가 한 88초 실행은 살아남았습니다. 51.9 kHz 단계가 stack이 넘치기 전에 끝나면 살아남는
  간헐적 결함입니다.

## 반복한 설계와 측정

| 시도 | pumpitea (Linux) | pumpit2a tick 전달 (Linux 30초) | Win32 pumpit2a |
|---|---|---|---|
| 이전 동작 | 사용자 실행 crash, 제 실행 2회 생존 | 5,316/5,445 (97.6%), 정상 상태 초당 ~227 | 정상 |
| v1: in-service 비트만 | 3/3 정상 | 3,969/5,222 (76%), 초당 ~170 — **회귀** | — |
| v2: + `sti` 전달, frame 생존을 ESP로 추적 | — | 1,790/5,553 — 거의 모두 depth 차단 | — |
| v3: + `sti` 연쇄 금지 (sti 핸들러 안에서 직접 주입) | 3/3 정상 | 5,433/5,670 (95.8%), 초당 ~241 | **2/2 single-step 예외** |
| v4: `sti`는 요청만, dispatcher가 소비 | — | — | 2/2 single-step 예외 |
| 최종: + 주입 frame에서 TF 제거 | 3/3 정상 | 5,363/5,705, 5,495/5,712 (94~96%) | 2/2 정상, 5,622/6,155·5,758/6,242 (91~92%) |

* v1의 회귀: pumpit2a ISR은 이전 핸들러로 chain하는 HLE 경계에서 시작하는데, 이전에는 거기서
  (EOI 전) 중첩 주입을 하며 밀린 tick을 따라잡았습니다. 실제 기계처럼 EOI 뒤 `sti`에서 전달하게
  해 되찾았고, 정상 상태 초당 ~241로 오히려 240 Hz에 더 가까워졌습니다.
* v2: `iret` 뒤 중단된 코드가 frame보다 깊이 내려가 frame이 살아 있는 것처럼 보였습니다. ESP로
  생존을 추적하지 않고 "`sti` 전달 두 번 연속 금지"로 바꿨습니다.
* v3~v4의 Win32 예외: 스위치를 꺼(`REPIU_PIC_TIMER_IN_SERVICE=0`) 같은 binary에서 A/B한 결과
  켠 쪽만 24초 무렵 `0x80000004`로 끝났습니다. dispatcher가 HLE 처리 전에 context에 세운 trace
  TF가 주입 frame에 들어가, 핸들러의 native `iret`이 TF를 되살린 것이 원인이었습니다. frame에서
  TF를 지우자 해결되었습니다.

## 검증 (최종)

| 검증 | 결과 |
|---|---|
| core probe `pic_timer_in_service` | Linux x64 32/32, Win32 30/30 |
| pumpitea, Linux x64, 40초 × 3 | 폴트 0, 3/3 240 Hz 전환 도달, tick 96~98% 전달 |
| pumpit2a, Linux x64, 30초 × 2 | 폴트 0, 94~96% 전달 |
| pumpit2a, Win32, 30초 × 2 | 정상 timeout 종료, 91~92% 전달 (스위치 끔 92~93%) |
| 최종 보고 | `timer IRQ0 in-service blocked/sti-delivered/sti-chain-blocked/eoi-cleared/stack-retired/active-at-end` |

pumpitea와 pumpit2a 모두 주입 수와 EOI 수가 같습니다. 두 ISR은 모두 스스로 EOI를 쓰고, stack
fallback은 한 번도 쓰이지 않았습니다.

## 따로 남긴 것

1. **Win32 pumpitea는 이 작업과 무관하게 실패합니다.** 약 8초에 데이터 페이지(`0x049E26C8`,
   `rw-`) 실행으로 `0xC0000005`가 나며, 스위치를 꺼도 같고 tick 주입은 0회입니다.
2. pumpitea의 frame 수는 조금 줄었습니다(35초에 약 880 대 1,000). 전달되는 tick이 늘어
   200회 `in` 지연 루프가 든 ISR이 더 자주 돌기 때문입니다(Task 735 관찰).
3. 이전 동작에서의 crash는 간헐적이라 스위치를 끈 실행으로 재현하지 못했습니다(1회 생존).
   원인은 사용자 로그의 fault stack으로 확인했고, 중첩 한계는 probe로 결정적으로 확인합니다.

---

# English

# Task 736 work log — timer IRQ0 in-service and delivery at `sti`

Design: [20260926-736](../design/20260926-736-pic-timer-in-service.md)
Work order: [20260926-736](../work-orders/20260926-736-pic-timer-in-service.md)

## Summary

After Task 735, pumpitea on Linux x64 ended in a segfault after about 23 s. While the game had the PIT
at 51.9 kHz, the engine kept injecting the next tick inside the timer ISR until the guest stack
overflowed: it could not see the ISR's `cli` (user mode cannot clear IF) and did not model the PIC's
in-service bit. With the in-service bit, delivery at `sti` (with no chaining), and TF removed from
injected frames, pumpitea reaches its 240 Hz phase with no faults, and pumpit2a's tick delivery is at
its previous level on both hosts.

## Analysis

* User log: after `divisor=23 frequency=51881.739130Hz` (generation 2), `signal=0xb` at 23 s with no
  240 Hz switch. The fault stack repeats a 60-byte frame (`0x0102AB07 0x24 0x200206`: pusha 32, segments
  16, iret frame 12).
* pumpitea ISR `0x0102AAE4`: `pusha`, segment pushes, a helper call, **`cli`**, a five-slot loop
  (`0x0102AB07` is its back edge), an optional chain to the previous handler, EOI at `0x0102AB7D`,
  `sti`, `iret`.
* The repeated frames' EIP is the back edge after the `cli`, and their saved EFLAGS has IF=1
  (`0x200206`), because injection consulted only IF in the host context.
* My earlier 88-second run survived: the defect is intermittent, surviving when the 51.9 kHz phase ends
  before the stack overflows.

## Iterations and measurements

See the table above. v1 (in-service bit only) fixed pumpitea but dropped pumpit2a to 76% delivery
(about 170 ticks/s): pumpit2a's ISR begins with the HLE boundary of its chain to the previous handler,
where the old behaviour injected nested ticks before the EOI and so caught up. Delivering after the EOI
at `sti`, as the real CPU does, recovered it, to about 241/s in steady state — closer to 240 Hz than
before. v2 tracked frame liveness from ESP and blocked almost everything, because after `iret` the
interrupted code may run deeper than the frame; it was replaced by "no two `sti` deliveries in a row".
v3 and v4 ended Win32 pumpit2a in `0x80000004` at about 24 s, and an A/B in the same binary with
`REPIU_PIC_TIMER_IN_SERVICE=0` showed only the enabled side failing: the trace TF the dispatcher sets in
the context before HLE handling went into the injected frame and the handler's native `iret` restored
it. Clearing TF from the frame fixed it.

## Verification (final)

Core probe: Linux x64 32/32, Win32 30/30. pumpitea, Linux x64, 3 × 40 s: no faults, all three reach the
240 Hz switch, 96-98% of ticks delivered. pumpit2a, Linux x64, 2 × 30 s: no faults, 94-96%. pumpit2a,
Win32, 2 × 30 s: normal timeout endings, 91-92% (92-93% with the switch off). In both games injections
equal EOIs: both ISRs write their own EOI, and the stack fallback was never used.

## Left separately

1. **Win32 pumpitea fails independently of this task**: about 8 s in, `0xC0000005` executing a data
   page (`0x049E26C8`, `rw-`), the same with the switch off, with zero ticks injected.
2. pumpitea's frame count dropped a little (about 880 against 1,000 in 35 s): more ticks now reach the
   ISR containing the 200-iteration `in` delay loop (Task 735's observation).
3. The old behaviour's crash is intermittent and did not reproduce in a switch-off run (one survived).
   The cause is established from the user log's fault stack, and the nesting bound deterministically
   by the probe.
