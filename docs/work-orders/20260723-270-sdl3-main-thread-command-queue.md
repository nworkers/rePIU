# SDL3 메인 스레드 렌더 명령 큐 작업 지시

## 한국어

### 목표

SDL3 창, 이벤트, OpenGL context와 GL 호출을 실행기 메인 스레드로 일원화하고 게스트 Glide 호출의 동기 의미를 보존합니다.

### 작업 범위

1. `GlideOpenGlBackend`에 소유 스레드 바인딩과 동기 명령 전달을 추가합니다.
2. 모든 공개 렌더/상태 API가 비소유 스레드 호출을 메인 스레드로 전달하게 합니다.
3. `PollThreadUntilExit`가 매 반복에서 대기 중인 렌더 명령을 처리하게 합니다.
4. 게스트 스레드의 backend 종료를 제거하고, 작업 스레드 정지 뒤 메인 스레드에서 종료합니다.
5. 아키텍처 문서와 작업 로그를 갱신하고 빌드 및 실제 실행으로 검증합니다.

### 완료 조건

- Win32 x86 Debug 빌드 성공
- SDL main-thread assertion 미발생
- 정상 및 timeout 종료에서 GL 자원 파괴가 메인 스레드에서 수행됨

## English

### Objective

Centralize SDL3 window, event, OpenGL-context, and GL operations on the executor main thread while preserving synchronous guest Glide semantics.

### Scope

1. Add owner-thread binding and synchronous command dispatch to `GlideOpenGlBackend`.
2. Dispatch every public rendering/state API invoked off-owner to the main thread.
3. Make `PollThreadUntilExit` service pending rendering commands on every iteration.
4. Remove backend shutdown from the guest worker and close it on the main thread after the worker stops.
5. Update architecture and work-log documentation, then verify with a build and real execution.

### Completion criteria

- Win32 x86 Debug build succeeds
- No SDL main-thread assertion
- GL resources are destroyed on the main thread after both normal and timeout termination
