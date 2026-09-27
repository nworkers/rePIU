# Task 745: 드라이버가 vsync를 거부할 때 엔진이 swap 간격을 맞춘다

## 한국어

### 배경

Linux에서 `cfg/repiu.ini`의 `swap_interval = 1`(또는 `REPIU_GLIDE_SWAP_INTERVAL=1`)을 줘도 vsync가
꺼진 것처럼 돈다는 보고다. 로그가 답을 준다.

```
Glide swap interval override requested/value/applied/effective: true/1/false/0
```

설정은 엔진까지 왔지만 `SDL_GL_SetSwapInterval(1)`이 **실패**했다. WSLg의 GL은 llvmpipe(소프트웨어
렌더러)이고 XWayland의 GLX는 `GLX_EXT_swap_control`을 광고하지만 실제 vblank 동기화를 제공하지 않아
호출이 거부된다. Windows에서는 드라이버가 받아 주므로 같은 설정이 동작한다. 이 상태에서 게임은 프레임
제한 없이 100~170 fps로 돌고, 사용자가 원하는 "vsync 켜짐"(주사율에 맞춘 프레임)이 되지 않는다.

### 설계

1. **거부를 기록한다.** `GlideSwapIntervalPolicySnapshot`에 실패 시 `SDL_GetError()` 문자열, 디스플레이
   주사율, 페이싱 상태를 더하고 최종 보고에 찍는다.
2. **엔진 페이싱 fallback.** override가 요청됐고(`interval ≥ 1`; adaptive `-1`은 1로), 적용에 실패했거나
   effective가 요청과 다르면 백엔드가 직접 간격을 맞춘다. 주기 = `interval / 주사율`(주사율은
   `SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window))->refresh_rate`, 0이면 60 Hz).
   `grBufferSwap`의 `SDL_GL_SwapWindow` 뒤에 다음 마감까지 잔다(`sleep_until` 뒤 마지막 ~300 µs는
   spin). 마감은 누적식으로 잡되 한 주기 이상 뒤처지면 지금 기준으로 다시 맞춘다(따라잡기 폭주 방지).
   페이싱한 swap 수와 잔 시간을 센다.
3. 드라이버가 받아 준 경우(Windows 등)는 지금처럼 드라이버에 맡기고 페이싱하지 않는다. override가
   없으면 아무것도 바뀌지 않는다(측정 절차의 `REPIU_GLIDE_SWAP_INTERVAL=0`도 그대로).
4. 주기 계산은 순수 함수 `ResolveGlideSwapPacingPeriodMicroseconds(interval, refresh_hz)`로 두고 core
   probe로 검사한다(60 Hz·1 → 16,667 µs, 0 Hz → 60 Hz 대체, 2 → 두 배, -1 → 1로).

### 검증 전략

WSLg에서 `REPIU_GLIDE_SWAP_INTERVAL=1`로 pumpitea attract를 돌려 `[repiu-frame-rate]`가 주사율(60)
근처에 잡히고 보고에 페이싱이 찍히는지, override 없이는 이전과 같은 fps인지, Win32 빌드·core probe가
통과하는지 본다.

## English

### Background

On Linux, `swap_interval = 1` in `cfg/repiu.ini` (or `REPIU_GLIDE_SWAP_INTERVAL=1`) still runs as if
vsync were off. The log says why: `Glide swap interval override requested/value/applied/effective:
true/1/false/0`. The setting reached the engine, but `SDL_GL_SetSwapInterval(1)` **failed**: WSLg's GL
is llvmpipe (a software renderer), and XWayland's GLX advertises `GLX_EXT_swap_control` without
providing real vblank synchronization, so the call is refused. On Windows the driver honours it, so the
same setting works there. In this state the game runs unthrottled at 100–170 fps rather than at the
refresh rate the user asked for.

### Design

1. **Record the refusal.** `GlideSwapIntervalPolicySnapshot` gains the `SDL_GetError()` text on
   failure, the display refresh rate and the pacing state; the final report prints them.
2. **Engine pacing fallback.** When an override was requested (`interval ≥ 1`; adaptive `-1` counts as
   1) and it either failed to apply or the effective interval differs, the backend paces the swaps
   itself: period = `interval / refresh rate` (from
   `SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window))->refresh_rate`, 60 Hz when 0). After
   `SDL_GL_SwapWindow` in `grBufferSwap` it sleeps until the next deadline (`sleep_until`, with the
   last ~300 µs spun). Deadlines accumulate but resync to now when more than a period behind (no
   catch-up bursts). Paced swaps and time slept are counted.
3. When the driver honoured the request (Windows, say) nothing changes; with no override nothing
   changes either (the measurement procedure's `REPIU_GLIDE_SWAP_INTERVAL=0` stays as it is).
4. The period is a pure function `ResolveGlideSwapPacingPeriodMicroseconds(interval, refresh_hz)`
   checked by the core probe (60 Hz and 1 → 16,667 µs; 0 Hz → 60 Hz; 2 → double; -1 → 1).

### Verification strategy

pumpitea attract on WSLg with `REPIU_GLIDE_SWAP_INTERVAL=1`: `[repiu-frame-rate]` near the refresh
rate (60) and pacing in the report; without the override the fps as before; Win32 build and core
probes pass.

---

## 결과 (구현 후) / Result (after implementation)

### 한국어

설계대로 넣었습니다. WSLg에서 `REPIU_GLIDE_SWAP_INTERVAL=1`이면 `[repiu-glide-swap] driver refused
swap interval 1 (That operation is not supported); pacing swaps every 16672 us (display 59.98 Hz)`가
찍히고 `[repiu-frame-rate]`가 59.9~60.1 fps로 잡힙니다(override 없이는 58~98 fps). 최종 보고의
`Glide swap pacing active/refresh-hz/period-us/paced-swaps/slept-ms/refusal` 줄이 거부 사유와 페이싱
통계를 냅니다. 설계와 달라진 점은 없습니다.

### English

Implemented as designed. On WSLg with `REPIU_GLIDE_SWAP_INTERVAL=1` the run prints
`[repiu-glide-swap] driver refused swap interval 1 (That operation is not supported); pacing swaps
every 16672 us (display 59.98 Hz)` and `[repiu-frame-rate]` holds at 59.9–60.1 fps (58–98 fps without
the override). The final report's `Glide swap pacing active/refresh-hz/period-us/paced-swaps/
slept-ms/refusal` line gives the refusal and the pacing statistics. No departures from the design.
