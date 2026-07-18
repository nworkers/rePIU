# 20260718-233 execution_trampoline.cpp 분해 작업 지시서 (Phase 1)

관련 설계: [20260718-233-execution-trampoline-decomposition.md](../design/20260718-233-execution-trampoline-decomposition.md)

## 목적 / Goal

12,117줄 단일 `execution_trampoline.cpp`를 동작 보존하며 하위 시스템별 TU로 분리한다. Phase 1은 파일 재배치까지(중립화 seam 미도입).

Split the 12,117-line monolithic `execution_trampoline.cpp` into per-subsystem translation units while preserving behavior. Phase 1 covers file relocation only (no neutralization seam yet).

## 공통 규칙 / Ground rules

- 한 증분 = 한 모듈 추출 → 빌드 green → 커밋. 큰 배치로 묶지 않는다.
- 공유 타입/상수는 `thread_context.h`(최상위 namespace, 외부 링크)에 둔다. 익명 네임스페이스 내부 `#include` 금지.
- TU 경계를 넘어 호출되는 함수는 익명 네임스페이스에서 꺼내(외부 링크) 헤더에 선언한다. 링커가 undefined로 알려주면 그 심볼만 승격한다.
- 분리 `.cpp`는 `CMakeLists.txt`의 `repiu_exe` 소스 목록에 추가한다.
- 순수 코드 이동만 한다. 로직/제어흐름은 바꾸지 않는다. 출처 주석은 보존한다.

## 증분 목록 / Increments

1. **[완료]** `thread_context.h` 추출: 클린 파일 라인 40–566(StackSwitchCallState~ThreadContext, 주석 포함)을 최상위 namespace 헤더로 이동. `execution_trampoline.cpp`는 top-level `#include "thread_context.h"` 추가, 해당 인라인 정의 제거. 경계 심볼 승격 없음.
2. **[완료]** `port_io_emulator.{h,cpp}`: `RecordPortIo`/`IsObservedPortInitializationWrite`/`IsPortIoTraceCandidate`/`HandlePortIoInstruction` 이동. 경계 승격: `IsAotCacheAddress`, `WriteGuestBytes`(신규 `execution_internal.h`에 선언 + 정의를 익명 밖으로). 커밋 fe5489e.
3. `exception_rescue_win32.{h,cpp}`: `ExceptionDispatchScope`, `GuestStackVectoredExceptionHandler`, 복구 전역 이동. 경계 승격: `g_active_thread_context`(및 `g_recovery_*`), `DispatchGuestException`.
4~10. 텔레메트리 / 메모리 접근 / DOS INT21 / MSCDEX·DPMI / 명령어 에뮬 / AOT 디스패치 / linexe·glide 순으로 동일 패턴 반복. 각 증분 착수 시 경계 심볼을 grep으로 먼저 식별한다.
11. 순수 중립 leaf(MSF/LBA·패킷·`EvaluateAotCondition`·x86 flags)를 중립 dir로 이동.
12. **[완료]** **디렉토리 그룹화(최종, 필수)** — 결정 확정(2026-07-18): **서브시스템별 세분화**로, **모든 TU 분리가 끝난 뒤 한 번에** 이동. 그때까지 신규 파일은 `src/platform/win32/`에 플랫으로 둔다. 이동 시 파일 간 상대 include·`CMakeLists.txt` 소스 경로를 함께 정리한다.

   목표 하위 디렉토리 매핑(잠정):

   | 하위 디렉토리 | 대상 파일 |
   |---|---|
   | `execution/` | `execution_trampoline.cpp`, `thread_context.h`, `execution_internal.h`, `RunWin32ExecutionThread`/`AttemptWin32*` |
   | `exception/` | `exception_rescue_win32.{h,cpp}` (VEH·ExceptionDispatchScope·복구 전역) |
   | `io/` | `port_io_emulator.{h,cpp}` |
   | `dos/` | DOS INT21/2F·DPMI(31h)·MSCDEX 서비스 |
   | `cpu_emul/` | 명령어 디코드·세그먼트·traced 메모리 에뮬 |
   | `aot/` | AOT 런타임 디스패치 |
   | `boundary/` | linexe far-transfer·glide 게이트 |
   | `telemetry/` | 스냅샷·라이브 텔레메트리 |

## 검증 / Verification

각 증분 후:
```
cmake --build build/win32_x86_dpmi --config Debug --target repiu_exe
cmake --build build/win32_x86_dpmi --config Debug --target repiu_loader_win32
```
링크 성공 = 심볼 정합성. 실패 시 undefined 심볼을 외부 링크로 승격 후 재빌드.

## 롤백 / Rollback

각 증분이 독립 커밋이므로 문제 증분만 revert. 기준선은 clean HEAD(green).
