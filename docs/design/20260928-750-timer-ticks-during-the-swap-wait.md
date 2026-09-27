# Task 750: swap 대기 중에도 타이머 tick을 전달한다

## 한국어

### 배경

Task 749의 위치 트레이스가 남긴 것: 60 Hz 대기(Task 745의 페이싱이든 wayland의 진짜 vsync든)에서 게스트는
MP3 frame-sync 토글을 **0–16 ms 균등 지연**으로, 프레임당 한 번 몰아서(5회 읽기) 관측하고, 곡 위치가 26 ms
대신 **16/33 ms 걸음**으로 나아간다. vsync off의 자유 실행에서는 지연 0–7 ms, 걸음 20–30 ms다. 사용자가
vsync를 켰을 때만 노트와 BGA의 멈칫을 느끼는 이유다.

원인은 `grBufferSwap` 게이트다. 게스트 스레드가 게이트 안에서 present(vblank 대기 또는 페이싱 sleep)가
끝나기를 기다리는 동안 게스트 코드는 한 줄도 돌지 않으므로 타이머 ISR도 돌지 않는다. 실제 기계의 Glide는
swap을 걸어 두고 돌아오며, CPU가 기다리는 동안에도 IRQ0은 240 Hz로 들어온다. Task 747은 밀린 tick을 게이트
**뒤**에 몰아서 빼 주어 개수는 맞췄지만 시각은 맞추지 못했다.

비동기 present(Task 440, `REPIU_GLIDE_ASYNC_PRESENT=1`)는 해법이 아니다: 게스트는 swap에서 돌아오지만 바로
다음 동기 호스트 명령에서 같은 시간만큼 막힌다(측정: 관측 지연 0–16 ms 그대로).

### 설계

1. **swap을 게시하고 게이트에서 기다린다.** 동기 경로의 `BufferSwap` 대신 `PostBufferSwap`으로 present를
   호스트 스레드에 넘기고, 게이트 핸들러는 `WaitForPendingSwaps`로 완료를 기다린다(0.5 ms 조각).
2. **기다리는 동안 밀린 tick을 주입한다.** tick이 밀렸으면, 인터럽트가 게이트에 도달한 `call` **직전에**
   온 것처럼 만든다: 반환 주소를 pop한 ESP에 프레임(EFLAGS, CS, EIP=`call`의 주소)을 쌓고 ISR로 간다.
   ISR의 `iret`는 `call`을 다시 실행하고, 게이트가 다시 들어와(`glide_swap_wait_active`) 계속 기다린다.
   present가 끝났을 때만 게이트가 호출자에게 반환한다. 주입은 기존 `InjectPendingInterrupts` 그대로라 IF,
   PIC in-service, `sti` 연쇄 규칙이 모두 적용된다.
3. **반환 목표는 게이트가 아니라 호출부다.** 게이트 주소는 LINEXE 세그먼트(selector 0x0080)에 있는데 그
   디스크립터는 실행 가능으로 표시돼 있지 않아 Linux x64의 프레임 CS 조회가 거절한다. 호출부는 게임
   코드라 조회가 통하고, 재실행도 번역된 코드 안에서 일어난다. 호출부는 `call rel32`이고 대상이 게이트
   자신이거나 import thunk(`jmp rel32` 또는 `jmp dword ptr [m32]`)가 게이트를 가리킬 때만 인정한다.
   인정되지 않으면 주입 없이 기다린다(이전 동작과 같음).
4. **tick 무장을 게스트 스레드에서도.** tick을 무장하는 코드는 호스트 폴 루프 안에 있었는데, 그 루프가
   곧 present를 실행하는 호스트 스레드라 게스트가 기다리는 바로 그 시간에 무장이 멈춘다(첫 구현: 504
   swap에 주입 11회). 그 블록을 callable로 빼 `ThreadContext::timer_tick_arm`에 등록하고, 두 호출자가
   `timer_tick_arm_mutex` 아래에서 부른다. 루프가 끝날 때 등록을 지운다.
5. **direct dispatch resolver**(x64·i386)는 게이트 핸들러가 "ESP = 진입 ESP + 4 − 12, EIP ≠ 게이트"로
   돌아오는 경우를 `glide_gate_interrupt_injected` 표시가 있을 때 받아들인다.
6. 스위치 `REPIU_GLIDE_SWAP_WAIT_TICKS`: Linux x64 전용, 기본 켜짐, `0`으로 끈다. Win32(i386)는 첫 주입 뒤
   게스트가 호출부로 돌아오지 못하고 멈춰(검증에서 확인) 제공하지 않는다. 비동기 present가 켜져 있으면
   이 경로는 쓰지 않는다. 최종 보고에 `Glide swap wait ticks
   swaps/injections`. 진단 `REPIU_GLIDE_SWAP_WAIT_LOG=1`.

### 검증 전략

위치 트레이스로 x11 페이싱·wayland 플레이의 관측 지연과 관측 간격을 재고(목표: 자유 실행 수준), 스위치
off 대조, 자유 실행, pumpit2a·pumpit1, tick 폐기 수, MP3 census, core probe(Linux·Win32), Win32 smoke.

## English

### Background

What Task 749's position trace left: under a 60 Hz wait (Task 745's pacing or wayland's real vsync) the
guest observes an MP3 frame-sync toggle with a **uniform 0–16 ms delay**, in one burst of five reads per
frame, and its song position advances in **16/33 ms steps** instead of 26 ms. Free-running with vsync
off, the delay is 0–7 ms and the steps 20–30 ms. That is why the user feels the arrows and BGA hitch only
with vsync on.

The cause is the `grBufferSwap` gate. While the guest thread waits inside it for the present (a vblank
wait or a pacing sleep), no guest code runs, so the timer ISR does not run either. The real machine's
Glide queues the swap and returns, and IRQ0 keeps arriving at 240 Hz while the CPU waits. Task 747 drained
the owed ticks **after** the gate, which fixed their count but not their time.

Asynchronous present (Task 440, `REPIU_GLIDE_ASYNC_PRESENT=1`) is not the answer: the guest returns from
the swap and blocks for the same time at the next synchronous host command (measured: the observation
delay stays 0–16 ms).

### Design

1. **Post the swap and wait at the gate.** `PostBufferSwap` hands the present to the host thread in place
   of the synchronous `BufferSwap`, and the gate handler waits with `WaitForPendingSwaps` (0.5 ms slices).
2. **Inject owed ticks while waiting.** An owed tick is delivered as if the interrupt had arrived **just
   before** the `call` that reached the gate: the return address is popped, the frame (EFLAGS, CS,
   EIP = the call's address) is pushed and execution goes to the ISR. Its `iret` runs the call again, the
   gate is entered again (`glide_swap_wait_active`) and keeps waiting; only a finished present makes the
   gate return to its caller. The injection is the existing `InjectPendingInterrupts`, so IF, the PIC
   in-service bit and the `sti` chain rule all apply.
3. **The return target is the call site, not the gate.** The gate lives in the LINEXE segment (selector
   0x0080), whose descriptor is not marked executable, so Linux x64's frame CS lookup refuses it. The call
   site is game code, which the lookup accepts, and the re-execution stays inside translated code. A call
   site is accepted only as `call rel32` whose target is the gate itself or an import thunk (`jmp rel32`
   or `jmp dword ptr [m32]`) leading to it; otherwise the wait injects nothing, as before.
4. **Tick arming from the guest thread too.** The code that arms ticks was inside the host poll loop,
   which is the host thread that runs the present, so arming stopped exactly while the guest waited (the
   first implementation: 11 injections in 504 swaps). The block is now a callable registered as
   `ThreadContext::timer_tick_arm`, run by both callers under `timer_tick_arm_mutex` and cleared when the
   loop returns.
5. **The direct dispatch resolvers** (x64, i386) accept a gate handler returning with "ESP = entry ESP +
   4 − 12, EIP ≠ gate" when `glide_gate_interrupt_injected` is set.
6. The switch `REPIU_GLIDE_SWAP_WAIT_TICKS`: Linux x64 only, on by default, `0` turns it off. On Win32
   (i386) the guest never returns to the call site after the first injection and the run stalls (found
   in verification), so it is not offered there. Not used while asynchronous present is on. The final report gains
   `Glide swap wait ticks swaps/injections`; `REPIU_GLIDE_SWAP_WAIT_LOG=1` is the diagnostic.

### Verification strategy

The position trace for x11 paced and wayland plays (observation delay and interval, aiming at the
free-running figures), the switch-off control, free-running, pumpit2a and pumpit1, dropped ticks, the MP3
census, the core probes (Linux, Win32) and a Win32 smoke run.
