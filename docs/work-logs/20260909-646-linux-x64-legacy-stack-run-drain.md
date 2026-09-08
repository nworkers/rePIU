# Task 646 작업 로그: Linux x64 legacy 연속 스택 명령 처리

## 한국어

### 구현

- `instruction_emulation`에 `HandleConsecutiveLegacyStackInstructions`를 추가했습니다.
- 지원 범위는 기존 `PUSH/POP r32`와 segment push/pop handler로 제한했습니다.
- guest code 2바이트 범위 확인, EIP 전진 확인, 호출부 최대 16개 제한을 적용했습니다.
- x64 `aot_legacy_fallback`의 shared 일반/segment 스택 HLE와 직접 segment fault 경로에서 첫 성공 뒤 연속 처리하도록 연결했습니다.
- segment trace 결과에 뒤따라 처리한 명령 수 `drained`를 추가했습니다.
- 일반 스택 core probe에 혼합 연속열과 처리 상한 검증을 추가했습니다.

### 검증

- Linux x64 `repiu`: 빌드 성공
- Linux x64 `repiu_core_probe`: 빌드 성공
- 전체 core probe: `25/25`, 실패 0
- `general_stack`: 일반 push/pop, ESP 특수 순서, 범위 거부, 혼합 연속열, 처리 상한 모두 성공
- 기존 경고 `g_repiu_active_thread_context initialized and declared extern`만 발생했습니다.

실제 `pumpit2a` 실행의 주소 감시 결과:

```text
[repiu-segment-hle-watch] eip=0x010F1D79 opcode=0x06 ... esp=0x0158CC44->0x0158CC40 next_eip=0x010F1D7A size=1
[repiu-segment-hle-watch] eip=0x010F1D7A opcode=0x0F ... esp=0x0158CC40->0x0158CC3C next_eip=0x010F1D7C size=2
```

Task 645의 `PUSH FS` guest-stack 이탈은 해소됐습니다. 게임은 아직 완전 실행되지
않으며, 다음 종료는 host `RecoverGuestStackException`의 `UD2`에서 관측됐습니다.
fault-exit trace는 `no-host-frame-to-unwind`, guest stack 사용, active call state 없음으로
기록했습니다. 다음 작업에서는 복구 주소로 바뀌기 전 원래 guest fault를 추적해야 합니다.

## English

### Implementation

- Added `HandleConsecutiveLegacyStackInstructions` to `instruction_emulation`.
- Limited its scope to the existing `PUSH/POP r32` and segment push/pop handlers.
- Added a two-byte guest-code range check, EIP-progress check, and a caller-side maximum of 16 instructions.
- Connected it after the first successful shared general/segment stack HLE and direct segment-fault handling during x64 `aot_legacy_fallback`.
- Added the number of following instructions consumed as `drained` to segment trace results.
- Extended the general-stack core probe with mixed-run and processing-bound checks.

### Verification

- Linux x64 `repiu`: build passed
- Linux x64 `repiu_core_probe`: build passed
- Full core probe: `25/25`, zero failures
- `general_stack`: ordinary push/pop, ESP special ordering, range rejection, mixed run, and processing bound all passed
- The only warning was the pre-existing `g_repiu_active_thread_context initialized and declared extern` warning.

Real `pumpit2a` address watches showed:

```text
[repiu-segment-hle-watch] eip=0x010F1D79 opcode=0x06 ... esp=0x0158CC44->0x0158CC40 next_eip=0x010F1D7A size=1
[repiu-segment-hle-watch] eip=0x010F1D7A opcode=0x0F ... esp=0x0158CC40->0x0158CC3C next_eip=0x010F1D7C size=2
```

The Task 645 guest-stack escape at `PUSH FS` is resolved. The game still does
not complete; the next exit was observed at the `UD2` in host
`RecoverGuestStackException`. Fault-exit tracing recorded
`no-host-frame-to-unwind`, guest-stack use, and no active call state. The next
task must trace the original guest fault before its address is replaced by the
recovery destination.
