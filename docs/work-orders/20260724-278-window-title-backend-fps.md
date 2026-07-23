# 창 제목 실행 backend·FPS 표시 작업 지시

## 한국어

### 작업 범위

1. 실행 컨텍스트에서 선택된 `ExecutionBackend`를 `GlideOpenGlBackend`에 전달합니다.
2. 기존 제목 끝에 `[<backend>] - FPS : <one-decimal>`을 추가합니다.
3. 성공한 guest buffer swap과 단조 시간을 이용해 약 1초 단위 FPS를 계산합니다.
4. 창 close/reopen 시 FPS 상태를 초기화합니다.
5. 아키텍처와 작업 로그를 갱신합니다.
6. Win32 x86 Debug 빌드 및 실제 창 제목을 검증합니다.

### 완료 조건

- `aot-dynamic` 실행 창 제목 끝이 `[aot-dynamic] - FPS : n.n` 형식입니다.
- 실행 약 1초 뒤 FPS가 실제 swap 빈도에 따라 갱신됩니다.
- SDL 제목 API는 실행기 메인 스레드에서만 호출됩니다.
- 영향 범위 빌드와 실제 실행 검증이 성공합니다.

## English

### Scope

1. Pass the selected `ExecutionBackend` from the execution context to `GlideOpenGlBackend`.
2. Append `[<backend>] - FPS : <one-decimal>` to the existing title.
3. Calculate FPS about once per second from successful guest buffer swaps and monotonic time.
4. Reset FPS state when the window closes or reopens.
5. Update architecture and work-log documentation.
6. Verify the Win32 x86 Debug build and the real window title.

### Completion criteria

- An `aot-dynamic` window title ends in `[aot-dynamic] - FPS : n.n`.
- FPS updates from the measured swap rate after about one second.
- SDL title APIs run only on the executor main thread.
- The affected build and real execution verification pass.
