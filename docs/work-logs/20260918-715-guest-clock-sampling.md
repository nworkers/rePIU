# Task 715 작업 로그 — 게스트 시계를 벽시계와 함께 기록하기

설계: [20260918-715](../design/20260918-715-guest-clock-sampling.md) ·
작업 지시: [20260918-715](../work-orders/20260918-715-guest-clock-sampling.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260918-714](20260918-714-linux-x64-pacing-safe-point-injection.md)

## 요약

**Linux x64의 게임 시계는 느린 것이 아니라, 렌더 단계에 들어가면 멈춥니다.** 로딩
중에는 Win32와 같은 속도로 가다가 16–17초에 정지하고, Task 714의 opt-in safe point
주입을 켜면 멈추지 않고 Win32와 비슷한 속도로 계속 갑니다. Task 714가 미확정으로
남긴 가설이 더 강한 형태로 확인됐습니다.

## 측정

`REPIU_LIVE_GUEST_PEEK=0x28FA0C,6`, 30초. 표의 값은 INT 8 ISR이 모든 틱에서 올리는
카운터(runtime offset `0x28FA20`)입니다.

| 시각(초) | Win32 | Linux (기본) | Linux (주입 on) |
|---:|---:|---:|---:|
| 5 | 82 | 0 | 0 |
| 8 | 664 | 142 | 202 |
| 12 | 1,622 | 1,071 | 1,135 |
| 15 | 2,347 | 1,728 | 1,825 |
| 17 | 2,827 | **1,755** | 2,284 |
| 20 | 3,548 | **1,755** | 2,972 |
| 25 | 4,747 | **1,755** | 4,129 |
| 29 | 5,711 | **1,755** | 5,053 |

* Win32: 5초부터 초당 약 240으로 꾸준히 증가
* Linux 기본: 7–15초에 초당 약 231(Win32와 같은 속도)로 오르다가 **17초부터 정지**
* Linux 주입 on: 29초까지 초당 약 231로 계속 증가, 30초 무렵 크래시(`exit=133`)

Linux가 Win32보다 2–3초 늦게 출발하는 것은 로딩(자산 디코드)이 느리기 때문이고,
출발한 뒤의 속도는 같습니다.

## 해석

로딩 중에는 게스트가 DOS HLE와 게이트를 자주 지나고, 그 지점에서 틱이 주입됩니다.
렌더가 시작되면 게스트는 AOT 코드와 Glide 게이트만 오가고, Win32가 틱을 넣는 경로는
거의 전부 AOT safe point입니다(Task 714: 5,667 / 5,729). Linux에서는 그 경로가
막혀 있으므로 시계가 멈춥니다. Task 714에서 본 "장면 진행이 느림"의 원인이 이것이고,
장면이 조금이라도 넘어간 것은 게임의 장면 진행 일부가 프레임 단위이기 때문으로
보입니다(미확인).

## 코드 변경과 검증

`REPIU_LIVE_GUEST_PEEK=<runtime offset>[,개수]` — live telemetry 옆에 게스트 dword를
최대 16개 기록합니다. 설정하지 않으면 아무 일도 하지 않습니다.

* Linux x64 core probe **30/30**, Win32 x86 core probe **28/28**, Win32 전체 빌드 오류 0

## 주소를 틀렸던 일

처음에는 파일의 원시 operand `0x17FA20`을 object 4 relocation base(`0x120000`)
기준으로 환산해 `0x16FA20`을 읽었고, 두 host 모두 30초 내내 0이었습니다. ISR이
Win32에서 5,700번 넘게 돈 것은 확실했으므로 환산이 틀린 것이었고, **재배치된 코드를
실행 중에 읽어**(`ff 05 20 fa 28 04` = `incl [0x0428FA20]`) 원시 값이 object 4 안의
offset임을 확인했습니다. 파일 바이트에서 주소를 추정하지 말고 실행 중인 코드에서
읽어야 합니다.

## 다음

**safe point 주입의 크래시**(Task 714의 일시적 `int3`)가 이제 유일한 장애물입니다.
주입이 게임 시계를 되살린다는 것이 확인됐으므로, 이 크래시를 고치는 것이 Linux
x64 렌더 단계를 바르게 진행시키는 길입니다.

---

## English

Design: [20260918-715](../design/20260918-715-guest-clock-sampling.md) ·
Work order: [20260918-715](../work-orders/20260918-715-guest-clock-sampling.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260918-714](20260918-714-linux-x64-pacing-safe-point-injection.md)

### Summary

**The Linux x64 game clock is not slow — it stops once rendering begins.** It runs
at Win32's rate during loading, halts at 16–17 seconds, and with Task 714's opt-in
safe-point injection on it keeps running at a rate close to Win32's. The hypothesis
Task 714 left unconfirmed is confirmed in a stronger form.

### Measurement

`REPIU_LIVE_GUEST_PEEK=0x28FA0C,6` over 30 seconds, reading the counter the INT 8
ISR increments on every tick (runtime offset `0x28FA20`). Win32 climbs about 240 per
second from 5 seconds on, reaching 5,711 at 29 s. Linux by default climbs about 231
per second from 7 to 15 seconds — the same rate — and then **stops at 1,755 from 17
seconds on**. Linux with injection on keeps climbing about 231 per second to 5,053 at
29 s and crashes near 30 s (`exit=133`). Linux starts two to three seconds later
because its loading (asset decode) is slower; once started, its rate matches.

### Interpretation

During loading the guest passes through DOS HLE and gates often, and ticks are
injected there. Once rendering starts the guest moves between AOT code and Glide
gates, and on Win32 almost every tick enters through an AOT safe point (5,667 of
5,729 in Task 714). On Linux that path is blocked, so the clock stops. That is the
cause of the slow scene progression seen in Task 714; that scenes advanced at all is
presumably because part of the game's progression is per frame (unconfirmed).

### Code change and verification

`REPIU_LIVE_GUEST_PEEK=<runtime offset>[,count]` logs up to 16 guest dwords beside
live telemetry and does nothing when unset. Linux x64 core probe **30 of 30**, Win32
x86 **28 of 28**, full Win32 build with no errors.

### Getting the address wrong

The first attempt converted the file's raw operand `0x17FA20` through object 4's
relocation base (`0x120000`), read `0x16FA20`, and saw zero on both hosts for 30
seconds. The ISR had certainly run over 5,700 times on Win32, so the conversion was
wrong; **reading the relocated code at run time** (`ff 05 20 fa 28 04` =
`incl [0x0428FA20]`) showed the raw value to be an offset inside object 4. Addresses
should be read from running code, not inferred from file bytes.

### Next

**The safe-point injection crash** (Task 714's transient `int3`) is now the only
obstacle. With injection confirmed to restore the guest clock, fixing that crash is
what makes the Linux x64 render phase progress correctly.
