# Task 747: Linux x64의 HLE 재진입 뒤에도 tick을 주입한다 — 페이싱 아래 backlog 넘침

## 한국어

### 배경

Task 745의 swap 페이싱을 켜자(`swap_interval = 1`) 노트 점프와 오디오 노이즈가 돌아왔다는 보고다.
페이싱은 `grBufferSwap` 게이트 안에서 프레임마다 약 16 ms를 자므로, 그동안 guest 스레드가 멈추고
240 Hz 타이머 tick이 4개씩 밀린다. 그 자체는 실제 기계의 vblank 대기와 같지만(대기 중에도 IRQ는
온다), 밀린 tick을 게이트 뒤에서 빠르게 전달하지 못하면 backlog(상한 64)가 넘쳐 tick을 버린다.
측정(attract 14초, 페이싱):

| | due | injected | dropped | max-backlog |
|---|---|---|---|---|
| 페이싱 없음 | 2,388 | 2,343 | 45 | 17 |
| 페이싱, 기본 | 2,725 | 1,728 | **936** | 64 |
| 페이싱, `REPIU_PIC_TIMER_IN_SERVICE=0` | 2,892 | 2,848 | 44 | 12 |

주입 지점 통계가 원인을 가른다. 페이싱 기본에서 주입 1,728 = safe point 974 + `sti` 754, 즉 **HLE
뒤 주입이 0**이다. Linux x64의 cache는 ISR의 `iret`·`out`·`cli` 같은 privileged 명령을 `HandleAotReentry`의
planner-HLE 분기로 처리하는데, 그 분기는 Win32/legacy의 HLE 체인(`HandlePrivilegedTrapInstruction` 뒤
`InjectPendingInterrupts`)과 달리 HLE 뒤에 주입을 시도하지 않는다. 그래서 x64의 tick은 (1) 메인
루프가 safe point를 지날 때, (2) ISR 안의 `sti`에서(연쇄 금지로 격번) 만 전달되고, 페이싱으로 메인
루프가 프레임당 safe point 하나 정도만 지나면 프레임당 tick 2개 = 240 Hz의 절반이라 backlog가 넘친다.
in-service 모델을 끄면 ISR 안의 safe point에서 중첩 주입되어 빠지지만, 그것은 Task 736이 막은
중첩(51.9 kHz 단계의 stack overflow)을 다시 여는 것이다.

실제 기계에서 밀린 IRQ는 ISR의 `iret` 직후(in-service 해제 + IF 복원) 곧바로 온다. 중첩 없이,
ISR이 끝나는 대로 하나씩.

### 설계

1. `HandleAotReentry`의 planner-HLE 분기에서 `DispatchGuestHleInstruction`이 처리한 뒤, resume 전에
   `InjectPendingInterrupts(win32_context, context)`를 부른다(legacy 체인과 같은 자리). 기존 규칙
   (IF, in-service, `sti` 연쇄 금지, cache 주소→guest 주소)은 그대로다. `iret` 뒤에는 in-service가
   EOI로 해제돼 있고 IF가 복원돼 있으므로 다음 tick이 **중첩 없이** 바로 주입된다. 주입이 EIP를 ISR
   입구로 바꾸면 `TryResumeAotAfterHandledHle`가 그 주소로 cache에 재진입한다.
2. 그 밖의 HLE(`out`, DOS 호출, 포트 I/O) 뒤에도 같은 시도가 일어나며, 이는 Win32가 이미 하는
   것이다.
3. 페이싱(Task 745)은 그대로 둔다. 게이트 대기가 길어져도 tick은 게이트 뒤 ISR 연쇄로 빠진다.

### 검증 전략

페이싱 attract 14초에서 dropped가 45 수준으로 내려가고 max-backlog가 작아지는지, 페이싱 60초·플레이
60초에서 폴트 0·MP3 정상·노트가 부드러운지(사용자 확인), 페이싱 없는 실행과 pumpit2a가 그대로인지,
core probe(Linux·Win32)가 통과하는지 본다.

## English

### Background

Turning on Task 745's swap pacing (`swap_interval = 1`) brought back the arrow jumps and audio noise.
Pacing sleeps about 16 ms per frame inside the `grBufferSwap` gate, during which the guest thread is
stopped and four 240 Hz ticks pile up. That much is like a real vblank wait (IRQs still arrive during
it), but if the owed ticks cannot be delivered quickly after the gate, the backlog (cap 64) overflows
and ticks are dropped. Measured (14 s attract, paced): no pacing 2,388 due / 2,343 injected / 45
dropped / backlog 17; paced 2,725 / 1,728 / **936** / 64; paced with
`REPIU_PIC_TIMER_IN_SERVICE=0` 2,892 / 2,848 / 44 / 12.

The injection-site counts settle it: paced, injected 1,728 = 974 at safe points + 754 at `sti`,
that is **zero after an HLE**. The Linux x64 cache handles the ISR's privileged instructions (`iret`,
`out`, `cli`) through `HandleAotReentry`'s planner-HLE branch, which, unlike the Win32/legacy HLE
chain (`InjectPendingInterrupts` after `HandlePrivilegedTrapInstruction`), never attempts an injection
after the HLE. So x64 delivers ticks only (1) when the main loop passes a safe point and (2) at the
ISR's `sti` (every other one, by the chain rule); with pacing the main loop passes about one safe
point per frame, giving two ticks a frame, half of 240 Hz, and the backlog overflows. Turning the
in-service model off drains through nested injections at safe points inside the ISR, which reopens
the nesting Task 736 closed (the stack overflow at the 51.9 kHz stage).

On the real machine an owed IRQ arrives right after the ISR's `iret` (in-service cleared, IF
restored): one at a time, without nesting.

### Design

1. In `HandleAotReentry`'s planner-HLE branch, call `InjectPendingInterrupts(win32_context,
   context)` after `DispatchGuestHleInstruction` handled the instruction and before the resume, the
   same place the legacy chain has it. The existing rules (IF, in-service, no `sti` chaining, cache
   address → guest address) stay. After an `iret` the in-service bit is already cleared by the EOI
   and IF is restored, so the next tick is injected **without nesting**; when the injection moves
   EIP to the ISR entry, `TryResumeAotAfterHandledHle` reenters the cache there.
2. The same attempt follows other HLEs (`out`, DOS calls, port I/O), as it already does on Win32.
3. Pacing (Task 745) stays; a long gate wait now drains its ticks through a chain of ISRs after it.

### Verification strategy

Paced 14 s attract: dropped back near 45 and a small max-backlog; paced 60 s and a 60 s play: no
faults, MP3 clean, arrows smooth (user); unpaced runs and pumpit2a unchanged; core probes (Linux,
Win32) pass.

---

## 결과 (구현 후) / Result (after implementation)

### 한국어

설계대로 한 줄을 넣었습니다. 페이싱 attract 14초에서 dropped 936 → 62, max-backlog 64 → 13. 페이싱
플레이 60초는 곡 도달·MP3 정상·폴트 0. 설계와 달라진 점은 없습니다. 게이트 대기 **중**의 tick 전달은
남은 과제입니다(작업 로그 "남은 것").

### English

The one line went in as designed. Paced 14 s attract: dropped 936 → 62, max-backlog 64 → 13; a paced
60 s play reaches the song with clean MP3 and no faults. No departures from the design. Delivering
ticks **during** a gate wait remains open (work log, "What remains").
