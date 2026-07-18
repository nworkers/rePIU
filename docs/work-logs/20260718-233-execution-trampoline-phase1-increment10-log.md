# 20260718-233 작업 로그 — Phase 1 증분 10 (AOT 런타임 디스패치)

관련: [작업 지시서](../work-orders/20260718-233-execution-trampoline-decomposition-order.md) · [증분9 로그](20260718-233-execution-trampoline-phase1-increment9-log.md)

## 한 일

AOT 런타임 디스패치 22개 함수(2198–3166, 연속)를 `aot_runtime_dispatch.{h,cpp}`로 분리(CMake 등록): 번역 워커(AotTranslationWorkerProc), 게스트 코드쓰기 watch/fault(HandleAotGuestCodeWrite*), 인라인캐시 패치, 페이지 회수, 전이대상 해석(ResolveAotTransferTarget), 조건/간접/복귀/재진입 디스패치(HandleAot*Transfer, HandleAotReentry), Bump* 카운터. `GuestEntryThreadProc`(실행 스레드)는 트램폴린 잔류.

## 경계 처리 (빌드가 잡아냄)
- 이미 분리된 모듈 함수 사용 → `guest_memory_access.h`·`instruction_emulation.h` include 추가(ReadGuest*/WriteGuest*/IsGuestRange*/ReadGeneralRegister32/DecodeModRmMemoryAddress).
- 트램폴린-내부 공유 3개 승격: `IsGuestInstructionPointer`, `RecordExecutionProbe`, `RecordExecutionTrace`(익명 밖 재배치, execution_internal.h 선언). HandleSingleStepTrace도 사용하므로 승격이 맞음.
- `ResolveAotTransferTarget`의 기본 인자(`force_generation = false`)는 헤더에만 두고 .cpp 정의에서 제거.

`execution_trampoline.cpp`: 4,944 → 3,973줄.

## 검증
```
cmake --build build/win32_x86_dpmi --config Debug --target repiu_loader_win32  # green
```

## 다음
증분 11: linexe far-transfer / glide 게이트 / allocator 제어흐름 경계.
