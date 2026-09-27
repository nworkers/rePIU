# Task 752 작업 로그 — WSL GPU 드라이버 선택과 페이서의 늦은 프레임

설계: [20260928-752](../design/20260928-752-wsl-gpu-driver-and-pacer-late-frames.md)
작업 지시: [20260928-752](../work-orders/20260928-752-wsl-gpu-driver-and-pacer-late-frames.md)

## 요약

pumpit8의 fps 저하는 두 가지가 겹친 것이었습니다. (1) Task 745의 페이서가 **조금이라도 늦은 프레임을 한
주기(16.7 ms) 더 붙잡고** 있었습니다(재동기 조건이 이 프레임 자신의 마감과 비교하고 있었음). (2) WSL에서
**소프트웨어 렌더러(llvmpipe)** 로 그리고 있었습니다: GPU는 Mesa의 D3D12 드라이버로만 제공되는데 Mesa가
스스로 고르지 않습니다. 페이서를 고치고 엔진이 WSL에서 D3D12 드라이버를 고르게 하자, pumpit8의 선택 화면이
36~42 fps → 60 fps, 전수 조사의 마지막 20초 fps 중앙값이 pumpit8 39.6 → 59.9, pumpitp2 38.9 → 59.8이
됐습니다. 보고된 오디오 노이즈와 노트 튐은 제 재현에서 나오지 않았으므로 사용자 확인이 필요합니다.

## 과정

1. 모든 프레임을 sleep·게스트·present로 갈라 5초 단위 중앙값을 냈습니다(`REPIU_GLIDE_LONG_FRAME_LOG=2`).
   pumpit8의 25초 이후: 게스트 4.3 ms, **present 10~12 ms**, 페이서 sleep 1.7~2.9 ms, 프레임 17.1~17.8 ms.
2. `glxinfo`: 기본은 `llvmpipe`, `GALLIUM_DRIVER=d3d12`면 `D3D12 (NVIDIA GeForce RTX 4090)`. `/dev/dxg`와
   `d3d12_dri.so`는 있었습니다. D3D12로 present 5 ms.
3. 곡 선택의 0.5초 게스트 정지는 미리듣기 오디오(100 KB 남짓)를 읽은 직후로, 플레이와 무관했습니다. 곡에
   들어가는 입력 스크립트(`pumpit8_play.txt`)로 다시 재니 플레이 구간은 두 렌더러 모두 60 fps 가까이였고
   MP3 공급(초당 16 KB)·starvation·tick 전달이 정상이었습니다.
4. 플레이 중 25 ms를 넘긴 프레임 51개 가운데 39개가 **sleep 16,672 µs**(정확히 한 주기)로 시작했습니다.
   페이서의 `deadline + period < now`가 이 프레임의 마감과 비교하고 있어, 늦기만 하면 재동기하고 `now`부터
   한 주기를 잤습니다. 조건을 "한 주기 넘게 늦음"으로 고치고 늦은 프레임은 자지 않게 했습니다.
5. 엔진이 창을 열기 전에 WSL의 D3D12 드라이버를 고르고(`SelectWslD3d12Driver`), `GL_RENDERER`를 찍습니다.

## 검증

| 검증 (pumpit8, 입력 스크립트, 120초) | 선택 화면 fps | 플레이 fps | 플레이 중 25 ms 초과 | 플레이 present 중앙값 |
|---|---|---|---|---|
| 수정 전, x11 + llvmpipe | 36~42 | 58~60 | 51 | 4.0~5.5 ms |
| 수정 전, x11 + D3D12(수동) | 55~60 | 59~60 | 43 | 4.0~5.7 ms |
| 수정 후, x11 + llvmpipe(`REPIU_WSL_D3D12=0`) | 55~61 | 59.6~60.3 | 0 | — |
| 수정 후, x11 + D3D12(자동) | 59.8~60.5 | 59.6~60.3 | 6 | — |
| 수정 후, wayland + D3D12(자동, 진짜 vsync) | 61.5~64.4 | 60.4~61.2 | 2 | 14.6 ms(vblank 대기 포함) |

| 그 밖의 검증 | 결과 |
|---|---|
| 페이서 지터(수정 후 x11 D3D12) | late-swaps 25(최대 6.7 ms), resyncs 10; 수정 전에는 late-swaps 항상 0 |
| Linux x64 16개 롬셋 재조사 | 모두 완주, 폴트 0, 16개 모두 `D3D12 (NVIDIA GeForce RTX 4090)`; pumpit8 중앙값 39.6 → 59.9, pumpitp2 38.9 → 59.8, 나머지 불변 |
| MP3·tick(pumpit8 플레이 120초) | pcm-empty 0, multi 0, tick dropped 107~130 / 28,300 |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug 빌드 + core probe + pumpit8 20초 | 아래 English 절과 같음 |

로그: `build/task752-*.err.log`, `build/survey/linux-x64-752/`.

## 남은 것

* 보고된 **오디오 노이즈와 노트 튐은 재현하지 못했습니다**. 제 재현은 곡 720에 0.5초 간격 자동 입력이고
  MP3·tick이 정상이었습니다. 수정본에서 남아 있으면 그 실행의 stderr(`REPIU_GLIDE_LONG_FRAME_LOG=1
  REPIU_PIU10_MP3_CENSUS_MS=20`)가 필요합니다.
* 곡 선택에서 곡을 옮길 때마다 미리듣기 로드로 게스트가 0.5초 멈춥니다(오디오 파일을 읽은 뒤). 원인은
  조사하지 않았습니다.
* pumpit1은 곡 중 30 fps로 돕니다(Win32도 같음, 게임 자체의 속도로 보임).
* D3D12 드라이버의 렌더링이 llvmpipe와 픽셀 단위로 같은지는 비교하지 않았습니다. 화면 샘플의 비검정 픽셀
  수와 밝기 격자는 같은 범위였습니다.

---

# English

# Task 752 work log — choosing the GPU driver on WSL, and the pacer's late frames

Design: [20260928-752](../design/20260928-752-wsl-gpu-driver-and-pacer-late-frames.md)
Work order: [20260928-752](../work-orders/20260928-752-wsl-gpu-driver-and-pacer-late-frames.md)

## Summary

pumpit8's fps loss was two things together. (1) Task 745's pacer **held a frame that was late at all one
period (16.7 ms) longer** (its resynchronisation test compared against this frame's own deadline). (2) On
WSL a **software renderer (llvmpipe)** was drawing: the GPU is offered only through Mesa's D3D12 driver,
which Mesa does not pick by itself. With the pacer corrected and the engine choosing the D3D12 driver on
WSL, pumpit8's selection screens went from 36–42 fps to 60, and the survey's median over the last 20 s
from 39.6 to 59.9 for pumpit8 and 38.9 to 59.8 for pumpitp2. The reported audio noise and arrow jumps did
not appear in my reproduction, so the user's confirmation is needed.

## Steps

1. Every frame split into sleep, guest and present, with medians per 5 s
   (`REPIU_GLIDE_LONG_FRAME_LOG=2`). pumpit8 after 25 s: guest 4.3 ms, **present 10–12 ms**, pacer sleep
   1.7–2.9 ms, frame 17.1–17.8 ms.
2. `glxinfo`: `llvmpipe` by default, `D3D12 (NVIDIA GeForce RTX 4090)` with `GALLIUM_DRIVER=d3d12`;
   `/dev/dxg` and `d3d12_dri.so` were there. Present 5 ms on D3D12.
3. The half-second guest stalls in song selection follow the read of a preview's audio (100 KB or so) and
   have nothing to do with play. Measured again with an input script that enters a song
   (`pumpit8_play.txt`), play was close to 60 fps on both renderers, with a normal MP3 feed (16 KB a
   second), starvation and tick delivery.
4. Of 51 frames over 25 ms during play, 39 began with a **sleep of 16,672 µs**, exactly one period. The
   pacer's `deadline + period < now` compared against this frame's own deadline, so any lateness
   resynchronised and slept a period from `now`. The test is now "more than a period late", and a late
   frame is not held.
5. The engine chooses WSL's D3D12 driver before the window opens (`SelectWslD3d12Driver`) and prints
   `GL_RENDERER`.

## Verification

| Check (pumpit8, input script, 120 s) | selection fps | play fps | over 25 ms in play | median present in play |
|---|---|---|---|---|
| Before, x11 + llvmpipe | 36–42 | 58–60 | 51 | 4.0–5.5 ms |
| Before, x11 + D3D12 (by hand) | 55–60 | 59–60 | 43 | 4.0–5.7 ms |
| After, x11 + llvmpipe (`REPIU_WSL_D3D12=0`) | 55–61 | 59.6–60.3 | 0 | — |
| After, x11 + D3D12 (chosen) | 59.8–60.5 | 59.6–60.3 | 6 | — |
| After, wayland + D3D12 (chosen, real vsync) | 61.5–64.4 | 60.4–61.2 | 2 | 14.6 ms (the vblank wait included) |

| Other checks | Result |
|---|---|
| Pacer jitter (after, x11 D3D12) | late-swaps 25 (6.7 ms at most), resyncs 10; late-swaps was always 0 before |
| The 16 ROM sets on Linux x64 | all ran, no faults, all 16 on `D3D12 (NVIDIA GeForce RTX 4090)`; pumpit8's median 39.6 → 59.9, pumpitp2's 38.9 → 59.8, the rest unchanged |
| MP3 and ticks (pumpit8, 120 s play) | pcm-empty 0, multi 0, ticks dropped 107–130 of 28,300 |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug build + core probe + pumpit8 20 s | build succeeded (0 errors), `core_probe_all=true`, 290 frames, no faults, renderer `NVIDIA GeForce RTX 4090/PCIe/SSE2`, driver choice not made (not Linux), swap interval 1 applied |

Logs: `build/task752-*.err.log`, `build/survey/linux-x64-752/`.

## What remains

* The reported **audio noise and arrow jumps were not reproduced**. My reproduction is song 720 with an
  automatic press every 0.5 s, and its MP3 and ticks were normal. If they remain with the fix, that run's
  stderr (`REPIU_GLIDE_LONG_FRAME_LOG=1 REPIU_PIU10_MP3_CENSUS_MS=20`) is what is needed.
* Moving between songs in the selection stalls the guest for half a second per preview (after its audio
  file is read). Not investigated.
* pumpit1 runs its songs at 30 fps (on Win32 too; it looks like the game's own rate).
* Whether the D3D12 driver renders pixel for pixel what llvmpipe does was not compared; the screen
  samples' non-black counts and brightness grids were in the same range.
