# Task 714 작업 로그 — 화면 비교와 x64 safe point 틱 주입

설계: [20260918-714](../design/20260918-714-linux-x64-pacing-safe-point-injection.md) ·
작업 지시: [20260918-714](../work-orders/20260918-714-linux-x64-pacing-safe-point-injection.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260918-713](20260918-713-high-byte-destination-lowering.md)

## 요약

**두 host는 같은 장면을 같은 순서로 그리지만 Linux x64가 느리게 진행합니다.** Linux는
AOT safe point에서 틱을 한 번도 주입하지 못하며, 그 원인 두 가지를 찾아 고쳤습니다.
그러나 고친 주입을 켜면 실행이 불안정해지므로 **opt-in으로 두었고, 기본 동작은
그대로**입니다. "틱 부족이 느린 진행의 원인"이라는 가설은 **확인되지 않았습니다.**

## 관측

`REPIU_GLIDE_PIXEL_DIAG=1`, 30초.

| 스왑 | Win32 | Linux x64 |
|---:|---|---|
| 3–60 | 주황 페이드인, 약 69,000픽셀 | 같은 장면, 스왑당 진행 약 절반 |
| 200 | 두 번째 장면 30,228픽셀 `(191,155,93)` | 아직 페이드 |
| 600 | 이후 장면 | 두 번째 장면 30,264픽셀 `(190,155,93)` |
| 400–1000 | 장면 네 번 전환 | 두 번째 장면에 머묾 |

| 30초 틱 | Win32 | Linux x64 |
|---|---:|---:|
| 도래 | 6,235 | 5,389 |
| 주입 | 5,729 | 1,754 |
| 폐기 | 506 | 3,571 |
| safe point trap / 주입 | 5,719 / 5,667 | 3,859 / **0** |

Linux의 INT 8 주입 수는 20초 1,755, 30초 1,754로 렌더 단계에서 거의 늘지 않습니다.

## 수정 (opt-in)

x64에서 인터럽트 프레임이 항상 게스트 주소를 담도록 했습니다. `eip`가 cache
주소이면 그 바이트가 실행하는 게스트 명령으로 바꿔 선택자 조회와 프레임 양쪽에
씁니다. 이것으로 두 원인 — 선택자 조회 실패(Task 710)와 IRETD HLE의 cache 복귀
주소 거절 — 이 함께 해결되고, 주입을 켠 실행의 첫 크래시(6초, ISR `IRET`)가
사라졌습니다.

그러나 켠 실행은 여전히 죽습니다.

| on 실행 | 결과 |
|---|---|
| 30초 예산 (두 번) | 27–28초에 처리되지 않은 breakpoint(`kind=2`, `si_code=0x80`), `exit=133` |
| 20초 예산 | 20초 전에 SIGSEGV, `exit=139` |

breakpoint는 cache `0x20007D0D`(게스트 `0x010F74A1`)에서 나며, 보고 시점에 그
바이트는 이미 원래 명령(`67 88 01`)으로 돌아가 있습니다 — **일시적 `int3`**입니다.
게스트는 ISR에서 불린 Watcom `itoa`류 루프(`mov al, cs:[edx+table]`)를 실행
중입니다. 런타임에 `int3`를 쓰는 곳은 probe sentinel과 page retirement뿐인데,
이 실행에서는 probe가 꺼져 있었습니다. 원인은 확인하지 못했습니다.

켠 실행의 장면 진행도 **빨라지지 않았습니다**(두 번째 장면 도달: on 스왑 800,
off 600, Win32 200). 크래시 전까지 실제 주입 수는 요약이 남지 않아 측정하지
못했습니다. 그래서 틱 부족 가설은 확인도 반증도 되지 않았습니다.

## 진단 추가

처리되지 않은 Linux 폴트 줄에 `kind=`와 `si_code=`를 더했습니다. 위의
"일시적 `int3`" 판단은 이것으로 가능했습니다.

## 검증

* Linux x64 core probe **30/30**, Win32 x86 core probe **28/28**, Win32 전체 빌드 오류 0
* 기본값(off) Linux 30초: **완주**, 폴트 0, safe point 주입 0/4,000 — 이전과 같음
* on Linux: IRET 크래시(6초) 해소, 이후 27–28초 크래시

## 다음

1. **일시적 `int3` 크래시의 정체.** 누가 그 바이트를 `0xCC`로 썼다가 되돌리는지,
   그리고 왜 그 순간의 breakpoint를 아무 처리기도 받지 않는지.
2. **틱과 장면 진행의 관계.** 주입이 안정적으로 동작해야 가설을 판정할 수 있습니다.

---

## English

Design: [20260918-714](../design/20260918-714-linux-x64-pacing-safe-point-injection.md) ·
Work order: [20260918-714](../work-orders/20260918-714-linux-x64-pacing-safe-point-injection.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260918-713](20260918-713-high-byte-destination-lowering.md)

### Summary

**Both hosts draw the same scenes in the same order, but Linux x64 progresses more
slowly.** Linux injects no ticks at AOT safe points; the two causes were found and
fixed. Turning the fixed injection on destabilizes execution, however, so it is
**opt-in and the default behavior is unchanged**. The hypothesis that tick
starvation causes the slow progression is **not confirmed**.

### Observation

Under `REPIU_GLIDE_PIXEL_DIAG=1` for 30 seconds, both hosts show the same orange
fade-in (about 69,000 pixels), Linux advancing about half as far per swap; Win32
reaches the second scene (30,228 pixels, `(191,155,93)`) at swap 200 and Linux at
swap 600; Win32 then changes scene four more times while Linux stays put. Ticks
come due at similar rates (6,235 vs 5,389), but Win32 injects 5,729 (5,667 at safe
points) and Linux 1,754 (**0** of 3,859 safe-point traps), dropping 3,571. Linux's
INT 8 count barely moves during rendering (1,755 at 20 s, 1,754 at 30 s).

### Fix (opt-in)

On x64 the interrupt frame now always holds a guest address: a cache `eip` is
replaced by the guest instruction it executes, for the selector lookup and the
frame. That resolves both causes — the failed selector lookup (Task 710) and the
IRETD HLE refusing a cache return address — and removes the first crash with
injection on (6 s, on the ISR's `IRET`).

Runs with injection on still die: twice at 27–28 seconds on an unhandled
breakpoint (`kind=2`, `si_code=0x80`, `exit=133`), and once before 20 seconds on a
SIGSEGV (`exit=139`). The breakpoint is at cache `0x20007D0D` (guest
`0x010F74A1`), and by the time it is reported the byte is the original instruction
again (`67 88 01`) — a **transient `int3`** — while the guest runs a Watcom
`itoa`-style loop called from the ISR (`mov al, cs:[edx+table]`). The only runtime
writers of `int3` are the probe sentinel and page retirement, and the probe was off.
The cause is not established.

Scene progression with injection on was **not faster** (second scene at swap 800
on, 600 off, 200 on Win32), and the number of injections before the crash could not
be measured because no summary was written. The tick-starvation hypothesis is
therefore neither confirmed nor refuted.

### Added diagnostic

Unhandled Linux fault lines gained `kind=` and `si_code=`, which is what made the
"transient `int3`" reading above possible.

### Verification

Linux x64 core probe **30 of 30**, Win32 x86 **28 of 28**, and the full Win32 build
had no errors. A default (off) 30-second Linux run **completes** with no faults and
0 of 4,000 safe-point injections, as before. With injection on, the 6-second IRET
crash is gone and the 27–28-second crash remains.

### Next

1. **What the transient `int3` is**: who writes that byte to `0xCC` and back, and why
   no handler claims the breakpoint at that moment.
2. **How ticks relate to scene progression**, which can be judged only once
   injection is stable.
