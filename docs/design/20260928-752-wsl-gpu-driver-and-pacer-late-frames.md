# Task 752: WSL에서 GPU 드라이버를 고르고, 늦은 프레임을 한 주기 더 붙잡지 않는다

## 한국어

### 배경

사용자 보고: "Linux x64 버전으로 pumpit8 실행 시 fps도 상당히 떨어지고 오디오 노이즈, 게임 플레이 시 노트
튐이 모두 관찰된다." 전수 조사(Task 751)에서도 pumpit8과 pumpitp2만 마지막 20초의 fps 중앙값이 36~41이었다.

프레임을 직전 페이서 sleep·게스트 구간·present로 갈라 재니(`REPIU_GLIDE_LONG_FRAME_LOG=2`, 모든 프레임)
원인이 둘 나왔다.

1. **페이서가 늦은 프레임을 한 주기 더 붙잡는다.** Task 745의 재동기 조건 `deadline + period < now`에서
   `deadline`은 직전 프레임의 마감이라 `deadline + period`는 이 프레임 자신의 마감이다. 즉 프레임이
   조금이라도 늦으면 "한 주기 넘게 밀렸다"로 판단해 `now`부터 한 주기를 통째로 잤다. 프레임 작업이 17 ms인
   화면은 33 ms 프레임이 되어 절반 속도로 돈다. 최종 보고의 `late-swaps`가 늘 0이고 `resyncs`만 쌓인 것이
   그 흔적이었다.
2. **WSL에서 소프트웨어 렌더러가 쓰인다.** WSL은 GPU를 Mesa의 D3D12 드라이버로만 내주는데 Mesa는 스스로
   그것을 고르지 않고 llvmpipe를 쓴다. 이 머신(RTX 4090)에서 pumpit8의 모드·곡 선택 화면은 present가
   llvmpipe 11~12 ms, D3D12 5 ms다. 게스트 구간(3~4.5 ms)을 더하면 llvmpipe는 주기를 넘나든다.

게임 플레이 구간 자체는 제 재현(곡 720, 곡 중 입력)에서 두 렌더러 모두 60 fps에 가깝고 MP3 공급·tick
전달도 정상이었다. 보고된 노이즈와 노트 튐은 재현하지 못했으며, 프레임이 주기를 넘길 때 1번 결함이 만드는
33 ms 프레임과, 선택 화면·무거운 장면에서 2번이 만드는 주기 초과가 그 조건이라는 것까지가 확인된 범위다.

### 설계

1. **페이서**. 이 프레임의 마감 `D = 직전 마감 + 주기`를 구하고, `D + 주기 < now`(한 주기 넘게 늦음)일
   때만 재동기한다. 재동기는 마감을 `now`로 두며 자지 않는다. 그 밖에는 마감을 `D`로 두고, `now < D`이면
   거기까지 자고 아니면 늦은 것으로 세고 지나간다. 마감은 계속 누적되므로 늦은 프레임 뒤의 프레임들이
   시간을 되찾는다.
2. **WSL의 D3D12 드라이버 선택**. 창을 열기 전에, `/dev/dxg`(WSL의 paravirtual GPU)와 `d3d12_dri.so`가 있고
   사용자가 `GALLIUM_DRIVER`·`MESA_LOADER_DRIVER_OVERRIDE`·`LIBGL_ALWAYS_SOFTWARE`를 주지 않았으면
   `GALLIUM_DRIVER=d3d12`를 설정한다. `REPIU_WSL_D3D12=0`은 Mesa의 선택을 그대로 둔다.
3. **무엇이 그리는지 말한다**. 컨텍스트가 만들어지면 `GL_RENDERER`를 stderr(`[repiu-glide] GL renderer: …`)와
   최종 보고(`Glide GL renderer/wsl-d3d12-chosen`)에 남긴다.

### 검증 전략

pumpit8 플레이(입력 스크립트 `pumpit8_play.txt`)를 x11 페이싱·wayland, D3D12·llvmpipe로 돌려 초당 fps와
25 ms 초과 프레임 수, present 중앙값을 비교한다. 16개 롬셋 재조사로 회귀를 본다. core probe(Linux·Win32).

## English

### Background

The user's report: "Running pumpit8 on Linux x64 the fps drops considerably, and audio noise and arrow
jumps during play are all observed." The survey (Task 751) had also shown pumpit8 and pumpitp2 alone
with a median of 36–41 fps over their last 20 s.

Splitting every frame into the previous pace's sleep, the guest's share and the present
(`REPIU_GLIDE_LONG_FRAME_LOG=2`) gave two causes.

1. **The pacer holds a late frame one period longer.** In Task 745's resynchronisation test
   `deadline + period < now`, `deadline` is the previous frame's, so `deadline + period` is this frame's
   own. A frame late by anything was taken for "more than a period behind" and slept a whole period from
   `now`. A screen whose frames take 17 ms became 33 ms frames at half rate. The final report's
   `late-swaps` always at 0 with `resyncs` accumulating was the trace of it.
2. **A software renderer draws on WSL.** WSL offers the GPU only through Mesa's D3D12 driver, and Mesa
   does not pick it by itself and uses llvmpipe. On this machine (RTX 4090) pumpit8's mode and song
   selection screens present in 11–12 ms on llvmpipe and 5 ms on D3D12. With the guest's share (3–4.5 ms)
   llvmpipe crosses the period back and forth.

Play itself, in my reproduction (song 720, input through the song), is close to 60 fps on both renderers
with a normal MP3 feed and tick delivery. The reported noise and arrow jumps were not reproduced; what is
established is that a frame over its period met defect 1's 33 ms frame, and that selection screens and
heavy scenes cross the period under cause 2.

### Design

1. **The pacer**. This frame's deadline is `D = previous deadline + period`; only `D + period < now` (more
   than a period late) resynchronises, which sets the deadline to `now` and does not sleep. Otherwise the
   deadline is `D`: `now < D` sleeps until it, and anything else counts as late and passes. Deadlines keep
   accumulating, so the frames after a late one make the time up.
2. **Choosing the D3D12 driver on WSL**. Before the window opens, when `/dev/dxg` (WSL's paravirtual GPU)
   and `d3d12_dri.so` exist and the user set none of `GALLIUM_DRIVER`, `MESA_LOADER_DRIVER_OVERRIDE` and
   `LIBGL_ALWAYS_SOFTWARE`, `GALLIUM_DRIVER=d3d12` is set. `REPIU_WSL_D3D12=0` leaves Mesa's choice alone.
3. **Say what draws**. Once the context exists `GL_RENDERER` goes to stderr (`[repiu-glide] GL renderer:
   …`) and to the final report (`Glide GL renderer/wsl-d3d12-chosen`).

### Verification strategy

pumpit8 plays (the input script `pumpit8_play.txt`) under x11 pacing and wayland, on D3D12 and llvmpipe,
comparing fps per second, frames over 25 ms and the median present; the 16 ROM sets surveyed again for
regressions; the core probes (Linux, Win32).
