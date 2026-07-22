# SDL3 메인 스레드 렌더 명령 큐 작업 로그

## 한국어

### 결과

- `GlideOpenGlBackend`에 호스트 스레드 바인딩, 단일 슬롯 동기 명령 전달, 메인 스레드 명령 pump를 추가했습니다.
- 창 생성, 이벤트 처리, buffer clear/swap, draw, texture/LFB, raster 상태와 종료를 포함한 모든 공개 backend 호출을 소유 스레드에서 실행하게 했습니다.
- 실행기 메인 스레드의 `PollThreadUntilExit`가 매 반복에서 대기 중인 Glide 명령을 처리합니다.
- 게스트 작업 스레드의 직접 `Close`를 제거하고 정상 종료와 timeout 강제 종료 모두 작업 스레드가 멈춘 뒤 메인 스레드에서 SDL/OpenGL 자원을 파괴합니다.
- SDL3 전환 뒤 불필요해진 `winmm` 링크를 제거하고 아키텍처 문서의 WGL/waveOut 설명을 현재 구현에 맞췄습니다.

### 검증

- `cmake --build build\win32_x86_debug --config Debug -- /m:1`: 성공
- `repiu_supervisor_win32.exe pumpit1 45000`: 45.5초 실행 후 정상 종료 코드 0
- 약 10초 시점에 SDL3 OpenGL 창 열기 성공 메시지 확인
- 이전 실행의 `SDL_IsMainThread_REAL()` assertion은 45초 실행 두 차례와 25초 실행 한 차례에서 재발하지 않음
- `ctest --test-dir build\win32_x86_debug -C Debug --output-on-failure`: 등록된 테스트 없음
- guest heartbeat와 dispatch count가 전체 실행 동안 계속 증가함
- `REPIU_GLIDE_PIXEL_DIAG=1` 검증에서는 45초 안에 첫 `grBufferSwap`이 관찰되지 않아 실제 swap 픽셀 검증은 미완료입니다. 이는 assertion이나 교착 없이 게스트가 계속 진행한 것과 별개의 후속 런타임 도달성 항목입니다.

### 회고

원본 게스트 실행 스레드를 유지하면서 SDL의 스레드 제약을 만족하려면 HLE gate 자체를 메인 스레드로 옮기기보다 backend 호출만 동기 전달하는 편이 원본 제어 흐름과 ABI를 더 잘 보존합니다. 단일 게스트 실행이라는 현재 불변 조건 덕분에 작은 명령 슬롯으로 충분하며, 향후 다중 guest thread를 도입하면 요청별 completion 객체가 필요합니다.

## English

### Result

- Added host-thread binding, single-slot synchronous dispatch, and main-thread command pumping to `GlideOpenGlBackend`.
- All public backend operations now execute on the owner thread, including window creation, events, clear/swap, draw, textures/LFB, raster state, and shutdown.
- The executor main thread services pending Glide commands on every `PollThreadUntilExit` iteration.
- Removed direct `Close` from the guest worker. Both normal and forced-timeout paths now destroy SDL/OpenGL resources on the main thread after the worker stops.
- Removed the obsolete `winmm` link and updated WGL/waveOut architecture text to describe the current SDL3 implementation.

### Verification

- `cmake --build build\win32_x86_debug --config Debug -- /m:1`: passed
- `repiu_supervisor_win32.exe pumpit1 45000`: exited normally with code 0 after 45.5 seconds
- Confirmed successful SDL3 OpenGL window creation at about 10 seconds
- `ctest --test-dir build\win32_x86_debug -C Debug --output-on-failure`: no tests registered
- The former `SDL_IsMainThread_REAL()` assertion did not recur across two 45-second runs and one 25-second run
- Guest heartbeat and dispatch counts continued increasing throughout execution
- With `REPIU_GLIDE_PIXEL_DIAG=1`, the guest did not reach its first `grBufferSwap` within 45 seconds, so actual swap-pixel verification remains incomplete. This is a separate runtime-reachability follow-up; the guest continued without an assertion or deadlock.

### Retrospective

To preserve the original guest execution thread while satisfying SDL's thread rules, synchronously dispatching only backend calls preserves the original control flow and ABI better than moving the HLE gate itself. The current single-guest-thread invariant makes a small command slot sufficient; a future multi-guest-thread design would require per-request completion objects.
