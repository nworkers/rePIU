# Task 710 작업 로그 — Linux x64 long-mode timer safe point

설계: [20260918-710](../design/20260918-710-linux-x64-timer-safe-points.md)
(구현 중 정정 3건이 그 문서 끝에 있습니다) ·
작업 지시: [20260918-710](../work-orders/20260918-710-linux-x64-timer-safe-points.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260918-709](20260918-709-final-execution-report.md)

## 요약

**timer safe point는 이제 Linux x64에서도 Win32와 같은 수로 심깁니다. 그러나
이 작업의 판정 기준 — PIU.BIN 읽기 루프가 끝나는 것 — 은 충족되지 않았고, 그
기준을 세운 가설 자체가 틀렸습니다.**

## 수행 결과

### 반영한 것

1. long-mode emission 경로가 backward edge에서 timer safe point를 심습니다.
   i386과 같은 규칙(backward direct jump, 또는 Jcc opcode가 있는 backward
   conditional branch)이라 두 host가 같은 게스트 edge에 safe point를 둡니다.
2. long mode에서는 compare를 `83 3C 25 <disp32> 00`(SIB 절대 주소)으로 씁니다.
   i386의 `83 3D <disp32> 00`은 long mode에서 RIP 상대라 엉뚱한 곳을 읽습니다.
   i386 바이트는 그대로입니다.
3. 방출 검증기가 entry마다 decode한 명령 수를 emitter의 값과 비교하므로 safe
   point 7개를 정확히 셉니다. branch slot이 거절해서 copy나 INT3 boundary로
   떨어지는 경우에는 safe point를 되돌립니다 — boundary의 fixup이 entry 시작에
   기록되는데 그 자리를 safe point가 차지하기 때문입니다.
4. 요청 플래그를 64비트 host에서만 4 GiB 아래 RW 페이지에 둡니다(Task 708의
   `ReserveLowAddressMemory`, 후보 `0x1E000000`부터, 최후수단 없음). i386은
   placement 멤버를 그대로 씁니다. 모든 읽기·쓰기는
   `AotTimerSafePointRequestWord`를 거칩니다.

설계는 플래그를 code cache 안에 두자고 했지만 **그렇게 할 수 없었습니다.** code
cache는 배치 뒤 execute-read이고 플래그는 다른 스레드가 아무 때나 씁니다. 설계가
대안으로 제시했던 별도 페이지를 택했습니다.

### 되돌린 것

x64 `InjectPendingInterrupts`는 게스트 논리 CS를 `eip`로 찾는데, safe point에서
`eip`는 cache 주소라 조회가 실패합니다. cache 주소를 게스트 주소로 되돌려 조회하게
고쳤더니 주입이 시작됐지만, **주입된 ISR 경로가 30초 안에 크래시했습니다.**

```text
[repiu-fault] unhandled signal=0x5 rip=0x20127b00 eip=0x103f1f5
              bytes=fb 45 8b 37 45 8d 7f 04 ...     (fb = STI)
exit=133
```

완주하던 실행을 크래시로 바꾸므로 **되돌렸습니다.** 조회 수정 자체는 옳다고
보지만 그 뒤의 ISR 경로가 먼저 고쳐져야 합니다.

같은 이유로, safe point 주입 뒤 x64 AOT 재진입 블록(Task 702/703 패턴)은 코드에
남아 있지만 **이번 실행에서 한 번도 실행되지 않았습니다.**

## 검증

### probe

* Linux x64 core probe **29/29**, Win32 x86 core probe **27/27**
* `long_mode_emission`에 `timer_safe_points` 항목 추가: 같은 계획에서 long mode와
  i386의 site 수가 같은지, long mode 바이트가 `83 3C 25`인지, i386 바이트가
  `83 3D`로 그대로인지, 끄면 하나도 심지 않는지, long-mode 이미지가 decode
  검증을 통과하는지
* **음성 확인**: 수정 전 emitter로 되돌리면
  `planted=false … i386_sites=1,long_sites=0`으로 실패합니다. 실행에서 본 결함을
  그대로 재현합니다.

이 계획의 i386 이미지는 safe point를 꺼도 `direct control-flow target is outside
the cache`로 invalid입니다. Task 630이 long-mode 경로용으로 쓴 계획이라서이고 이
작업과 무관합니다. 그래서 i386 쪽은 이미지 유효성이 아니라 심긴 site와 쓴 바이트를
비교합니다.

### 실제 `pumpit2a`

| | 수정 전 | 수정 후 | Win32 x86 |
|---|---:|---:|---:|
| safe point sites | 0 | **1,067** | 1,067 |
| safe point trap / injected (30초) | 0 / 0 | 781 / **0** | 5,845 / 5,800 |
| DOS read count (30초) | 1,110,359 | **1,110,356** | 122 |
| 실행 종료 | 예산 완주 | 예산 완주, 폴트 0 | 예산 완주 |

Win32는 site 1,067, 주입 5,800, 읽기 122로 **변화 없습니다.**

## 틀린 가설

설계는 "루프는 시간으로 끝나고 Linux에서는 시간이 게스트에게 도달하지 않는다"고
했습니다. 루프 구간에서 두 host의 타이머 상태를 나란히 놓으면 **똑같습니다.**

| 루프 구간 | Win32 (1.5초) | Linux (10초) |
|---|---|---|
| tick due / injected / dropped | 27 / 0 / 27 | 171 / 0 / 171 |
| safe point trap / injected | 25 / 0 | 171 / 0 |
| INT 8 chain HLE count | 0 | 0 |
| DOS read count | **37** | **754,759** |

틱은 보류가 아니라 **버려집니다.** 이 시점에는 게스트가 INT 8 처리기를 아직
설치하지 않아 주입할 벡터가 없습니다. Win32가 37회 만에 빠져나오는 이유는 타이머와
**무관합니다.**

돌이켜 보면 Task 709 요약에서 `timer_safe_points true/0`과 읽기 폭증이 나란히
보였을 때 **상관을 인과로 읽었습니다.** 루프 구간만 잘라서 두 host의 타이머 상태를
먼저 비교했으면 설계 전에 반증됐을 것입니다.

## 남은 것

1. **Win32가 PIU.BIN 루프를 무엇으로 빠져나오는지.** 두 host 모두 쓰레기
   offset으로 seek하고 0바이트 읽기를 반복합니다(Win32 `0x0458CC60`, Linux
   `0x010F0FDF`). 다른 것은 Win32만 37회 뒤에 루프를 떠난다는 것입니다. 타이머는
   배제됐습니다. 다음 작업은 루프의 탈출 조건을 게스트 코드에서 읽는 것입니다.
2. **safe point 주입 뒤 ISR 경로의 SIGTRAP.** CS 조회를 고치면 드러나는 크래시로,
   INT 8 처리기 epilogue의 `STI`(게스트 `0x0103F1F5`)에서 처리되지 않은
   SIGTRAP이 납니다.

---

## English

Design: [20260918-710](../design/20260918-710-linux-x64-timer-safe-points.md)
(three corrections made during implementation are at its end) ·
Work order: [20260918-710](../work-orders/20260918-710-linux-x64-timer-safe-points.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260918-709](20260918-709-final-execution-report.md)

### Summary

**Timer safe points are now planted on Linux x64 in the same number as on
Win32. This task's criterion — the PIU.BIN read loop ending — was not met, and
the hypothesis behind that criterion was wrong.**

### What landed

1. The long-mode emission path plants a timer safe point on a backward edge,
   under the same rule as i386 (a backward direct jump, or a backward
   conditional branch with a Jcc opcode), so both hosts put safe points on the
   same guest edges.
2. Under long mode the compare is `83 3C 25 <disp32> 00`, the SIB absolute
   form; the i386 `83 3D <disp32> 00` is RIP-relative in long mode. The i386
   bytes are unchanged.
3. The seven safe-point instructions are counted exactly, because the emitter's
   verifier compares each entry's decoded instruction count with its own. If the
   branch slot refuses and the entry falls to a copy or an INT3 boundary, the
   safe point is rolled back — the boundary's fixup is recorded at the entry's
   start, which the safe point would otherwise occupy.
4. On a 64-bit host only, the request flag moves to one RW page below 4 GiB
   (Task 708's `ReserveLowAddressMemory`, candidates from `0x1E000000`, no last
   resort); i386 keeps the placement member. Every read and write goes through
   `AotTimerSafePointRequestWord`.

The design proposed putting the flag inside the code cache, and **that could not
be done**: the cache is execute-read once placed, and the flag is written by
another thread at any moment. The separate page the design named as the
alternative was used instead.

### What was reverted

x64 `InjectPendingInterrupts` finds the guest's logical CS by looking up `eip`,
which at a safe point is a cache address, so the lookup fails. Mapping the cache
address back to its guest address made injection start — and the injected ISR
path then **crashed within 30 seconds** with an unhandled SIGTRAP at the `STI`
(`fb`) in the INT 8 handler's epilogue, guest `0x0103F1F5`, `exit=133`. It
turned a completing run into a crash, so it was **reverted**. The lookup fix is
believed correct, but the ISR path behind it has to be fixed first. For the same
reason the x64 AOT re-entry block after a safe-point injection (the Task 702/703
pattern) is in the code but **never executed** in these runs.

### Verification

Linux x64 core probe **29 of 29** and Win32 x86 **27 of 27**. The new
`long_mode_emission` `timer_safe_points` item checks that long mode and i386
plant the same number of sites on one plan, that the long-mode bytes are
`83 3C 25` and the i386 bytes still `83 3D`, that nothing is planted when
disabled, and that the long-mode image passes decode verification. **Negative
check**: with the pre-fix emitter the item fails with
`planted=false … i386_sites=1,long_sites=0`, reproducing the live defect
exactly. The i386 image of this plan is invalid even with safe points off
(`direct control-flow target is outside the cache`) because Task 630 wrote the
plan for the long-mode path; the i386 side therefore compares planted sites and
written bytes rather than image validity.

On a real 30-second `pumpit2a` run, safe-point sites went from 0 to **1,067**
(Win32: 1,067) and trap/injected is 781/**0** (Win32: 5,845/5,800). The DOS
read count is **1,110,356**, unchanged from 1,110,359 (Win32: 122). Both runs
reached their budget with no fault. Win32 is unchanged at 1,067 sites, 5,800
injections and 122 reads.

### The hypothesis that failed

The design said the loop ends on time and time never reaches the guest on Linux.
Side by side over the loop only, the two hosts' timer state is **identical**:
ticks due are all **dropped** (27 on Win32 at 1.5 s, 171 on Linux at 10 s), none
injected, safe points all deferred, INT 8 chain count zero. The guest has not yet
installed its INT 8 handler on either host, so there is no vector to inject into.
Win32 leaves the loop after 37 reads for a reason **unrelated to the timer**.

In hindsight, when Task 709's summary showed `timer_safe_points true/0` next to
the read explosion, a correlation was read as a cause. Comparing the two hosts'
timer state over the loop alone would have falsified it before the design was
written.

### What is left

1. **What makes Win32 leave the PIU.BIN loop.** Both hosts seek to a garbage
   offset (Win32 `0x0458CC60`, Linux `0x010F0FDF`) and repeat zero-length reads;
   only Win32 leaves after 37. The timer is ruled out. The next task reads the
   loop's exit condition from the guest code.
2. **The SIGTRAP in the ISR path after a safe-point injection**, exposed once the
   CS lookup is fixed, at the `STI` in the INT 8 handler's epilogue (guest
   `0x0103F1F5`).
