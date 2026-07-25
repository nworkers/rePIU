# 20260726-300 작업 로그: guest instruction probe fail-closed

설계: [docs/design/20260726-300-guest-instruction-probe-fail-closed.md](../design/20260726-300-guest-instruction-probe-fail-closed.md)

작업 지시: [docs/work-orders/20260726-300-guest-instruction-probe-fail-closed.md](../work-orders/20260726-300-guest-instruction-probe-fail-closed.md)

## 한국어

### 결과

비정상 guest EIP에서 원래 예외를 처리하려다 HLE opcode probe가 다시 access
violation을 일으키는 2차 크래시를 fail-closed로 차단했습니다.

- `DispatchGuestException`은 AOT→guest 주소 변환 뒤 15바이트 decode window를
  검증합니다. guest-stack 실행에서 유효하지 않으면 decoder를 호출하지 않고 원래
  exception code/context를 캡처한 뒤 기존 host recovery로 종료합니다.
- 재사용되는 `DispatchGuestHleHandlers`도 같은 15바이트 진입 조건을 적용합니다.
- DOS 21h/2Fh, DPMI 31h, mouse 33h traced handler는 각각 독립적인 2바이트
  guest-range guard를 가집니다.

주소를 추측하거나 `RET`을 자동 복구하지 않으므로 원본 실패 의미는 유지됩니다.

### 검증

별도 VS2022 Win32 Debug tree에서 `repiu_loader_win32` 증분 빌드가 성공했습니다.
변경된 `instruction_emulation.cpp`와 `execution_trampoline.cpp`가 compile/link됐고
기존 C4819 경고 외 새 오류는 없습니다. `git diff --check`도 통과했습니다.

Task 299의 45초 실행에서 이미 원래 `ESP-12` 실패가 제거됐으므로 Task 300 guard의
실제 invalid-EIP 분기는 의도적으로 재현하지 않았습니다. 이 guard는 향후 다른 1차
guest 오류가 발생해도 host decoder AV로 증거가 바뀌지 않게 하는 방어 계층입니다.

---

## English

Added fail-closed decode gates so an invalid guest EIP cannot trigger a
secondary host AV while HLE probes inspect opcodes. Guest-stack VEH dispatch
and the reusable handler chain require a 15-byte x86 decode window, and all
four traced interrupt handlers independently require their two opcode bytes.
Invalid addresses preserve and capture the original exception instead of
guessing a recovery target.

The incremental VS2022 Win32 Debug loader build passed, compiling and linking
both changed translation units; only existing C4819 warnings remained.
The invalid-EIP branch was not force-reproduced because Task 299 already
removed the primary ESP-12 failure.
