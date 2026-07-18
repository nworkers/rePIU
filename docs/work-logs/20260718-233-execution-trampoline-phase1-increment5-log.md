# 20260718-233 작업 로그 — Phase 1 증분 5 (게스트/섀도 메모리 접근)

관련: [설계](../design/20260718-233-execution-trampoline-decomposition.md) · [작업 지시서](../work-orders/20260718-233-execution-trampoline-decomposition-order.md) · [증분4 로그](20260718-233-execution-trampoline-phase1-increment4-log.md)

## 한 일 / What was done

게스트 선형/섀도 메모리 접근 프리미티브를 `guest_memory_access.{h,cpp}`로 분리했다(지금까지 중 가장 결합이 깊음 — 이 함수들은 명령어 핸들러 전반에서 호출됨).

- `guest_memory_access.cpp`: `IsGuestRangeReadable`, `IsGuestRangeWritable`, `WriteGuestUInt8/16/32`, `ReadGuestUInt32`, `WriteShadowMemory`, `ReadShadowUInt32/8`, `AppendConsoleOutput`, `ReadGuestAsciz`.
- `guest_memory_access.h`: 위 함수 선언(호출자 다수 — `IsGuestRangeReadable` 37, `IsGuestRangeWritable` 13곳 등).
- `CMakeLists.txt`에 `guest_memory_access.cpp` 추가.

`execution_trampoline.cpp`: 10,185 → 9,853줄.

## 경계 처리 / Boundary handling

1. **`NoteSuccessfulAotGuestWrite` 승격**: 메모리 write 헬퍼가 이 AOT 훅(111줄, 8002–8112)을 호출. 익명 밖으로 재배치(외부 링크)하고 `execution_internal.h`에 선언, forward 선언(639–641) 제거. (AOT 모듈은 증분 9에서 분리 예정이므로 그때 이 함수도 함께 이동 검토.)
2. **`AppendConsoleOutput` 기본 인자**: 정의에 있던 `bool stderr_stream = false`를 헤더 선언으로 이관하고 `.cpp` 정의에서는 제거(기본 인자 중복 방지).
3. **NOMINMAX 매크로 충돌 [공유 수정]**: 메모리 헬퍼가 `std::numeric_limits<std::uint32_t>::max()`를 사용하는데, `thread_context.h`가 NOMINMAX 없이 `<windows.h>`를 포함해 `max` 매크로와 충돌(C2589/C2059). `thread_context.h`의 windows.h include 앞에 `#ifndef NOMINMAX / #define NOMINMAX`를 추가해 **모든 추출 TU를 일괄 보호**.

## 검증 / Verification

```
cmake --build build/win32_x86_dpmi --config Debug --target repiu_exe          # green (전체 TU 재컴파일)
cmake --build build/win32_x86_dpmi --config Debug --target repiu_loader_win32  # green (링크 성공, unresolved 없음)
```
첫 빌드에서 NOMINMAX 충돌을 잡아냈고(빌드=진리), 공유 헤더 수정으로 해결.

## 다음 / Next

증분 6: DOS INT 21h/2Fh 서비스(`HandleDos*`, `HandleDosInterrupt21/2F`). 결합 심볼을 grep으로 선식별하되 빌드로 최종 확인.
