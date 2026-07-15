# 2026-07-15 POP ES/FS/GS HLE 복원 및 하드웨어 컨텍스트 동기화 작업 지시서
# 2026-07-15 POP ES/FS/GS HLE Restoration and Hardware Context Sync Work Order

## 1. 목적 (Objective)

Task 206에서 POP ES/FS/GS HLE를 롤백한 이후 발생한 LINEXE 모듈 스캔 루프 무한 반복 결함을 수정합니다. POP ES/FS/GS 명령을 다시 HLE로 처리하되, shadow 레지스터뿐만 아니라 하드웨어 레지스터(CONTEXT)도 명시적으로 동기화하여 상태 일관성을 보장합니다.

Fix the infinite loop regression in the LINEXE module scan loop that occurred after rolling back POP ES/FS/GS HLE in Task 206. Re-enable HLE for POP ES/FS/GS instructions, explicitly synchronizing both shadow registers and hardware registers (CONTEXT) to ensure state consistency.

## 2. 작업 내용 (Tasks)

1. `src/platform/win32/execution_trampoline.cpp` 파일 수정
   * `HandleSegmentPopInstruction` 함수에서 다음 opcode를 검사하고 처리하도록 확장:
     * `0x07` (POP ES)
     * `0x1F` (POP DS) - 기존 유지
     * `0x0F 0xA1` (POP FS)
     * `0x0F 0xA9` (POP GS)
   * 각 opcode에 따라 메모리에서 16비트 selector를 읽어옵니다.
   * `RecordGuestSegmentLoad`를 호출하여 shadow 레지스터를 갱신합니다.
   * `win32_context->SegEs`, `win32_context->SegDs`, `win32_context->SegFs`, `win32_context->SegGs` 필드 중 해당하는 것을 읽어온 selector 값으로 갱신합니다.
   * 명령어의 바이트 길이에 맞게 `win32_context->Eip`를 증가시킵니다.
   * 성공적으로 처리되었으면 `true`를 반환합니다.

## 3. 검증 계획 (Verification Plan)

1. 빌드 수행 (`repiu_loader_win32.exe`)
2. `REPIU_EXECUTION_TIMEOUT=30000` 설정 후 디버그 빌드 실행
3. 로그 확인:
   * `LINEXE bridge entry` 카운트가 `0`보다 큰지 확인
   * `Glide gate entries` 카운트가 증가하는지 확인
   * `Glide window opens` 카운트 증가 및 실제 OpenGL 창이 표시되는지 확인
4. 예기치 않은 세그먼트 예외(0xC0000005 등)가 발생하지 않고 게임 진행 상태(`progress`) 카운터가 증가하는지 확인
