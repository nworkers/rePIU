# Task 752: WSL GPU 드라이버 선택과 페이서의 늦은 프레임 작업 지시

설계: [20260928-752](../design/20260928-752-wsl-gpu-driver-and-pacer-late-frames.md)

## 한국어

1. pumpit8을 곡까지 몰고 가는 입력 스크립트(`scripts/input_scripts/pumpit8_play.txt`)로 프레임을
   sleep·게스트·present로 갈라 잰다.
2. `PaceSwapAfterPresent`의 재동기 조건을 "한 주기 넘게 늦음"으로 고치고, 늦은 프레임을 자지 않고 보낸다.
3. 창을 열기 전에 WSL의 D3D12 드라이버를 고른다(`SelectWslD3d12Driver`, `REPIU_WSL_D3D12=0`으로 끔).
   `GL_RENDERER`를 stderr와 최종 보고에 남긴다.
4. x11 페이싱·wayland × D3D12·llvmpipe로 pumpit8 플레이를 비교하고, 16개 롬셋 재조사와 core
   probe(Linux·Win32)를 확인한 뒤 README·analysis·작업 로그를 갱신하고 커밋한다.

## English

1. Measure frames split into sleep, guest and present with an input script that drives pumpit8 into a
   song (`scripts/input_scripts/pumpit8_play.txt`).
2. Correct `PaceSwapAfterPresent`'s resynchronisation test to "more than a period late" and let a late
   frame pass without a sleep.
3. Choose WSL's D3D12 driver before the window opens (`SelectWslD3d12Driver`, off with
   `REPIU_WSL_D3D12=0`), and put `GL_RENDERER` on stderr and in the final report.
4. Compare pumpit8 plays under x11 pacing and wayland on D3D12 and llvmpipe, check the 16 ROM sets and the
   core probes (Linux, Win32), update README, the analysis and the work log, and commit.
