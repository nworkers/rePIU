# #6 작업 로그: direct 모델에서도 swap 대기 중 타이머 tick 전달

Issue: [#6](https://github.com/reexec/rePIU/issues/6) · 설계: [20261005-i006](../design/20261005-i006-direct-model-swap-wait-ticks.md) ·
작업 지시: [20261005-i006](../work-orders/20261005-i006-direct-model-swap-wait-ticks.md)

## 요약

게임 창이 숨겨져 vsync swap이 오래 막힐 때 Linux i386이 타이머 tick을 대량으로 버리던 것을 고쳤습니다. Task 750의 "swap 대기 중
tick 주입"이 direct 모델에서 멈추던 원인(gate thunk의 프레임과 인터럽트 프레임이 게스트 스택의 같은 자리)을 찾아, 엔진이 만든
13바이트 출구 코드를 거쳐 핸들러에 들어가게 했습니다. direct 모델은 대기가 50 ms를 넘긴 뒤에만 주입하므로 창이 보일 때의 동작과
프레임은 그대로입니다. 12초 최소화에서 버려진 tick: pumpit1 4,456 → 21, pumpitea 4,451 → 54.

## 바꾼 것

* `include/repiu/engine/glide_gate_interrupt_exit.h`, `src/engine/glide_gate_interrupt_exit.cpp`(신규): 출구 코드
  (`push cs; push [eip]; jmp [handler]`)의 인코딩, thunk 프레임 재배치, 실행 가능 페이지에 한 번 배치.
* `ResolveAotDbtGlideGateFrame`: swap gate가 tick을 주입했으면 프레임을 재배치하고 출구 코드로 나감.
* `InjectsTicksDuringSwapWait()`: direct 모델도 true. `GlideSwapWaitTicksEnabled()`: direct 모델에서 출구 코드를 만들 수 없으면 false.
* `ContinueGlideSwapWait`: 대기 시작 시각(`ThreadContext::glide_swap_wait_begin`)과 보류 시간(`GlideSwapWaitTickHold`: direct
  50 ms, cache 0 ms, `REPIU_GLIDE_SWAP_WAIT_TICK_HOLD_MS`).
* probe `glide_gate_interrupt_exit`(core probe, Win32 AOT probe `--glide-gate-interrupt-exit`).
* `ARCHITECTURE.md`, `linux-port-frontier.md`, 설계.

## 경과

1. 스위치만 켜서 재현: `swaps/injections 1/1` 뒤 예외. Task 750 로그가 Win32에서 본 것과 같습니다.
2. 처음 설계(far 점프, CS를 slot에서 push): 출구 코드의 `jmp fword ptr`에서 SIGSEGV. 값을 찍어 보니 핸들러 selector는 논리
   selector `0x24`였고, 주입이 프레임에 쓴 CS는 0이었습니다(thunk 경로의 컨텍스트에는 `SegCs`가 없음).
3. `push cs` + near 점프로 변경: 예외 없이 동작. 25초에 주입 5,126회, dropped 21.
4. 창이 보이는 상태에서 끈 설정과 교대로 재니 프레임이 약 15% 적었습니다(669~719 대 809~817 / 20초). 예외 수는 비슷해 원인을
   찾지 못했습니다. 보류 50 ms를 넣자 주입 0회, 프레임 809~813으로 끈 설정과 같아졌습니다.

## 검증

실기: Ubuntu 26.04, RTX 4090, Linux i386 Release, `SDL_VIDEO_DRIVER=x11`, vsync 기본.

| 검증 | 결과 |
|---|---|
| 빌드 | Linux i386 Release, Linux x64 Debug exit 0, 새 경고 없음 |
| core probe | 두 호스트 모두 `glide_gate_interrupt_exit_all=true`, `core_probe_all=true` |
| pumpit1 20초, 창 보임, 교대 4쌍 | 수정 809~813프레임·주입 0·dropped 21~22 / 끔 804~816프레임 |
| pumpit1 34초, 12초 최소화 | 수정: dropped **21**, max-backlog 13, 주입 5,718 / 끔: dropped **4,456**, max-backlog 64 |
| pumpit8 34초, 12초 최소화 | dropped 60, 주입 5,721, 예외 0 |
| pumpitea 34초, 12초 최소화 | dropped 54, 주입 5,728, 예외 0 (보류 전 빌드의 끔: 4,451) |
| pumpitea 40초 정상 | MP3 decoded 548(정상), dropped 47, 예외 0 |
| Linux x64 Debug pumpit1 20초 | 637·635프레임, 주입 1,538·1,598 (변화 없음; 보류 0 ms) |

* 최소화한 실행의 프레임 수(약 470 / 34초)는 수정 전후가 같습니다. 숨겨진 창의 프레임이 줄어드는 것은 컴포지터의 동작이고 이
  작업이 고친 것은 게임 시간입니다.
* 측정 중 사용자가 같은 모니터에서 작업하고 있어 창이 가려진 실행이 섞였습니다(끈 설정의 96·24프레임 등). 위 표의 "창 보임"
  줄은 dropped가 21~22인 실행들입니다.

## Win32 확인

2026-10-05, Windows 11(4K 150%), Win32 Release(`build/Release`), vsync 기본(swap interval 1 적용 확인), 예산 34초(창 보임 실행은
pumpit8·pumpitea 40초). 최소화는 Linux와 같이 9초 뒤 `ShowWindow(SW_MINIMIZE)` 12초, 그다음 복원.

| 검증 | 결과 |
|---|---|
| 빌드 | exit 0, 바뀐 파일에 새 경고 없음. 단 기존 트리는 `cmake -S . -B build`로 다시 구성해야 새 파일 두 개가 들어감(첫 빌드는 링크 오류) |
| core probe | `glide_gate_interrupt_exit_all=true`, `glide_gate_fixup_index_all=true`. AOT probe `--glide-gate-interrupt-exit`, `--glide-gate-fixup-index` 모두 true |
| core probe 전체 | `core_probe_all=false`: `stack_bridge_contract=false` 하나. **기존 결함**: main 시점 Release(`build/win32_x86_debug/Release`)도 false, Debug는 true. Release에서만 나는 probe 쪽 문제로 보임(거부 호출이 레지스터를 그대로 돌려주는데, 앞 호출의 마커가 EAX에 남아 있을 수 있음) |
| pumpit1 최소화, 수정 | dropped 48·36, 주입 26·26, 예외 0 |
| pumpit1 최소화, 끔 | dropped 35·35 |
| pumpit1 창 보임 | 1,566프레임, 주입 0, dropped 37 |
| pumpit1 창 보임, 보류 0 ms(부하) | **1,566프레임**, 주입 3,804, dropped 38, 예외·실패 0 |
| pumpit8 창 보임 / 최소화 | 1,930프레임 dropped 99 / 주입 30 dropped 96, 예외 0 |
| pumpitea 창 보임 / 최소화 | dropped 60 / 주입 12 dropped 61, 예외 0. MP3 received 14,002·19,002(끔), main 바이너리 23,865 |

* **Windows는 최소화해도 swap이 막히지 않습니다.** 최소화 중 pumpit1은 게임 자신의 30 fps 그대로였고, pumpit8은 약 125 fps로
  오히려 빨라졌습니다. 그래서 Linux i386의 대량 손실(4,200대)은 Windows에서 원래 생기지 않고, 켜고 끈 dropped가 같습니다.
  최소화·복원 순간 1초 안에 프레임이 약 1,000개 몰리는 것은 끈 설정에서도 같습니다(첫 실행의 10,121프레임은 이 몰림이 길었던
  것).
* 이 작업의 Win32 위험은 출구 코드 경로였습니다(Task 750은 Win32에서 첫 주입 뒤 예외를 봤음). 보류 0으로 매 swap 대기마다 주입한
  3,804회가 예외 없이 지나갔고 프레임이 끈 설정과 같았습니다. **i386에서 본 약 15% 프레임 손실은 Win32에서 나타나지 않았습니다.**
* pumpitea는 세 바이너리 모두 약 8초 뒤 프레임이 멈춥니다(기존 동작). 첫 실행의 MP3 received 2는 시작 시점 편차였고, 다시 돌리니
  14,002였습니다.

## 남은 것

* ~~Win32 확인~~ → 위 절. 보류 0이 Win32에서 프레임을 잃지 않으므로, direct 모델의 보류를 Win32에서만 0으로 내리는 선택지가
  생겼습니다(별도 판단).
* **대기 시작부터 주입하면 i386의 프레임이 줄어드는 원인(미확정).** 풀리면 보류를 0으로 내려 Task 750이 x64에 준 효과(프레임 안의
  tick 시각)를 direct 모델도 얻을 수 있습니다. `REPIU_GLIDE_SWAP_WAIT_TICK_HOLD_MS=0`으로 재현합니다.
* 다른 창에 **완전히 가려진** 경우(최소화가 아닌)는 프로그램으로 만들 수 없어 직접 확인하지 못했습니다. 같은 swap 지연이면
  같은 경로를 탑니다.
* 호출부가 `call rel32`가 아닌 게임은 주입 없이 기다립니다(Task 750과 같음).

---

# #6 Work Log: Timer Ticks During the Swap Wait on the Direct Model Too

Issue: [#6](https://github.com/reexec/rePIU/issues/6) · Design: [20261005-i006](../design/20261005-i006-direct-model-swap-wait-ticks.md) ·
Work order: [20261005-i006](../work-orders/20261005-i006-direct-model-swap-wait-ticks.md)

## Summary

Linux i386 no longer drops timer ticks in bulk when the game window is hidden and vsync swaps stay blocked. The reason Task
750's "tick injection during the swap wait" stopped on the direct model was found (the gate thunk's frame and the interrupt
frame are the same slots of the guest stack), and the gate now enters the handler through 13 bytes of exit code the engine
generates. The direct model injects only once the wait has lasted 50 ms, so behaviour and frames with a visible window are
unchanged. Ticks dropped in 12 s minimised: pumpit1 4,456 → 21, pumpitea 4,451 → 54.

## Changes

* `include/repiu/engine/glide_gate_interrupt_exit.h`, `src/engine/glide_gate_interrupt_exit.cpp` (new): encoding of the exit
  code (`push cs; push [eip]; jmp [handler]`), the thunk frame rearrangement, and its one-time placement in an executable page.
* `ResolveAotDbtGlideGateFrame`: when the swap gate injected a tick, it rearranges the frame and leaves through the exit code.
* `InjectsTicksDuringSwapWait()`: true on the direct model too. `GlideSwapWaitTicksEnabled()`: false on the direct model when
  the exit code cannot be made.
* `ContinueGlideSwapWait`: the wait's start (`ThreadContext::glide_swap_wait_begin`) and the hold (`GlideSwapWaitTickHold`:
  50 ms direct, 0 ms cache, `REPIU_GLIDE_SWAP_WAIT_TICK_HOLD_MS`).
* The `glide_gate_interrupt_exit` probe (core probe, and `--glide-gate-interrupt-exit` in the Win32 AOT probe).
* `ARCHITECTURE.md`, `linux-port-frontier.md`, the design.

## How it went

1. Reproduced with only the switch on: an exception after `swaps/injections 1/1`, as the Task 750 log saw on Win32.
2. The first design (a far jump, CS pushed from a slot): SIGSEGV at the exit code's `jmp fword ptr`. Printing the values
   showed the handler's selector to be the logical `0x24`, and the CS the injection wrote into the frame to be 0 (the thunk
   path's context has no `SegCs`).
3. Changed to `push cs` and a near jump: runs without an exception; 5,126 injections in 25 s, dropped 21.
4. With the window visible and alternating with the switch off, it drew about 15% fewer frames (669–719 against 809–817 in
   20 s). The exception counts are alike and the cause was not found. With a 50 ms hold there are no injections and 809–813
   frames, the same as with the switch off.

## Verification

Real hardware: Ubuntu 26.04, RTX 4090, Linux i386 Release, `SDL_VIDEO_DRIVER=x11`, default vsync.

| Check | Result |
|---|---|
| Builds | Linux i386 Release and Linux x64 Debug, exit 0, no new warnings |
| Core probe | `glide_gate_interrupt_exit_all=true` and `core_probe_all=true` on both hosts |
| pumpit1 20 s, window visible, 4 alternating pairs | changed: 809–813 frames, 0 injections, dropped 21–22 / off: 804–816 frames |
| pumpit1 34 s, minimised for 12 s | changed: dropped **21**, max-backlog 13, 5,718 injections / off: dropped **4,456**, max-backlog 64 |
| pumpit8 34 s, minimised for 12 s | dropped 60, 5,721 injections, no exception |
| pumpitea 34 s, minimised for 12 s | dropped 54, 5,728 injections, no exception (off, on the build before the hold: 4,451) |
| pumpitea 40 s, normal | MP3 decoded 548 (nominal), dropped 47, no exception |
| Linux x64 Debug, pumpit1 20 s | 637 and 635 frames, 1,538 and 1,598 injections (unchanged; hold 0 ms) |

* The frame count of a minimised run (about 470 in 34 s) is the same before and after. Fewer frames in a hidden window is
  the compositor's doing; what this task fixes is the game's time.
* The user was working on the same monitor during the measurements, so some runs had a covered window (the 96 and 24
  frames of the switch-off runs, for instance). The "window visible" row uses the runs whose dropped count is 21–22.

## Win32

2026-10-05, Windows 11 (4K at 150%), Win32 Release (`build/Release`), default vsync (swap interval 1 confirmed applied),
34 s budget (40 s for the visible pumpit8 and pumpitea runs). Minimising as on Linux: after 9 s, `ShowWindow(SW_MINIMIZE)`
for 12 s, then restore.

| Check | Result |
|---|---|
| Build | exit 0, no new warnings in the changed files. The existing tree needed `cmake -S . -B build` before it picked up the two new files (the first build failed to link) |
| Core probe | `glide_gate_interrupt_exit_all=true`, `glide_gate_fixup_index_all=true`; AOT probe `--glide-gate-interrupt-exit` and `--glide-gate-fixup-index` both true |
| Whole core probe | `core_probe_all=false` from `stack_bridge_contract=false` alone. **Pre-existing**: the main-era Release (`build/win32_x86_debug/Release`) is false too, Debug is true. It looks like a Release-only probe issue (the refused call returns registers untouched, and the previous call's marker may still be in EAX) |
| pumpit1 minimised, changed | dropped 48 and 36, 26 and 26 injections, no exception |
| pumpit1 minimised, off | dropped 35 and 35 |
| pumpit1 visible | 1,566 frames, no injection, dropped 37 |
| pumpit1 visible, 0 ms hold (stress) | **1,566 frames**, 3,804 injections, dropped 38, no exception or failure |
| pumpit8 visible / minimised | 1,930 frames, dropped 99 / 30 injections, dropped 96, no exception |
| pumpitea visible / minimised | dropped 60 / 12 injections, dropped 61, no exception. MP3 received 14,002 and 19,002 (off); main binary 23,865 |

* **On Windows a minimised window does not block the swap.** Minimised, pumpit1 kept the game's own 30 fps and pumpit8
  sped up to about 125 fps. So Linux i386's bulk loss (in the 4,200s) never happens on Windows, and dropped is the same
  with the switch on or off. The burst of about 1,000 frames in the second of minimising or restoring happens with the
  switch off too (the first run's 10,121 frames was a longer burst).
* The Win32 risk in this task was the exit code path (Task 750 saw an exception after the first injection on Win32).
  With a 0 ms hold, injecting on every swap wait, 3,804 injections ran without an exception and with the same frames as
  the switch off. **The roughly 15% frame loss seen on i386 does not appear on Win32.**
* pumpitea stops drawing after about 8 s with all three binaries (existing behaviour). The first run's MP3 received of 2
  was a variation in when the music starts; run again it was 14,002.

## Left

* ~~Win32~~ → the section above. Since a 0 ms hold costs Win32 no frames, lowering the direct model's hold to 0 on
  Win32 alone is now an option (a separate decision).
* **Why injecting from the start of the wait costs i386 frames (unresolved).** Solved, the hold could drop to 0 and the
  direct model would get what Task 750 gave x64 (ticks on time within a frame). `REPIU_GLIDE_SWAP_WAIT_TICK_HOLD_MS=0`
  reproduces it.
* A window **fully covered** by another (not minimised) could not be produced by program and was not checked directly; with
  the same swap delay it takes the same path.
* Games whose call site is not `call rel32` wait without injection (as in Task 750).
