# SDL quit·창 닫기 안전 종료 작업 지시

## 한국어

### 작업 범위

1. `GlideOpenGlBackend`에 host 종료 요청 상태와 조회 API를 추가합니다.
2. `SDL_EVENT_QUIT`과 `SDL_EVENT_WINDOW_CLOSE_REQUESTED`가 같은 종료 요청을 설정하게
   변경합니다.
3. `PollThreadUntilExit`가 요청을 감지해 전용 결과를 반환하게 합니다.
4. execution trampoline이 timeout teardown을 재사용해 guest worker를 중단하고
   SDL/GL 및 translation worker를 순서대로 정리하게 합니다.
5. execution attempt에 quit 상태와 로그를 추가합니다.
6. 아키텍처와 작업 로그를 갱신하고 실제 창 닫기 종료를 검증합니다.

### 완료 조건

- `Ctrl+C`에 대응하는 `SDL_EVENT_QUIT`과 창 닫기가 모두 실행 종료를 요청합니다.
- event handler에서 직접 process exit 또는 SDL resource 파괴를 하지 않습니다.
- 종료 결과가 timeout/exception과 구분됩니다.
- Win32 x86 Debug 빌드와 실제 창 close 검증이 성공합니다.

## English

### Scope

1. Add host exit-request state and an accessor to `GlideOpenGlBackend`.
2. Make `SDL_EVENT_QUIT` and `SDL_EVENT_WINDOW_CLOSE_REQUESTED` set the same
   request.
3. Make `PollThreadUntilExit` return a dedicated result for that request.
4. Reuse timeout teardown in the execution trampoline to stop the guest worker,
   then clean up SDL/GL and the translation worker in order.
5. Add quit state and logging to the execution attempt.
6. Update architecture/work-log documentation and verify a real window close.

### Completion criteria

- Both `SDL_EVENT_QUIT` (including `Ctrl+C`) and window close request execution
  shutdown.
- The event handler does not directly exit the process or destroy SDL resources.
- Quit is distinguishable from timeout and exception.
- Win32 x86 Debug build and real window-close verification pass.
