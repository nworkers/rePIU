# Task 745 작업 로그 — 드라이버가 vsync를 거부할 때 엔진이 swap 간격을 맞춘다

설계: [20260927-745](../design/20260927-745-swap-pacing-when-vsync-refused.md)
작업 지시: [20260927-745](../work-orders/20260927-745-swap-pacing-when-vsync-refused.md)

## 요약

Linux에서 `swap_interval = 1`을 줘도 vsync가 꺼진 것처럼 돈다는 보고의 원인은 설정 전달이 아니라
드라이버의 거부였습니다. 사용자 로그의 `Glide swap interval override requested/value/applied/effective:
true/1/false/0`이 그것을 말했고, WSLg의 GL(llvmpipe, XWayland GLX)은 `GLX_EXT_swap_control`을
광고하면서도 `SDL_GL_SetSwapInterval(1)`을 "That operation is not supported"로 거부합니다. 이제 거부되면
엔진이 `grBufferSwap` 뒤에 디스플레이 주사율 기준의 다음 마감까지 자서 프레임을 맞춥니다. WSLg에서
fps가 58~98에서 59.9~60.1로 잡힙니다. 드라이버가 받아 주는 환경(Windows)과 override가 없는 실행은
바뀌지 않습니다.

## 과정

1. 사용자의 `cfg/repiu.ini`(`[Video] swap_interval = 1`)와 로그를 봤습니다. 설정은 런처가 환경 변수
   `REPIU_GLIDE_SWAP_INTERVAL`로 엔진에 전달했고, 백엔드가 `SDL_GL_SetSwapInterval(1)`을 불렀으나
   `applied=false, effective=0`이었습니다.
2. x11 드라이버에서 같은 실패를 재현했고 `glxinfo`가 llvmpipe와 `GLX_EXT_swap_control`을 보였습니다.
   wayland 드라이버는 창을 만들지 못해 비교 대상이 아니었습니다.
3. `GlideSwapIntervalPolicySnapshot`에 `failure`(SDL 오류), `refresh_rate_hz`, `pacing_active`,
   `pacing_period_us`, `paced_swaps`, `paced_sleep_us`를 더했습니다. 주기는 순수 함수
   `ResolveGlideSwapPacingPeriodMicroseconds(interval, refresh_hz)`(adaptive -1은 1, 주사율 0은 60 Hz)이고
   probe가 검사합니다.
4. 백엔드는 override가 요청됐고 적용에 실패했거나 effective가 다르면 페이싱을 켭니다. `PaceSwapAfterPresent`가
   누적 마감을 쓰되 한 주기 이상 뒤처지면 지금 기준으로 다시 맞추고, 마지막 300 µs는 spin합니다.
5. 최종 보고에 `Glide swap pacing active/refresh-hz/period-us/paced-swaps/slept-ms/refusal`을 더하고,
   README의 vsync 문단에 인자 실행 시의 환경 변수와 페이싱 fallback을 적었습니다.

## 검증

| 검증 | 결과 |
|---|---|
| WSLg, `REPIU_GLIDE_SWAP_INTERVAL=1`, attract 14초 | `[repiu-glide-swap] driver refused swap interval 1 (That operation is not supported); pacing swaps every 16672 us (display 59.98 Hz)`, fps 59.9~60.1(로딩 정지 구간 제외), paced-swaps 565, slept 7,582 ms, 폴트 0 |
| 같은 실행, override 없음 | 58~98 fps, 보고 `requested=false`, 페이싱 없음 |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug 빌드 + core probe | 아래 English 절과 같음 |

로그: `build/task745-vsync-on.err.log`, `build/task745-vsync-off.err.log`, `build/task745-vsync-x11.err.log`.

## 남은 것

* 페이싱은 프레임 간격만 맞추고 찢어짐(tearing)은 막지 못합니다. WSLg의 합성기가 vblank를 제공하지
  않는 한 방법이 없습니다.
* wayland 드라이버로는 창이 열리지 않는 것을 보았지만 조사하지 않았습니다.

---

# English

# Task 745 work log — engine swap pacing when the driver refuses vsync

Design: [20260927-745](../design/20260927-745-swap-pacing-when-vsync-refused.md)
Work order: [20260927-745](../work-orders/20260927-745-swap-pacing-when-vsync-refused.md)

## Summary

`swap_interval = 1` on Linux still ran as if vsync were off, and the cause was not the setting's
delivery but the driver's refusal: the user's log said `Glide swap interval override
requested/value/applied/effective: true/1/false/0`, and WSLg's GL (llvmpipe under XWayland GLX)
advertises `GLX_EXT_swap_control` yet refuses `SDL_GL_SetSwapInterval(1)` with "That operation is
not supported". When refused, the engine now sleeps after `grBufferSwap` until the next deadline at
the display's refresh rate. On WSLg the frame rate goes from 58–98 to 59.9–60.1 fps. Hosts whose
driver honours the request (Windows) and runs without an override are unchanged.

## Steps

1. The user's `cfg/repiu.ini` (`[Video] swap_interval = 1`) and log: the launcher passed the setting
   as `REPIU_GLIDE_SWAP_INTERVAL`, the backend called `SDL_GL_SetSwapInterval(1)`, and got
   `applied=false, effective=0`.
2. Reproduced under the x11 driver; `glxinfo` showed llvmpipe and `GLX_EXT_swap_control`. The wayland
   driver opened no window, so it was no comparison.
3. `GlideSwapIntervalPolicySnapshot` gained `failure` (SDL's error), `refresh_rate_hz`,
   `pacing_active`, `pacing_period_us`, `paced_swaps` and `paced_sleep_us`. The period is the pure
   function `ResolveGlideSwapPacingPeriodMicroseconds(interval, refresh_hz)` (adaptive -1 counts as
   1, a zero rate as 60 Hz), checked by the probe.
4. The backend enables pacing when an override was requested and failed to apply or the effective
   interval differs. `PaceSwapAfterPresent` accumulates deadlines, resyncs when more than a period
   behind, and spins the last 300 µs.
5. The final report gained `Glide swap pacing active/refresh-hz/period-us/paced-swaps/slept-ms/
   refusal`, and the README's vsync paragraph names the environment variable for argument runs and
   the pacing fallback.

## Verification

| Check | Result |
|---|---|
| WSLg, `REPIU_GLIDE_SWAP_INTERVAL=1`, 14 s attract | `[repiu-glide-swap] driver refused swap interval 1 (That operation is not supported); pacing swaps every 16672 us (display 59.98 Hz)`, 59.9–60.1 fps (outside the loading stall), 565 paced swaps, 7,582 ms slept, no faults |
| Same run without the override | 58–98 fps, `requested=false`, no pacing |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug build + core probe | build succeeded, `core_probe_all=true` |

Logs: `build/task745-vsync-on.err.log`, `build/task745-vsync-off.err.log`,
`build/task745-vsync-x11.err.log`.

## What remains

* Pacing fixes the frame interval, not tearing; nothing can while WSLg's compositor provides no
  vblank.
* The wayland driver opened no window; not investigated.
