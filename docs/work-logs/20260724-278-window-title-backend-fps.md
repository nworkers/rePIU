# 창 제목 실행 backend·FPS 표시 작업 로그

## 한국어

### 구현 결과

- 실행 컨텍스트가 확정한 플랫폼 공용 `ExecutionBackend`를 `GlideOpenGlBackend`에 명시적으로 전달했습니다.
- 기존 제목 끝에 `[<backend>] - FPS : <한 자리 소수>`를 추가했습니다.
- 초기 제목은 측정 표본이 없으므로 `FPS : 0.0`을 표시합니다.
- 성공한 guest `grBufferSwap`만 집계하고 `std::chrono::steady_clock` 기준 약 1초마다 FPS를 계산합니다.
- 긴 로딩 시간이 첫 표본에 섞이지 않도록 첫 성공 swap에서 측정 구간을 시작하며, 프레임 사이 간격 수를 실제 경과시간으로 나눕니다.
- `SDL_SetWindowTitle`은 기존 guest-worker 동기 명령 큐 안에서 실행되므로 SDL window를 소유한 실행기 메인 스레드에서만 호출됩니다.
- close/reopen 시 측정 상태를 초기화합니다.

### 검증

- VS2022 Win32 x86 Debug 전체 빌드 성공
- 보정 후 `repiu_loader_win32`와 `repiu_supervisor_win32` 증분 빌드 성공
- `aot-dynamic` 실제 `pumpit1` 145초 supervisor 실행:
  - 초기 제목:
    `rePIU v0.0.87 - Build Jul 24 2026 - Glide 2 OpenGL [aot-dynamic] - FPS : 0.0`
  - 실제 swap 이후 관측 제목:
    `...[aot-dynamic] - FPS : 3.4`, `3.9`, `4.0`, `3.7`, `3.8` 등
  - Glide milestone `gate/open/tex/draw/swap = 1/1/1/1/1`
  - final fatal count 0
  - final legacy fallback count 0
  - supervisor deadline에 의한 `child_exit=124 terminated=true`
- 제목 갱신 실패와 `SDL_IsMainThread` assertion 없음
- `git diff --check` 통과

실측 FPS 값이 60이 아닌 것은 현재 guest가 이 검증 구간에서 약 1–4 FPS로 프레임을 제출했기 때문이며, 표시 형식과 측정 갱신은 요구한 한 자리 소수 형식을 따릅니다.

## English

### Implementation result

- Explicitly propagated the platform-neutral `ExecutionBackend` selected by the execution context into `GlideOpenGlBackend`.
- Appended `[<backend>] - FPS : <one decimal>` to the existing title.
- The initial title displays `FPS : 0.0` before a measurement sample exists.
- Counted only successful guest `grBufferSwap` calls and calculated FPS over roughly one-second `std::chrono::steady_clock` periods.
- Started the first period at the first successful swap so loading time does not contaminate the sample, dividing frame intervals by actual elapsed time.
- `SDL_SetWindowTitle` executes inside the existing synchronous guest-worker command path and therefore only on the executor main thread that owns the SDL window.
- Reset measurement state on close/reopen.

### Verification

- Full VS2022 Win32 x86 Debug build passed
- Post-correction incremental `repiu_loader_win32` and `repiu_supervisor_win32` build passed
- Real 145-second supervised `pumpit1` run using `aot-dynamic`:
  - Initial title:
    `rePIU v0.0.87 - Build Jul 24 2026 - Glide 2 OpenGL [aot-dynamic] - FPS : 0.0`
  - Titles observed after real swaps included:
    `...[aot-dynamic] - FPS : 3.4`, `3.9`, `4.0`, `3.7`, and `3.8`
  - Glide milestone `gate/open/tex/draw/swap = 1/1/1/1/1`
  - Final fatal count 0
  - Final legacy fallback count 0
  - `child_exit=124 terminated=true` at the supervisor deadline
- No title-update failure or `SDL_IsMainThread` assertion
- `git diff --check` passed

The measured value was not 60 because the guest submitted roughly 1–4 FPS during this verification period. The display and update behavior follow the requested one-decimal format.
