# Task 737: Win32 pumpitea 시작 크래시 작업 지시

설계: [20260926-737](../design/20260926-737-win32-pumpitea-indirect-fallback-and-mode16-copy.md)

## 한국어

1. Win32 pumpitea의 `0xC0000005`를 예외 스택, 마지막 간접 전송, 기능 스위치 A/B로 좁힌다.
2. `HandleAotIndirectTransfer`가 i386에서 해석 실패 시 CALL의 push를 되돌리게 하고, i386
   host-dispatch miss tail의 CALL fallback이 두 슬롯을 버리게 한다. Task 650 probe를 갱신한다.
3. dynamic 번역 실패 메시지에 단계와 첫 decode 실패 항목을 넣는다.
4. i386 `kCopy`가 16-bit 코드 객체 기록을 HLE 경계로 방출하게 한다.
5. 지연 루프 batcher가 `jge exit; jmp back` 형식을 받아들이게 한다.
6. Win32와 Linux x64에서 core probe, pumpitea, pumpit2a로 검증한다.
7. 설계, 작업 로그, ARCHITECTURE(Task 650 절 정정 포함), analysis, `EXE_DESIGN.*`를 갱신하고
   커밋한다.

## English

1. Narrow Win32 pumpitea's `0xC0000005` with the exception stack, the last indirect transfer and an
   A/B over the feature switches.
2. Make `HandleAotIndirectTransfer` undo a CALL's commit on i386 when resolution fails, and make the
   i386 host-dispatch miss tail's CALL fallback drop both slots. Update the Task 650 probe.
3. Put the stage and the first decode-failure sample into the dynamic translation failure message.
4. Make i386 `kCopy` emit 16-bit code-object records as an HLE boundary.
5. Make the delay-loop batcher accept the `jge exit; jmp back` form.
6. Verify with the core probe, pumpitea and pumpit2a on Win32 and Linux x64.
7. Update the design, work log, ARCHITECTURE (including the Task 650 correction), analysis and
   `EXE_DESIGN.*`, then commit.
