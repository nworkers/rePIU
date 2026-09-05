# 작업 로그 20260905-587 — Linux x64 세그먼트 PUSH HLE

설계: [20260905-587](../design/20260905-587-linux-x64-segment-push-hle.md)  
작업 지시: [20260905-587](../work-orders/20260905-587-linux-x64-segment-push-hle.md)

## 결과

`pumpit2a`의 이전 Linux x64 frontier `0x010F4A96`을 원본 실행 파일에서 덤프해
`06` (`push es`)로 확인했습니다. `instruction_emulation`에 `ES`, `SS`, `DS`, `FS`,
`GS` segment PUSH handler를 추가했습니다. handler는 shadow selector를 32-bit guest
stack slot에 zero-extended dword로 저장하고, ESP/EIP만 갱신합니다.

초기 연결은 일반 fault chain만 덮어 AOT boundary가 legacy single-step으로 먼저
들어가는 경로를 놓쳤습니다. 따라서 `DispatchGuestHleHandlers`의 opcode fast switch와
일반 fallback에도 동일 handler를 연결했습니다. 이로써 원본 `push es`를 host long
mode로 실행하는 경로를 피합니다.

## 검증

* Linux x64 `repiu`를 `build/linux_x64_debug`에서 다시 링크했습니다.
  `repiu_exe`와 `repiu` target은 성공했습니다. GCC는 기존
  `g_repiu_active_thread_context`의 `extern` 초기화 경고 하나를 냈습니다.
* `REPIU_GUEST_WATCH=0x010F4A96`로 `pumpit2a`를 실행했습니다. AOT boundary
  `at=0x2004F917`와 guest step `0x010F4A96`까지 도달했습니다.
* 실행은 더 이상 guest 주소 `0x010F4A96`의 SIGILL을 보고하지 않고, 이후
  `0x402AAE46` SIGTRAP와 `0x402AAE47` SIGILL에서 멈췄습니다. 이 high-address
  frontier의 원인은 아직 확인되지 않았습니다.
* Linux x64 core probe는 `dos_file_handle_cache` 뒤에서 비정상적으로 오래 정지해
  이번 작업에서 완주 결과를 얻지 못했습니다. 이 정지는 segment PUSH handler를
  직접 호출하는 probe가 아니므로, 다음 작업에서 작은 독립 probe로 보강합니다.

## 후속 작업

1. segment PUSH HLE의 selector 값, 4-byte stack write, ESP/EIP 보존을 직접 검증하는
   Linux x64 probe를 추가합니다.
2. `0x402AAE46/47`의 full host RIP와 AOT/guest provenance를 기록하여 PUSH 뒤
   high-half address fault를 분리합니다.

---

# Work log 20260905-587 — Linux x64 Segment PUSH HLE

Design: [20260905-587](../design/20260905-587-linux-x64-segment-push-hle.md)  
Work order: [20260905-587](../work-orders/20260905-587-linux-x64-segment-push-hle.md)

## Result

Dumping the former Linux x64 `pumpit2a` frontier at `0x010F4A96` confirmed
byte `06` (`push es`). A segment-PUSH handler now covers ES, SS, DS, FS, and
GS. It stores a zero-extended shadow selector in a 32-bit guest-stack slot and
updates only ESP and EIP.

The initial ordinary-fault-chain wiring missed the AOT-boundary route, which
entered legacy single-stepping first. The handler was therefore also added to
the `DispatchGuestHleHandlers` opcode fast switch and general fallback, keeping
original `push es` out of host long-mode execution.

## Verification

* Linux x64 `repiu_exe` and `repiu` linked successfully in
  `build/linux_x64_debug`. GCC emitted one pre-existing warning for initialized
  `extern g_repiu_active_thread_context`.
* `pumpit2a` with `REPIU_GUEST_WATCH=0x010F4A96` reached the AOT boundary at
  `0x2004F917` and the guest step at `0x010F4A96`.
* The run no longer reported SIGILL at guest `0x010F4A96`; it then stopped at
  SIGTRAP `0x402AAE46` and SIGILL `0x402AAE47`. The high-address frontier is
  unresolved.
* The Linux x64 core probe stalled abnormally after `dos_file_handle_cache`,
  so this task has no complete core-probe result. It does not directly invoke
  the segment-PUSH handler; the next task should add a small independent
  probe.

## Follow-up

1. Add a Linux x64 probe that directly checks selector value, four-byte stack
   write, and ESP/EIP preservation for segment PUSH HLE.
2. Record full host RIP and AOT/guest provenance for the post-PUSH
   `0x402AAE46/47` fault.
