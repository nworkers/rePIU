# Task 730 작업 지시: x64 종료 회수 판정에 전체 RIP 사용

설계: [20260922-730](../design/20260922-730-x64-shutdown-recovery-full-rip.md)

## 한국어

1. `include/repiu/engine/shutdown_recovery_policy.h`와
   `src/engine/execution/shutdown_recovery_policy.cpp` 신설: `ShutdownRecoveryDecision`,
   `ShutdownRecoveryPosition`, `DecideShutdownRecovery`, `HostAddressFitsGuestSpace`,
   `ShutdownRecoveryDecisionName`. 할당·잠금 없음(시그널 핸들러에서 호출).
2. `execution_trampoline.cpp`의 `RecoverGuestThreadForShutdownCommon`이 이 함수로 판정.
   x64 Linux는 `ReadHostInstructionPointer(host_context)`로 전체 RIP를 넘깁니다.
   `GuestShutdownRecoveryRequest`에 `last_host_ip`, `aliased_count`, `last_decision` 추가.
3. `[repiu-shutdown]` 줄에 `host_ip=`, `aliased=`, `decision=` 추가.
4. `repiu_aot_probe --shutdown-recovery-policy` probe와 `CMakeLists.txt`.
5. 문서: `ARCHITECTURE.md`, `docs/analysis/linux-port-frontier.md`, `docs/guides/linux-shutdown-check.md`
   (새 필드 읽는 법), 작업 로그.
6. 검증: Win32 전체 빌드·core probe·전용 probe, WSL 빌드·core probe, Task 729 설정 10회 관찰.

하지 않을 것: 다른 x64 fault 경로 수정, 기본값 변경.

---

## English

1. New `include/repiu/engine/shutdown_recovery_policy.h` and
   `src/engine/execution/shutdown_recovery_policy.cpp`: `ShutdownRecoveryDecision`,
   `ShutdownRecoveryPosition`, `DecideShutdownRecovery`, `HostAddressFitsGuestSpace` and
   `ShutdownRecoveryDecisionName`, with no allocation or locking (called from a signal handler).
2. `RecoverGuestThreadForShutdownCommon` in `execution_trampoline.cpp` decides through it; x64
   Linux passes the full RIP from `ReadHostInstructionPointer(host_context)`. Add `last_host_ip`,
   `aliased_count` and `last_decision` to `GuestShutdownRecoveryRequest`.
3. Add `host_ip=`, `aliased=` and `decision=` to the `[repiu-shutdown]` line.
4. Add the `repiu_aot_probe --shutdown-recovery-policy` probe and update `CMakeLists.txt`.
5. Documentation: `ARCHITECTURE.md`, `docs/analysis/linux-port-frontier.md`,
   `docs/guides/linux-shutdown-check.md` (how to read the new fields), and the work log.
6. Verification: Win32 full build, core probe and the dedicated probe; WSL build and core probe; ten
   observations under Task 729's settings.

Not to be done: changing other x64 fault paths, changing defaults.
