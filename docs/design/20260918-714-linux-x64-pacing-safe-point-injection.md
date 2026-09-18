# Task 714 설계 — 화면 비교와 x64 safe point 틱 주입

## 목적

Task 713으로 Glide gate 96개가 두 host에서 일치한다. 다음 질문은 **화면에 실제로
무엇이 그려지는가**다. 기존 `REPIU_GLIDE_PIXEL_DIAG`(스왑마다 back buffer의
non-black 픽셀 수와 평균 색)로 두 host를 비교한다.

## 관측

같은 장면이 같은 순서로 그려지지만 **Linux가 느리게 진행한다.**

| 스왑 | Win32 | Linux x64 |
|---:|---|---|
| 3–60 | 주황 페이드인(약 69,000픽셀, 빨강:초록≈5:3) | **같은 장면**, 스왑당 진행이 약 절반 |
| 200 | 두 번째 장면(30,228픽셀, `(191,155,93)`) | 아직 페이드 |
| 600 | 세 번째 이후 장면들 | **두 번째 장면**(30,264픽셀, `(190,155,93)`) |
| 400–1000 | 장면 네 번 전환 | 두 번째 장면에 머묾 |

타이머 틱 통계(30초)는 이렇게 갈린다.

| | Win32 | Linux x64 |
|---|---:|---:|
| 도래한 틱 | 6,235 | 5,389 |
| 주입된 틱 | 5,729 | 1,754 |
| 버린 틱 | 506 | 3,571 |
| safe point trap / 주입 | 5,719 / **5,667** | 3,859 / **0** |

Win32는 거의 모든 틱을 AOT safe point에서 주입한다. Linux는 safe point가 발동해도
한 번도 주입하지 못하고, 렌더 단계에서는 주입 수가 거의 늘지 않는다(20초 1,755,
30초 1,754).

**가설(미확정):** 틱이 부족해 게스트 게임 시계가 느리게 가고, 그래서 장면 진행이
늦다.

## 확인된 원인 두 가지

1. **CS 조회.** x64 `InjectPendingInterrupts`는 게스트 논리 CS를 `eip`로 선택자
   표에서 찾는데, safe point에서 `eip`는 cache 주소라 조회가 실패하고 틱이
   보류·폐기된다(Task 710에서 발견).
2. **IRET 프레임의 복귀 주소.** 조회를 고쳐도 프레임에 cache 주소가 들어간다.
   Task 704의 x64 IRETD HLE는 복귀 대상이 실행 가능한 32비트 게스트 descriptor
   범위 안이어야 복원하므로, cache 주소를 거절하고 ISR이 `IRET`에서 죽는다.
   게이트에서 주입한 틱은 복귀 주소가 게스트 주소라 이 문제가 없었다.

## 설계

x64에서는 인터럽트 프레임이 **항상 게스트 주소**를 담는다. `eip`가 cache
주소이면 그 cache 바이트가 실행하는 게스트 명령 — safe point에서는 그것이 지키는
backward edge — 으로 바꾸고, 선택자 조회와 프레임 양쪽에 쓴다. 그곳으로 돌아가면
safe point를 다시 지나며 요청이 이미 지워졌음을 보고 분기로 떨어진다(flags는
`pushfd`/`popfd`로 보존). i386에서는 게스트의 `IRET`이 native로 실행되어 cache
주소로 직접 돌아가므로 그대로 둔다.

**그러나 켜면 실행이 불안정해진다.** 주입을 켠 실행은 IRET을 넘어서지만

* 27–28초에 cache 안의 **일시적 `int3`**(breakpoint, `SI_KERNEL`)로 죽는다. 보고
  시점에 그 바이트는 이미 원래 명령으로 돌아가 있고, 게스트는 ISR에서 불린 Watcom
  `itoa`류 루프(`CS:` override 테이블 읽기)를 실행 중이다.
* 다른 실행은 20초 전에 SIGSEGV로 죽는다.

그래서 이 수정은 **opt-in**(`REPIU_LINUX_X64_SAFE_POINT_INJECTION=1`)으로 둔다.
기본 동작은 이전과 같다.

## 진단 추가

처리되지 않은 Linux 폴트 줄에 **폴트 종류와 `si_code`**를 남긴다. SIGTRAP은
breakpoint(처리기가 RIP를 `int3`로 되감음)와 single-step(RIP는 다음 명령) 두
가지이고, 종류 없이는 `rip`가 `int3`가 아닌 바이트를 가리킬 때 어느 쪽인지 말할 수
없었다.

## 검증

* 두 host core probe 전체
* 기본값(off) 30초 Linux 실행이 이전과 같이 완주하는지
* on 실행이 IRET 크래시(6초)를 넘기는지

---

## English

### Purpose

After Task 713 all 96 Glide gates match across the hosts, so the next question is
**what actually reaches the screen**. The existing `REPIU_GLIDE_PIXEL_DIAG` (per-swap
non-black count and mean color of the back buffer) compares the two hosts.

### Observation

The same scenes are drawn in the same order, but **Linux progresses more slowly**:
both hosts show the same orange fade-in over swaps 3–60, Linux advancing about half
as far per swap; Win32 reaches the second scene (30,228 pixels, mean
`(191,155,93)`) at swap 200 and Linux at swap 600; Win32 then changes scene four
more times by swap 1000 while Linux stays on the second scene.

Over 30 seconds, ticks come due at a similar rate on both hosts (6,235 vs 5,389),
but Win32 injects 5,729 of them — 5,667 at AOT safe points — while Linux injects
1,754, **none** at its 3,859 safe-point traps, and drops 3,571. During rendering the
Linux injection count barely moves (1,755 at 20 s, 1,754 at 30 s).

**Hypothesis (unconfirmed):** tick starvation slows the guest's clock and therefore
its scene progression.

### Confirmed causes

1. **The CS lookup.** x64 `InjectPendingInterrupts` looks up the guest's logical CS
   by `eip`, which at a safe point is a cache address, so the lookup fails and the
   tick is deferred and dropped (found in Task 710).
2. **The IRET frame's return address.** Even with the lookup fixed, the frame
   carries the cache address. Task 704's x64 IRETD HLE restores only a return target
   inside an executable 32-bit guest descriptor, so it refuses the cache address and
   the ISR dies on its `IRET`. Ticks injected at gates never hit this because their
   return address is a guest address.

### Design

On x64 the interrupt frame **always holds a guest address**. When `eip` is a cache
address it is replaced by the guest instruction that byte executes — at a safe
point, the backward edge it guards — for both the selector lookup and the frame.
Returning there re-runs the safe point, finds the request cleared, and falls into
the branch with the flags preserved by `pushfd`/`popfd`. On i386 the guest's `IRET`
runs natively and returns to the cache address directly, so that host is unchanged.

**Turning it on makes the run unstable.** Runs with injection get past the IRET but
die at 27–28 seconds on a **transient `int3`** in the cache (a breakpoint,
`SI_KERNEL`) whose byte has already been restored by the time it is reported, while
the guest is in a Watcom `itoa`-style loop called from the ISR; another run died
before 20 seconds with a SIGSEGV. The fix is therefore **opt-in**
(`REPIU_LINUX_X64_SAFE_POINT_INJECTION=1`); the default behavior is unchanged.

### Added diagnostic

Unhandled Linux fault lines now carry **the fault kind and `si_code`**. A SIGTRAP is
either a breakpoint, whose RIP the handler rewinds onto the `int3`, or a single
step, whose RIP is the next instruction; without the kind, a report whose `rip`
points at a non-`int3` byte could not say which.

### Verification

* Every core-probe group on both hosts.
* A default (off) 30-second Linux run still completing as before.
* An on run getting past the IRET crash (6 s).
