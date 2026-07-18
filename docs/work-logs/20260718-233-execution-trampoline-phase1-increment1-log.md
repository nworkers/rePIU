# 20260718-233 작업 로그 — Phase 1 증분 1 (공유 상태 헤더 추출)

관련: [설계](../design/20260718-233-execution-trampoline-decomposition.md) · [작업 지시서](../work-orders/20260718-233-execution-trampoline-decomposition-order.md)

## 한 일 / What was done

`execution_trampoline.cpp`의 최상위 공유 타입/상수 블록(라인 40–566: `StackSwitchCallState`, `DosInterruptVectorShadow`, `ShadowWriteProvenance`, `AotWorkerOperation`, DOS/DPMI 식별 상수, `ThreadContext`)을 새 헤더 `src/platform/win32/thread_context.h`(최상위 `namespace repiu::platform::win32`, 외부 링크)로 이동했다. `execution_trampoline.cpp`는 시스템 include 직후(익명 네임스페이스 밖) top-level `#include "thread_context.h"`를 추가하고 인라인 정의를 제거했다.

Moved the top-of-file shared type/constant block (lines 40–566) into a new header `thread_context.h` at named namespace scope (external linkage), and replaced the inline definitions in `execution_trampoline.cpp` with a top-level include placed outside the anonymous namespace.

- `execution_trampoline.cpp`: 12,117줄 → 11,592줄 (−525).
- `thread_context.h`: 신규 554줄.
- 원본의 출처 주석(DOS/32A 참조, DOS4GW resident handler 근거, LINEXE 토폴로지 근거)은 그대로 보존.

## 핵심 판단 / Key decisions

- 이전 미완성 시도(stash됨)는 이 헤더를 **익명 네임스페이스 안에서** include하여 중첩 네임스페이스가 되고, `.cpp`도 CMake 미등록에 경계 심볼 미승격이라 링크 불가였다. 본 증분은 헤더를 **top-level**로 include하여 이를 회피했다.
- 증분 1은 타입/상수만 이동하므로 TU 경계를 넘는 함수 호출이 없어 경계 심볼 승격이 불필요하다(가장 낮은 위험의 첫 단계).
- stash된 `thread_context.h`는 코드는 동일하나 출처 주석을 누락했으므로 재사용하지 않고 클린 파일 원문에서 재생성했다.

## 검증 / Verification

```
cmake --build build/win32_x86_dpmi --config Debug --target repiu_exe        # green
cmake --build build/win32_x86_dpmi --config Debug --target repiu_loader_win32 # green (link OK)
```
기존과 동일한 C4819 코드페이지 경고만 발생(무해). 순수 코드 이동이므로 링크 성공이 심볼 정합성을 확인한다.

## 다음 / Next

작업 지시서의 증분 2(`port_io_emulator` 분리, 경계 심볼 `IsAotCacheAddress`·`WriteGuestBytes` 외부 링크 승격)부터 이어서 진행한다.
