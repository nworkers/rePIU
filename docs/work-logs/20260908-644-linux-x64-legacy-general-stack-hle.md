# Task 644 작업 로그: Linux x64 legacy 일반 스택 명령 HLE

설계: [20260908-644](../design/20260908-644-linux-x64-legacy-general-stack-hle.md)  
작업 지시: [20260908-644](../work-orders/20260908-644-linux-x64-legacy-general-stack-hle.md)  
분석: [linux-port-frontier 3.81](../analysis/linux-port-frontier.md)

## 한국어

`instruction_emulation`에 한 바이트 `PUSH/POP r32` handler를 추가하고, x64
`aot_legacy_fallback`일 때만 shared HLE dispatch에서 호출했습니다. guest arena
읽기·쓰기 helper를 사용하며 `PUSH ESP`의 감소 전 값과 `POP ESP`의 최종 register
write 순서를 보존합니다.

Linux x64 전용 core probe를 추가했습니다. 최초 probe mapping을 commit하지 않아
접근 fault가 발생한 문제를 수정했고, 내부 handler 참조로 정적 라이브러리의 GL backend가
link에 포함되므로 Linux x64 probe target에 `GL`을 명시했습니다.

### 검증

* Linux x64 `repiu` 빌드: 통과
* Linux x64 `repiu_core_probe` 빌드: 통과
* core probe: `25/25`, failures `0`
* `general_stack`: 일반 push/pop, `PUSH ESP`, `POP ESP`, 범위 거부 모두 통과
* 실제 `pumpit2a`: `PUSH ES` 진입 ESP `0x0158CC50`으로 기존보다 `-20`
* 기존 `0x011A8E10` zero-block 반환은 사라짐

실제 실행은 아직 정상 완료되지 않습니다. `0x010F1E56 RET`의 새 source는
`0x00000024`, 소비 슬롯은 `0x0158CC64`입니다. 다음 작업은 이 슬롯의 writer와
`0x010F1D74` 진입 transfer를 연결해 guest return address 누락 경계를 찾는 것입니다.

## English

Added a one-byte `PUSH/POP r32` handler to `instruction_emulation` and invoked
it from shared HLE dispatch only during x64 `aot_legacy_fallback`. It uses guest
arena access helpers and preserves the pre-decrement value for `PUSH ESP` and
the final register-write ordering for `POP ESP`.

Added a Linux x64 core probe. The initial probe mapping fault was corrected by
committing the reservation, and the Linux x64 probe target now names `GL`
because referencing the internal handler pulls the static library's GL backend
into the link.

### Verification

* Linux x64 `repiu` build: passed
* Linux x64 `repiu_core_probe` build: passed
* Core probe: `25/25`, failures `0`
* `general_stack`: ordinary push/pop, `PUSH ESP`, `POP ESP`, and range rejection passed
* Real `pumpit2a`: ESP at `PUSH ES` entry is `0x0158CC50`, a correction of `-20`
* The former `0x011A8E10` zero-block return is gone

The real run still does not complete normally. The new source consumed by
`RET` at `0x010F1E56` is `0x00000024` from slot `0x0158CC64`. The next task is
to correlate that slot's writer with the transfer into `0x010F1D74` and locate
the missing guest return-address boundary.
