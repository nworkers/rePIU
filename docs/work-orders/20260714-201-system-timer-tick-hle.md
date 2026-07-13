# System Timer Tick HLE 구현 작업 지시서 (안정화)
# System Timer Tick HLE Implementation Work Order (Stabilization)

## 1. 작업 개요 (Task Overview)
* **목적:** 비동기 멀티스레딩 쓰기로 유발되던 `STATUS_GUARD_PAGE_VIOLATION` 크래시를 방지하기 위해 비동기 타이머 스레드를 완전 제거하고, 호스트 메인 폴링 루프(`PollThreadUntilExit`) 내에서 동기식으로 BDA `0x46C` 번지 시스템 타이머 틱을 주기적으로 갱신하도록 구조를 개선합니다.
* **관련 문서:** `docs/design/20260714-system-timer-tick-hle.md`

* **Goal:** Discard the crash-prone asynchronous timer thread and implement synchronous BIOS Data Area (BDA) `0x46C` system tick updates directly inside the host poller loop (`PollThreadUntilExit`).
* **References:** `docs/design/20260714-system-timer-tick-hle.md`

---

## 2. 세부 구현 대상 (Detailed Tasks)

### 1) ThreadContext 타이머 스레드 변수 롤백 제거
* `src/platform/win32/execution_trampoline.cpp` 내의 `struct ThreadContext` 구조체 끝부분에 임시 추가하였던 `timer_thread`, `timer_thread_shutdown`, `timer_tick_count` 를 제거합니다.

### 2) 타이머 스레드 제어 로직 롤백 및 TimerUpdateWorkerProc 제거
* `execution_trampoline.cpp` 내에 정의했던 `TimerUpdateWorkerProc` 함수와 `RunWin32ExecutionThread` 내의 스레드 런칭/소멸 관련 라이프사이클 구문들을 완전히 롤백하여 복원합니다.

### 3) PollThreadUntilExit 내 동기식 틱 갱신 이식
* `execution_trampoline.cpp` 내의 `PollThreadUntilExit` 함수 메인 루프에 경과 시간(`GetTickCount() - start_tick`)을 55ms 주기로 나누어 틱 카운트를 산출하고, 이를 `WriteDosLowMemory` 로 주입하는 동기식 로직을 구성합니다.

---

## 3. 검증 방법 (Verification Procedure)
* `win32_x86_debug` 빌드를 다시 컴파일하여 링크 오류가 없음을 확인합니다.
* `repiu_supervisor_win32.exe`를 사용하여 에뮬레이터를 무제한 구동하고, 10초 타임아웃 예외 크래시 없이 `grDrawTriangle` 등의 호출이 로그에 지속되는지 모니터링합니다.
