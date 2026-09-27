# Task 750 작업 로그 — swap 대기 중에도 타이머 tick을 전달한다

설계: [20260928-750](../design/20260928-750-timer-ticks-during-the-swap-wait.md)
작업 지시: [20260928-750](../work-orders/20260928-750-timer-ticks-during-the-swap-wait.md)

## 요약

60 Hz 대기(페이싱·진짜 vsync)에서 게스트가 MP3 frame-sync 토글을 0–16 ms 늦게, 프레임당 한 번 몰아서 보던
것을 고쳤습니다. `grBufferSwap` 게이트가 present를 호스트 스레드에 게시하고 기다리는 동안, 밀린 tick을
"게이트에 도달한 `call` 직전에 온 인터럽트"로 주입합니다: ISR의 `iret`가 `call`을 다시 실행해 게이트로
돌아오고, present가 끝났을 때만 게이트가 반환합니다. 곡 구간의 관측 지연이 **0–16 ms → 0–4 ms**, 관측
간격이 **16/33 ms 쌍봉 → 25 ms 중심(21–31 ms)** 이 되어 vsync off의 자유 실행과 같아졌습니다. x11 페이싱과
wayland 모두 60 fps 유지, 25 ms 초과 프레임 0, 폴트 0입니다.

## 과정

1. 비동기 present(`REPIU_GLIDE_ASYNC_PRESENT=1`)로 먼저 시험: 관측 지연 0–16 ms 그대로. 게스트가 swap에서
   돌아와도 다음 동기 호스트 명령에서 같은 시간 막힙니다. wayland에서는 25 ms 초과 프레임 87개로 오히려
   나빠졌습니다.
2. 게이트 재진입 구현 첫 판(EIP를 게이트 주소에 둔 채 주입): 주입 0회. 진단 결과 게이트 주소
   (0x095D04A8)의 selector는 LINEXE 세그먼트 0x0080인데 그 디스크립터가 실행 가능으로 표시돼 있지 않아
   x64의 프레임 CS 조회가 거절했습니다.
3. 반환 목표를 호출부로 변경. pumpitea의 호출부는 `53 E8 D0 BB 0B 00`(`push ebx; call 0x01107264`)이고
   대상은 import thunk `E9 3F 92 4C 08`(`jmp 0x095D04A8`)입니다. `call rel32` + (`jmp rel32` | `jmp [m32]`)를
   검증한 뒤, 반환 주소를 pop한 ESP에 프레임을 쌓습니다. resolver는 ESP = 진입 + 4 − 12를 받아들입니다.
4. 주입 504 swap에 11회뿐. tick 무장 코드가 호스트 폴 루프(= present를 실행하는 호스트 스레드) 안에 있어
   게스트가 기다리는 동안 무장이 멈추고 있었습니다. 무장 블록을 callable로 빼 게스트 대기 루프도 부르게
   하자 504 swap에 1,871회(12초 attract의 tick 2,147개 중).
5. 재진입은 같은 호출이므로 게이트 진입·호출 카운트를 되돌려 게임이 부른 횟수 그대로 둡니다.

## 검증

| 검증 | 결과 |
|---|---|
| x11 페이싱 플레이 62초, 수정 전(749) → 후 | 관측 지연 0–15 ms 균등 → 0–4 ms(98%); 관측 간격 16:344 / 33:468 → 25:485 중심 21–31; 주입 11,366 / swap 3,396 |
| wayland(진짜 vsync) 플레이 62초 | 관측 지연 0–4 ms(98%), 관측 간격 25:526 중심, 주입 11,354 / swap 3,336, 60 fps, 25 ms 초과 프레임 0 |
| x11 vsync off 플레이 62초 | 관측 지연 0–5 ms, fps 103–197, 폴트 0 |
| 곡 중 토글 간격(세 설정) | 24–27 ms 98% 이상(749 유지), multi 0, pcm-empty 0, starvation 1–4 |
| tick 전달(플레이 62초) | due 14,016 / injected 13,966 / dropped 50, deferred 120,136 → 6,103 |
| 스위치 off 대조(`REPIU_GLIDE_SWAP_WAIT_TICKS=0`) attract 12초 | swaps/injections 0/0, 이전 동작 |
| pumpit2a 25초(페이싱·자유), pumpit1 20초(페이싱) | 폴트 0, 주입 2,708 / 1,843 / 2,295 |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug 빌드 + core probe + pumpitea 15초 | 아래 English 절과 같음 |
| Win32에서 스위치 강제 켬(제한 전 빌드) | 첫 주입 뒤 정지(swaps/injections 1/1, 프레임 0, 폴트 0) → Linux x64 전용으로 제한 |

로그: `build/task750-*.err.log`.

**사용자 확인(2026-09-28):** 같은 WSLg 머신에서 플레이한 결과 "상태가 아주 좋아졌다". Task 749의 시계와
이 작업의 대기 중 tick 전달을 함께 적용한 빌드입니다.

## 남은 것

* Win32(i386)에는 제공하지 않습니다. 켜 보니 첫 주입 뒤 게스트가 호출부로 돌아오지 못하고 멈췄습니다
  (i386의 direct dispatch 연속 또는 native `iret` 뒤 재진입 경로 미조사). Win32의 real vsync도 같은 게이트
  대기를 하므로 같은 양자화가 있을 것입니다.
* Win32 pumpitea 15초에서 tick 380개 폐기(max-backlog 64)는 vsync off에서도 같아 이번 변경과 무관하지만,
  747의 "Win32에는 backlog가 없다"는 기록과 다릅니다. 미조사.
* 호출부가 `call rel32`가 아닌 게임(레지스터·메모리 간접 호출)은 주입 없이 기다립니다(이전 동작).
  `REPIU_GLIDE_SWAP_WAIT_LOG=1`이 그 경우를 찍습니다.
* wayland 시작 시 tick 폭주(Task 748)는 이번 실행들에서 나오지 않았고 미조사입니다.

---

# English

# Task 750 work log — timer ticks during the swap wait

Design: [20260928-750](../design/20260928-750-timer-ticks-during-the-swap-wait.md)
Work order: [20260928-750](../work-orders/20260928-750-timer-ticks-during-the-swap-wait.md)

## Summary

Under a 60 Hz wait (pacing or real vsync) the guest saw an MP3 frame-sync toggle 0–16 ms late, in one
burst per frame; that is fixed. While the `grBufferSwap` gate waits for the present it posted to the
host thread, an owed tick is injected as "an interrupt that arrived just before the `call` that reached
the gate": the ISR's `iret` runs the call again and comes back to the gate, which returns only when the
present is done. Inside the song the observation delay went **0–16 ms → 0–4 ms** and the observation
interval **from a 16/33 ms bimodal to a 25 ms centre (21–31 ms)**, the same as free-running with vsync
off. x11 pacing and wayland both hold 60 fps with no frame over 25 ms and no faults.

## Steps

1. Asynchronous present (`REPIU_GLIDE_ASYNC_PRESENT=1`) first: the observation delay stayed 0–16 ms. The
   guest returns from the swap and blocks as long at the next synchronous host command; on wayland it
   was worse, 87 frames over 25 ms.
2. The first gate re-entry (injecting with EIP left on the gate): zero injections. The diagnostic showed
   the gate address (0x095D04A8) resolving to the LINEXE segment 0x0080, whose descriptor is not marked
   executable, so the x64 frame CS lookup refused it.
3. The return target became the call site. pumpitea's is `53 E8 D0 BB 0B 00` (`push ebx; call
   0x01107264`) and its target the import thunk `E9 3F 92 4C 08` (`jmp 0x095D04A8`). After verifying
   `call rel32` plus (`jmp rel32` | `jmp [m32]`), the frame is pushed on the ESP with the return address
   popped; the resolvers accept ESP = entry + 4 − 12.
4. Only 11 injections in 504 swaps. The tick arming code was inside the host poll loop, which is the host
   thread running the present, so arming stopped while the guest waited. With the block made a callable
   the guest's wait loop runs too: 1,871 injections in 504 swaps (of 2,147 ticks in a 12 s attract).
5. A re-entry is the same call, so the gate entry and call counts are taken back and stay the game's own.

## Verification

| Check | Result |
|---|---|
| x11 paced 62 s play, before (749) → after | observation delay uniform 0–15 ms → 0–4 ms (98%); observation interval 16:344 / 33:468 → centred on 25:485, 21–31; 11,366 injections / 3,396 swaps |
| wayland (real vsync) 62 s play | observation delay 0–4 ms (98%), interval centred on 25:526, 11,354 injections / 3,336 swaps, 60 fps, no frame over 25 ms |
| x11 vsync-off 62 s play | observation delay 0–5 ms, 103–197 fps, no faults |
| In-song toggle interval (all three) | 24–27 ms in 98% or more (749 holds), multi 0, pcm-empty 0, starvation 1–4 |
| Tick delivery (62 s play) | due 14,016 / injected 13,966 / dropped 50, deferred 120,136 → 6,103 |
| Switch-off control (`REPIU_GLIDE_SWAP_WAIT_TICKS=0`), 12 s attract | swaps/injections 0/0, the previous behaviour |
| pumpit2a 25 s (paced, free), pumpit1 20 s (paced) | no faults, 2,708 / 1,843 / 2,295 injections |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug build + core probe + pumpitea 15 s | build succeeded (0 errors), `core_probe_all=true`, 293 frames, swaps/injections 0/0, no faults; ticks due/injected/dropped 1,786/1,404/380, the same as with vsync off (1,880/1,496/384) |
| Win32 with the switch forced on (the build before the restriction) | stalled after the first injection (swaps/injections 1/1, no frame, no fault) → restricted to Linux x64 |

Logs: `build/task750-*.err.log`.

**User confirmation (2026-09-28):** playing on the same WSLg machine, "much better". The build carries
both Task 749's clock and this task's ticks during the wait.

## What remains

* Not offered on Win32 (i386). Forced on, the guest never returned to the call site after the first
  injection and the run stalled (the i386 direct dispatch continuation, or the re-entry after a native
  `iret`, is not investigated). Win32's real vsync waits in the same gate, so the same quantisation is
  expected there.
* Win32 pumpitea dropped 380 ticks in 15 s (max-backlog 64), the same with vsync off and so unrelated to
  this change, but at odds with 747's note that Win32 has no backlog. Not investigated.
* A game whose call site is not `call rel32` (a register or memory indirect call) waits without
  injection, as before; `REPIU_GLIDE_SWAP_WAIT_LOG=1` names that case.
* The tick storm at a wayland start (Task 748) did not appear in these runs and is not investigated.
