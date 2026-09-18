# Task 705 작업 로그 — Linux x64 thread interrupt no-op context 보존

## 수행 결과

host-thread interrupt callback 반환형을 bool로 바꾸어 native context write-back을
명시적으로 제어했습니다. 순수 sampling과 guest/AOT 밖 shutdown recovery 거절은 false를
반환하며, 실제 register를 편집한 recovery만 true를 반환합니다. Linux signal handler와
Windows backend 모두 true인 경우에만 context를 저장합니다.

이 변경은 host code에서 사용 중인 64-bit R15를 32-bit guest ESP snapshot으로 덮어쓰지
않습니다. interrupt delivery/answer 상태, timeout/abandon handshake, Task 507/508의
refused-recovery 즉시 종료 정책은 유지했습니다.

## 검증

- Linux x64 Debug `repiu`, `repiu_core_probe` 빌드 성공
- core probe 27/27 성공
- host-thread interrupt sample/edit/refusal/abandon group 전체 성공
- 수정 전 30초 `pumpit2a` timeout: `MOV EAX,[R15]` SIGSEGV와 core dump 재현
- 수정 후 동일 실행: SIGSEGV 없음, `attempts=40 answered=1 recovered=0 stopped=0`
- 수정 후 `probe-dump`, `immediate-exit` 표식 출력 및 외부 제한 전에 자체 종료

추가로 Win32 x86 Debug 빌드를 시도했습니다. 이번 작업이 변경한 `host_thread.cpp`,
`execution_trampoline.cpp`, `native_phase_sampler.cpp`는 MSVC 컴파일을 통과했습니다.
전체 빌드는 이후 별도 `linux_x64_native_write_trace.cpp`가 Win32에서
`linux_x64_aot_frame.h`의 Linux x64 layout static assertion을 활성화하는 기존 구성
문제로 실패했습니다. Task 705 callback 계약 관련 컴파일 오류는 없었습니다.
후속 Task 706에서 이 플랫폼 source 편성을 바로잡았고 Win32 x86 Debug 전체 빌드와
core probe를 완료했습니다.

---

## English

### Result

Host-thread interrupt callbacks now return a bool that explicitly controls
native context write-back. Pure sampling and refused shutdown recovery outside
guest/AOT code return false; only recovery that edits registers returns true.
Both the Linux signal handler and Windows backend store context only for true.

This prevents a 64-bit R15 used by host code from being overwritten by a
32-bit guest-ESP snapshot. Interrupt delivery/answer state, timeout/abandon
handshake, and the Task 507/508 immediate-exit policy after refused recovery are
unchanged.

### Verification

- Linux x64 Debug `repiu` and `repiu_core_probe` built successfully.
- All 27 core-probe groups passed.
- Host-thread interrupt sample/edit/refusal/abandon groups all passed.
- Before the fix, a 30-second `pumpit2a` timeout reproduced the
  `MOV EAX,[R15]` SIGSEGV and core dump.
- Under identical conditions after the fix, no SIGSEGV occurred and shutdown
  reported `attempts=40 answered=1 recovered=0 stopped=0`.
- The process printed `probe-dump` and `immediate-exit` and ended before the
  outer limit.

An additional Win32 x86 Debug build was attempted. The Task 705 translation
units `host_thread.cpp`, `execution_trampoline.cpp`, and
`native_phase_sampler.cpp` all compiled under MSVC. The full build later failed
in unrelated `linux_x64_native_write_trace.cpp`, where the Win32 build activates
Linux x64 layout static assertions from `linux_x64_aot_frame.h`. No callback
contract compilation error occurred.
Follow-up Task 706 corrected that platform source assignment and completed the
full Win32 x86 Debug build and core probe.
