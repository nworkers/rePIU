# 작업 로그 648: Linux x64 unresolved return provenance

## 결과

Task 647의 실제 실행에서 확인한 unresolved x64 return 경계를 최종 fault
trace까지 연결했습니다. `LinuxX64TransferFailureProvenance`는 최근 x64
resolver 실패의 producer EIP, target EIP, transfer 후 guest ESP, transfer
종류, 실패 이유를 `ThreadContext`의 고정 크기 record에 보존합니다.

target `0`은 invalid 상태로 버리지 않고 유효한 관측값으로 남겼습니다. 다음
resolver 진입 때 이전 record를 지우며, 성공한 transfer가 있으면 실패 record가
남지 않습니다. `REPIU_FAULT_EXIT_TRACE=1`에서는 다음 field를 기존
`[repiu-exit]` 줄에 추가합니다.

```text
x64_transfer_valid=1
x64_transfer_producer_eip=0x010F1E56
x64_transfer_target_eip=0x00000000
x64_transfer_guest_esp=0x0158CC5C
x64_transfer_kind=ret
x64_transfer_failure=translation-failed
```

resolver의 `return 0`, unresolved thunk의 `INT3`/`UD2`, 원본 guest bytes,
guest register/stack, AOT policy는 변경하지 않았습니다.

## 변경 사항

* `include/repiu/engine/linux_x64_transfer_failure_provenance.h`와
  telemetry 구현을 추가했습니다.
* `ThreadContext`와 `RecordFaultExit`에 provenance를 연결했습니다.
* invalid, zero-target RET, indirect-call, failure-name 변환을 검사하는
  core probe를 추가했습니다.
* CMake, core probe 등록, `ARCHITECTURE.md`, Linux frontier 분석,
  설계·작업 지시서를 갱신했습니다.

## 검증

* `git diff --check`: 기존 CRLF 파일의 줄바꿈 경고 외 오류 없이
  통과했습니다.
* WSL Ubuntu-24.04에서 `CMAKE_BUILD_PARALLEL_LEVEL=2`로 Linux x64 Debug
  headless `repiu_core_probe` 및 `repiu` 빌드를 성공했습니다.
* 전체 probe 결과는 `core_probe_total=27`, `core_probe_failures=0`,
  `core_probe_all=true`였습니다.
* 실제 `pumpit2a` 실행에서 resolver가
  `source=0x00000000`, `producer=0x010F1E56`,
  `guest_esp=0x0158CC5C`, `translation-failed`를 보고했고, 두 최종
  `[repiu-exit]` 줄에 동일한 provenance가 출력되었습니다.
* 첫 fault는 unresolved thunk `INT3`인 `eip=0x402BD30D`, 두 번째 fault는
  뒤따른 `UD2`인 `eip=0x402BD30E`였습니다. 프로세스는 기존 fail-closed
  동작대로 illegal-instruction 종료를 보였습니다.

## 판단과 다음 단계

이번 작업으로 `0x010F1E56 RET`가 zero target을 선택했다는 사실은 최종
fault trace에서도 확인할 수 있게 되었습니다. 그러나 zero word를 기록한
stack writer나 zero target의 원인은 아직 미확정이며, 이를 근거 없이 수정하지
않았습니다. 다음 작업은 해당 RET 직전 stack writer와 guest ESP 변화를
분석하는 별도 작업으로 진행해야 합니다.

---

# Work Log 648: Linux x64 unresolved return provenance

## Result

Connected the unresolved x64 return boundary observed in Task 647's real run
to the final fault trace. `LinuxX64TransferFailureProvenance` retains the
producer EIP, target EIP, guest ESP after the transfer, transfer kind, and
failure reason for the latest failed x64 resolver call in a fixed-size
`ThreadContext` record.

Target `0` is retained as a valid observation rather than discarded as an
invalid state. The previous record is cleared at the next resolver entry, so a
successful later transfer cannot leave an old failure behind. With
`REPIU_FAULT_EXIT_TRACE=1`, the existing `[repiu-exit]` line gains:

```text
x64_transfer_valid=1
x64_transfer_producer_eip=0x010F1E56
x64_transfer_target_eip=0x00000000
x64_transfer_guest_esp=0x0158CC5C
x64_transfer_kind=ret
x64_transfer_failure=translation-failed
```

The resolver's `return 0`, the unresolved thunk's `INT3`/`UD2`, original guest
bytes, guest registers/stack, and AOT policy were not changed.

## Changes

* Added `include/repiu/engine/linux_x64_transfer_failure_provenance.h` and its
  telemetry implementation.
* Connected the provenance to `ThreadContext` and `RecordFaultExit`.
* Added a core probe for invalid state, zero-target RET, indirect-call, and
  failure-name conversion.
* Updated CMake, core-probe registration, `ARCHITECTURE.md`, the Linux frontier
  analysis, and the design/work-order documents.

## Verification

* `git diff --check` passed with no errors apart from line-ending warnings from
  existing CRLF files.
* Linux x64 Debug headless `repiu_core_probe` and `repiu` built successfully in
  WSL Ubuntu-24.04 with `CMAKE_BUILD_PARALLEL_LEVEL=2`.
* The full probe result was `core_probe_total=27`,
  `core_probe_failures=0`, and `core_probe_all=true`.
* The real `pumpit2a` run observed resolver
  `source=0x00000000`, `producer=0x010F1E56`,
  `guest_esp=0x0158CC5C`, and `translation-failed`; both final
  `[repiu-exit]` lines printed the same provenance.
* The first fault was the unresolved thunk `INT3` at `eip=0x402BD30D`; the
  second was the following `UD2` at `eip=0x402BD30E`. The process ended with
  the existing fail-closed illegal-instruction behavior.

## Finding and next step

The fact that the original `RET` at `0x010F1E56` selected a zero target is now
visible in the final fault trace. The stack writer that stored the zero word and
the cause of the zero target remain unresolved, and no unsupported correction
was applied. The next task should separately analyze the stack writer and
guest-ESP changes immediately before this RET.

---
