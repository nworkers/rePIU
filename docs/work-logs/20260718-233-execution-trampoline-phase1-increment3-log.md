# 20260718-233 작업 로그 — Phase 1 증분 3 (예외 디스패치 TU 분리)

관련: [설계](../design/20260718-233-execution-trampoline-decomposition.md) · [작업 지시서](../work-orders/20260718-233-execution-trampoline-decomposition-order.md) · [증분2 로그](20260718-233-execution-trampoline-phase1-increment2-log.md)

## 한 일 / What was done

VEH(vectored exception handler) 스캐폴딩을 별도 TU `exception_rescue_win32.{h,cpp}`로 분리했다.

- `ExceptionDispatchScope`(RAII 텔레메트리 스코프)를 헤더에 **inline 클래스**로 이동(자기완결: ThreadContext 멤버 + Interlocked만 사용).
- 기존 `GuestStackVectoredExceptionHandler`(약 542줄 본체, 가드 포함)를 **`LONG DispatchGuestException(EXCEPTION_POINTERS*)` 으로 개명**하고 익명 네임스페이스 밖(외부 링크)으로 재배치. **반환형/본문 변환 없음**(EXCEPTION_CONTINUE_* 그대로 유지).
- 얇은 진입점 `LONG WINAPI GuestStackVectoredExceptionHandler(...) { return DispatchGuestException(...); }`만 `exception_rescue_win32.cpp`에 둠. `AddVectoredExceptionHandler` 등록부 2곳은 이 forwarder를 참조.
- `CMakeLists.txt`에 `exception_rescue_win32.cpp` 추가.

`execution_trampoline.cpp`: 11,371 → 11,323줄.

## 핵심 설계 판단 / Key decisions

- 거대한 VEH 본체는 수십 개의 익명 네임스페이스 핸들러를 호출하므로, 이를 다른 TU로 옮기면 대량 승격이 필요하다. 그래서 **본체는 트램폴린에 `DispatchGuestException`으로 남기고(외부 링크 하나만 승격)**, 얇은 forwarder + RAII 스코프만 분리했다.
- 반환형을 `bool`로 바꾸는 대신 `LONG` 그대로 두어 **본문을 한 줄도 고치지 않고** 개명·재배치만 했다(회귀 위험 최소).
- 복구 전역(`g_active_thread_context`, `g_recovery_*`)은 grep 결과 **트램폴린 전용**이라 승격 불필요 → 익명 네임스페이스에 그대로 유지. 유일한 경계 심볼은 `DispatchGuestException`.

## 검증 / Verification

```
cmake --build build/win32_x86_dpmi --config Debug --target repiu_exe          # green (execution_trampoline.cpp, exception_rescue_win32.cpp)
cmake --build build/win32_x86_dpmi --config Debug --target repiu_loader_win32  # green (링크 성공, unresolved 없음)
```
크로스-TU 심볼(`DispatchGuestException`, `GuestStackVectoredExceptionHandler`) 정합성 확인. VEH 등록부 2곳 정상 해결.

## 다음 / Next

증분 4~: 텔레메트리 스냅샷 / 게스트·섀도 메모리 접근 / DOS INT21 / MSCDEX·DPMI / 명령어 에뮬 / AOT 디스패치 / linexe·glide / 순수 중립 leaf. 각 착수 시 경계 심볼 grep 선행.
