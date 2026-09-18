# Task 704 작업 로그 — Linux x64 IRETD HLE

## 수행 결과

Linux x64 INT 8 주입 frame의 CS를 signal context의 host selector에서 interrupted
EIP를 포함하는 logical guest executable selector로 변경했습니다. selector 해석이
실패하면 frame 기록과 pending tick 소비 전에 주입을 보류합니다.

전용 `interrupt_return` CPU-emulation 모듈을 추가했습니다. handler는 prefix 없는
32-bit `IRETD`와 12-byte EIP/CS/EFLAGS frame, executable k32 복귀 target을 검증한
뒤에만 EIP, CS, EFLAGS, ESP를 복원합니다. 공용 HLE dispatcher와 fault-level chain을
같은 handler에 연결했고, 성공한 fault-level 복귀는 handled-boundary AOT 재진입을
사용합니다. 원본 ISR body와 i386 native 경로는 변경하지 않았습니다.

## 검증

- Linux x64 Debug `repiu`, `repiu_core_probe` 빌드 성공
- core probe 27/27 성공
- IRETD probe: `iretd_hle=1,bad_selector=1,bad_frame=1`
- 실제 `pumpit2a`에서 guest `0x0103F1F5` IRETD 후 `EIP=0x01054480`,
  `ESP=0x0158CBEC`, cache target `0x200C85FB`로 재진입 확인
- 45초 동안 약 800회의 INT 8 주입과 heartbeat/dispatch 진행 확인
- 기존 IRETD `no-host-frame-to-unwind` 미재현

제한 실행은 강제 `timeout` 종료 과정의 별도 host-side SIGSEGV로 끝났습니다. 반복
IRETD 경로는 그 이전까지 계속 진행했으며, graceful exit와 forced shutdown 분리는
후속 진단 범위로 남겼습니다.

---

## English

### Result

Linux x64 INT 8 injection now writes the logical executable guest selector that
contains the interrupted EIP instead of the signal context's host selector.
Failure to resolve it defers injection before frame writes or pending-tick
consumption.

A dedicated `interrupt_return` CPU-emulation module now validates unprefixed
32-bit `IRETD`, its 12-byte EIP/CS/EFLAGS frame, and an executable k32 return
target before restoring EIP, CS, EFLAGS, and ESP. Shared HLE and the fault-level
chain use the same handler, and a successful fault-level return uses the shared
handled-boundary AOT re-entry. The original ISR body and native i386 path remain
unchanged.

### Verification

- Linux x64 Debug `repiu` and `repiu_core_probe` built successfully.
- All 27 core-probe groups passed.
- IRETD probe: `iretd_hle=1,bad_selector=1,bad_frame=1`.
- Real `pumpit2a` returned from guest `0x0103F1F5` to `EIP=0x01054480`,
  `ESP=0x0158CBEC`, and cache target `0x200C85FB`.
- Roughly 800 INT 8 injections and advancing heartbeat/dispatch counters were
  observed over 45 seconds.
- The former IRETD `no-host-frame-to-unwind` did not recur.

The limited run ended with a separate host-side SIGSEGV during forced `timeout`
termination. The repeating IRETD path continued until that point; distinguishing
graceful exit from forced shutdown remains follow-up diagnostic work.
