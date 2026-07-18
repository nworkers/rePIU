# 20260718-233 작업 로그 — Phase 1 증분 9 (명령어 에뮬레이션)

관련: [작업 지시서](../work-orders/20260718-233-execution-trampoline-decomposition-order.md) · [증분8 로그](20260718-233-execution-trampoline-phase1-increment8-log.md)

## 한 일 (최대 증분)

x86 명령어 에뮬레이션 47개 함수를 `instruction_emulation.{h,cpp}`로 분리(CMake 등록):
- 레지스터/플래그/디코드 헬퍼(ReadRegister16, WriteRegister16/8, SetCompareFlags8, ReadGeneralRegister32, WriteGeneralRegister32, ReadGeneralRegister8, DecodeModRmMemoryAddress, HasEvenParity, UpdateAdd32/Logical32/Subtract8Flags 등)
- 세그먼트 레지스터 명령 핸들러(HandleSegment*, ReadSegment*, RecordGuestSegment*, RecordDosEnvironmentAccess)
- traced 메모리 명령 핸들러(HandleTracedMemory*, HandleTracedFpuMemoryInstruction)
- REP 문자열 연산(HandleRepStosd/Movs/Cmpsb/Lodsb, CopyHostMemoryWithoutVehRecursion)
- traced DOS/DPMI/마우스 인터럽트 래퍼(HandleTracedDosInterrupt21/2F, HandleTracedDpmiInterrupt31, HandleTracedMouseInterrupt33)

**비연속 추출**: [1009–3858] ∪ [3936–4158], 중간 `HandleOriginalFatalBreakpoint`(3860–3933)는 트램폴린 잔류.

## 경계 처리
- 추가 승격 불필요(증분 6·8에서 substrate·ResolveSegmentLinearRange 이미 승격).
- **혼합 forward-decl 블록**: 이동 함수의 anon forward 선언 29개(66줄)를 이름 매칭으로만 제거하고, 잔류 함수(HandlePrivilegedTrapInstruction, HandleGuestLowMemoryReadFault, AOT 등) 선언은 보존. 실행 전 제거 목록을 dry-run으로 검증.
- traced 인터럽트 래퍼가 dos/dpmi 모듈 함수를 호출하므로 emul 모듈이 `dos_int21_services.h`·`dpmi_mscdex_services.h`를 include.

`execution_trampoline.cpp`: 8,082 → 4,944줄 (−3,138). 원본 12,117 대비 59% 감소.

## 검증
```
cmake --build build/win32_x86_dpmi --config Debug --target repiu_exe          # green (2 TU, C2572 없음)
cmake --build build/win32_x86_dpmi --config Debug --target repiu_loader_win32  # green (링크 성공)
```

## 다음
증분 10: AOT 런타임 디스패치.
