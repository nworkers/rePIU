# 작업 로그 20260913-663 — Linux x64 LINEXE far-transfer 프레임 추적

## 한국어

### 작업 개요

Task 663 설계에 따라 LINEXE far-transfer boundary와 `66 EA` far-jump HLE에
선택적 bounded trace를 추가했습니다.

- 설계: [20260913-663-linux-x64-linexe-far-frame-trace.md](../design/20260913-663-linux-x64-linexe-far-frame-trace.md)
- 작업 지시: [20260913-663-linux-x64-linexe-far-frame-trace.md](../work-orders/20260913-663-linux-x64-linexe-far-frame-trace.md)
- 누적 frontier: [linux-port-frontier.md](../analysis/linux-port-frontier.md#3100-task-663--linux-x64-linexe-far-transfer-frame-trace)

### 구현 내용

`REPIU_LINEXE_FAR_TRANSFER_TRACE=1`을 추가했습니다. `HandleLinexeFarTransferBoundary`에서는
실제 `FF 1D` indirect transfer 또는 정확한 `66 EA 04 00 2C 00` direct transfer 후보에
대해서만 최대 64개의 진입·스택 window·서비스 decode·frame return 기록을 남깁니다.
`HandleFarJumpInstruction`에도 같은 환경 변수로 최대 64개의 입력 ESP, selector/offset,
translated target 기록을 추가했습니다. 기본 실행에서는 환경 변수가 없거나 `0`일 때
기존 출력과 guest state를 유지합니다.

일반 fault가 trace capacity를 먼저 소비하지 않도록 후보 opcode를 확인한 뒤 sequence를
할당하도록 제한했습니다. stack window는 `IsGuestRangeReadable` 확인 후에만 읽습니다.

### 검증 결과

Linux x64 Debug `repiu` 전체 링크가 성공했습니다.

```text
cmake --build build/linux_x64_debug --config Debug --target repiu -j2
[100%] Built target repiu
```

`repiu_core_probe`도 통과했습니다.

```text
core_probe_total=27
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
```

다음 bounded runtime에 새 trace와 기존 provenance trace를 함께 켰습니다.

```text
REPIU_LINEXE_FAR_TRANSFER_TRACE=1
REPIU_LINUX_X64_GUEST_ENTRY_TRACE=0x010F0232
REPIU_LINUX_X64_RETURN_FRAME_TRACE=1
```

결과에는 `[repiu-linexe-far]` 또는 `[repiu-linexe-far-jump]`가 없었고, 다음 기존
경로만 관찰되었습니다.

```text
[repiu-x64-guest-entry] n=1 target=0x010F0232 fault_kind=breakpoint fault_eip=0x200018E7 entry_eip=0x200018E7 entry_guest=0x010F0232 exit_eip=0x20001917 exit_guest=0x010F0237 entry_esp=0x0158C84C exit_esp=0x0158C860 eflags=0x00200246 pending=0 legacy=0 exit_site=step-trace-hle-resumed
[repiu-x64-return-frame] n=1 source=0x00000000 guest_eip=0x00000000 guest_esp=0x0158C864 status=0x010F0237 producer=ret
[repiu-fault] unhandled signal=0x5
```

### 결론과 다음 frontier

이번 bounded failure 경로는 `HandleLinexeFarTransferBoundary` 또는
`HandleFarJumpInstruction`을 통과하지 않았습니다. 따라서 Task 661의 4바이트 ESP delta를
LINEXE service frame cleanup이나 `66 EA` far-jump HLE의 직접 원인으로 귀속할 근거는
확보되지 않았습니다.

확인된 것은 `0x010F0232`가 AOT cache breakpoint/HLE reentry로 관찰되고
`0x010F0237 RET`까지 처리된다는 점입니다. zero-return frame과 fail-closed `SIGTRAP`은
그대로 유지됩니다. 다음 작업은 AOT/HLE 재진입 전후의 guest ESP를 직접 기록하여,
`PUSH ES` 이후의 4바이트 차이가 HLE frame 복구 또는 return resolver 경계에서 생기는지
분리해야 합니다.

### 작업 상태

- 구현: 완료
- 빌드: 통과
- 코어 프로브: `27/27`, 실패 `0`
- LINEXE/far-jump trace: 해당 bounded failure 경로에서 미관찰
- 4-byte ESP delta 원인: 미확정
- 정상 게임 실행: 미확정

## English

### Summary

Following the Task 663 design, this task added bounded opt-in traces to the
LINEXE far-transfer boundary and the `66 EA` far-jump HLE.

- Design: [20260913-663-linux-x64-linexe-far-frame-trace.md](../design/20260913-663-linux-x64-linexe-far-frame-trace.md)
- Work order: [20260913-663-linux-x64-linexe-far-frame-trace.md](../work-orders/20260913-663-linux-x64-linexe-far-frame-trace.md)
- Cumulative frontier: [linux-port-frontier.md](../analysis/linux-port-frontier.md#3100-task-663--linux-x64-linexe-far-transfer-frame-trace)

### Implementation

Added `REPIU_LINEXE_FAR_TRANSFER_TRACE=1`. `HandleLinexeFarTransferBoundary`
now records at most 64 entries, stack windows, service decoding, and frame
returns, but only after recognizing an actual `FF 1D` indirect transfer or the
exact `66 EA 04 00 2C 00` direct-transfer candidate. `HandleFarJumpInstruction`
records input ESP, selector/offset, and the translated target under the same
environment variable. With no variable, or with value `0`, default output and
guest state are unchanged.

The trace sequence is allocated only after candidate-opcode filtering so
ordinary faults cannot exhaust the bounded capture. The stack window is read
only after `IsGuestRangeReadable` succeeds.

### Verification

The Linux x64 Debug `repiu` target linked successfully:

```text
cmake --build build/linux_x64_debug --config Debug --target repiu -j2
[100%] Built target repiu
```

`repiu_core_probe` also passed:

```text
core_probe_total=27
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
```

A bounded runtime enabled the new trace together with the existing provenance
traces:

```text
REPIU_LINEXE_FAR_TRANSFER_TRACE=1
REPIU_LINUX_X64_GUEST_ENTRY_TRACE=0x010F0232
REPIU_LINUX_X64_RETURN_FRAME_TRACE=1
```

The output contained no `[repiu-linexe-far]` or
`[repiu-linexe-far-jump]` records. It only reproduced the existing path:

```text
[repiu-x64-guest-entry] n=1 target=0x010F0232 fault_kind=breakpoint fault_eip=0x200018E7 entry_eip=0x200018E7 entry_guest=0x010F0232 exit_eip=0x20001917 exit_guest=0x010F0237 entry_esp=0x0158C84C exit_esp=0x0158C860 eflags=0x00200246 pending=0 legacy=0 exit_site=step-trace-hle-resumed
[repiu-x64-return-frame] n=1 source=0x00000000 guest_eip=0x00000000 guest_esp=0x0158C864 status=0x010F0237 producer=ret
[repiu-fault] unhandled signal=0x5
```

### Conclusion and next frontier

The bounded failure path did not pass through `HandleLinexeFarTransferBoundary`
or `HandleFarJumpInstruction`. Task 661's four-byte ESP delta therefore cannot
be attributed directly to LINEXE service-frame cleanup or `66 EA` far-jump HLE
from this evidence.

The confirmed path is still the AOT cache-breakpoint/HLE reentry at
`0x010F0232`, followed by handling through `0x010F0237 RET`. The zero-return
frame and fail-closed `SIGTRAP` remain unchanged. The next task must capture
guest ESP immediately before and after AOT/HLE reentry to separate a delta in
HLE frame restoration from one in the return resolver boundary.

### Status

- Implementation: complete
- Build: passed
- Core probe: `27/27`, zero failures
- LINEXE/far-jump trace: not observed on the bounded failure path
- Four-byte ESP delta cause: unresolved
- Normal game execution: unresolved
