# 20260718-233 작업 로그 — Phase 1 증분 4 (라이브 텔레메트리 + Win32 스레드 API)

관련: [설계](../design/20260718-233-execution-trampoline-decomposition.md) · [작업 지시서](../work-orders/20260718-233-execution-trampoline-decomposition-order.md) · [증분3 로그](20260718-233-execution-trampoline-phase1-increment3-log.md)

## 한 일 / What was done

라이브 텔레메트리·실행 스냅샷 헬퍼를 `live_telemetry_snapshot.{h,cpp}`로 분리했다. 분리 과정에서 드러난 의존성 때문에 Win32 스레드 API 블록도 `win32_thread_api.h`로 함께 승격했다.

- `live_telemetry_snapshot.cpp`: `OpenSharedTelemetryMapping`, `WriteLiveTelemetrySnapshot`, `PollThreadUntilExit`, `CopySnapshotFromContextRecord`, `CaptureSuspendedThreadSnapshot`, `BuildSingleStepSnapshot`, `CopyThreadObservationToAttempt`.
- `live_telemetry_snapshot.h`: `SharedTelemetryMapping`(RAII) 구조체 + 외부 호출되는 함수 선언(`OpenSharedTelemetryMapping`, `PollThreadUntilExit`, `CopySnapshotFromContextRecord`, `CopyThreadObservationToAttempt`).
- `win32_thread_api.h`: kernel32 스레드 API 함수포인터 테이블(typedefs, `Win32ThreadApi`, `ResolveKernel32Function`, `inline GetWin32ThreadApi`).
- `BuildDosEnvironmentBlock`(텔레메트리 함수들 사이에 끼어 있던 DOS 헬퍼)는 트램폴린에 유지 → **비연속 추출**.
- `CMakeLists.txt`에 `live_telemetry_snapshot.cpp` 추가.

`execution_trampoline.cpp`: 11,323 → 10,185줄 (−1,138).

## 핵심 교훈 / Key lesson

첫 시도에서 **경계 피호출 함수 탐지 패턴이 불완전**했다(`Get*`, `DWORD` 반환형 함수를 누락). 그 결과 텔레메트리가 트램폴린의 `GetWin32ThreadApi`/`Win32ThreadApi`에 의존하고 `PollThreadUntilExit`가 범위에 섞여 있는 것을 놓쳤다. **빌드가 정확히 잡아냈고**(C3861/C2065), 이를 근거로 스레드 API를 공용 헤더로 승격해 해결했다.

→ 이후 증분에서는 경계 심볼 grep 시 반환형/접두사에 제한을 두지 말고, **빌드 오류를 승격 대상의 최종 진리로** 삼는다.

## 검증 / Verification

```
cmake --build build/win32_x86_dpmi --config Debug --target repiu_exe          # green (2 TU)
cmake --build build/win32_x86_dpmi --config Debug --target repiu_loader_win32  # green (링크 성공, unresolved 없음)
```

## 다음 / Next

증분 5: 게스트/섀도 메모리 접근 헬퍼(`WriteGuestUInt*`/`ReadGuest*`/`WriteShadowMemory`/`ReadShadow*`/`ReadGuestAsciz`/`AppendConsoleOutput`). 이들은 호출자가 매우 많아 헤더 선언이 다수 필요할 것.
