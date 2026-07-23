# SDL quit·창 닫기 안전 종료 작업 로그

## 한국어

### 원인

vendored SDL3의 event 초기화가 기본 `SIGINT`/`SIGTERM` handler를 설치하고 신호를
`SDL_EVENT_QUIT`으로 변환하는 것을 확인했습니다. 기존 rePIU event 정책은
`SDL_EVENT_QUIT`과 `SDL_EVENT_WINDOW_CLOSE_REQUESTED`에서 창만 숨겼으므로,
SDL 전환 뒤 `Ctrl+C`도 프로세스를 끝내지 못했습니다.

### 구현

- `GlideOpenGlBackend`에 main-thread 소유 `exit_requested` 상태를 추가했습니다.
- `SDL_EVENT_QUIT`과 `SDL_EVENT_WINDOW_CLOSE_REQUESTED`가 모두 동일한 종료 요청을
  설정하며, 더 이상 창만 숨기지 않습니다.
- `PollThreadUntilExit`가 host 명령/event pump 직후 요청을 감지해 전용 wait result를
  반환합니다.
- execution trampoline이 기존 timeout의 guest context recovery와 worker 정리 절차를
  공유합니다.
- event handler에서는 직접 `exit()`하거나 SDL resource를 파괴하지 않습니다.
- execution attempt에 `quit_requested`를 추가해 timeout과 명확히 구분했습니다.

### 검증

- VS2022 Win32 x86 Debug 전체 빌드 성공
- 실제 `aot-dynamic` SDL 창에 `WM_CLOSE` 전달:
  - `WM_CLOSE_POSTED=true`
  - supervisor `child_exit=0`, `terminated=false`
  - `Win32 SDL exit requested: true`
  - `Win32 minimal execution timed out: false`
  - exception caught false, fatal breakpoint count 0
  - thread exit code 0
  - `minimal execution stopped by SDL exit request`
- 직접 `aot-dbt` loader 1초 timeout 회귀:
  - `timed_out=true`
  - `quit_requested=false`
  - timeout context captured true
  - exception caught false, fatal breakpoint count 0
- vendored SDL source와 rePIU event 분기에서 `SIGINT` → `SDL_EVENT_QUIT` →
  `exit_requested` 연결을 코드 수준으로 확인
- `git diff --check` 통과

일부 검증 시작은 알려진 32-bit 저주소 배치 변동으로 창 생성 전에 종료됐으며,
정상 배치된 실행만 기능 판정에 사용했습니다.

## English

### Cause

The vendored SDL3 event initialization installs default `SIGINT`/`SIGTERM`
handlers and translates those signals into `SDL_EVENT_QUIT`. The previous rePIU
policy only hid the window for both quit and window-close events, so `Ctrl+C`
stopped terminating the process after the SDL migration.

### Implementation

- Added main-thread-owned `exit_requested` state to `GlideOpenGlBackend`.
- Both `SDL_EVENT_QUIT` and `SDL_EVENT_WINDOW_CLOSE_REQUESTED` now set the same
  shutdown request instead of merely hiding the window.
- `PollThreadUntilExit` detects the request immediately after pumping host
  commands/events and returns a dedicated wait result.
- The execution trampoline shares the established timeout context-recovery and
  worker teardown procedure.
- The event handler never calls `exit()` or destroys SDL resources directly.
- Added `quit_requested` to the execution attempt, distinct from timeout.

### Verification

- Full VS2022 Win32 x86 Debug build passed
- Posted `WM_CLOSE` to a real `aot-dynamic` SDL window:
  - `WM_CLOSE_POSTED=true`
  - supervisor `child_exit=0`, `terminated=false`
  - SDL exit requested true, timed out false
  - no caught exception or fatal breakpoint
  - thread exit code 0
- Direct `aot-dbt` one-second timeout regression:
  - timed out true, SDL exit requested false
  - timeout context captured
  - no caught exception or fatal breakpoint
- Confirmed the `SIGINT` → `SDL_EVENT_QUIT` → `exit_requested` connection in
  the vendored SDL source and the shared rePIU event branch
- `git diff --check` passed

Some validation launches ended before window creation because of the known
32-bit low-address placement variability. Only normally placed runs were used
for functional conclusions.
