# DOS system date 설정 HLE 작업 지시

설계: [20260815-484-dos-set-system-date.md](../design/20260815-484-dos-set-system-date.md)

1. 공용 DOS 날짜 값·검증·일수 변환 모듈을 추가합니다.
2. 실행 context에 host date 대비 가상 DOS date offset을 저장합니다.
3. Function 2Bh handler를 추가하고 일반/traced INT 21h 분기에 연결합니다.
4. Function 2Ah가 가상 날짜 offset을 반영하도록 갱신합니다.
5. 날짜 계약 probe를 전체 AOT probe에 연결합니다.
6. 관련 analysis/KB와 작업 로그를 갱신합니다.
7. Win32 x86 Debug/Release 빌드와 전체 probe를 실행합니다.

## 완료 조건

유효한 날짜는 `AL=00h`, 잘못된 날짜는 `AL=FFh`이며 잘못된 요청은 기존 offset을
바꾸지 않아야 합니다. 호스트 날짜를 변경하지 않고 Function 2Ah가 설정 결과를 반환해야
하며 기존 전체 probe가 통과해야 합니다.

---

# DOS Set-System-Date HLE Work Order

Design: [20260815-484-dos-set-system-date.md](../design/20260815-484-dos-set-system-date.md)

1. Add shared DOS date values, validation, and day conversion.
2. Store a virtual DOS date offset from host date in the execution context.
3. Add Function 2Bh and route both ordinary and traced INT 21h paths to it.
4. Make Function 2Ah apply the virtual date offset.
5. Connect a date-contract probe to the full AOT probe.
6. Update the relevant analysis, KB, and work log.
7. Run Win32 x86 Debug/Release builds and full probes.

## Completion criteria

A valid date returns `AL=00h`; an invalid date returns `AL=FFh` without changing
the previous offset. Function 2Ah must return the set date without changing the
host clock, and all existing probes must pass.
