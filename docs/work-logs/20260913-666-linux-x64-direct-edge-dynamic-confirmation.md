# Task 666 작업 로그: Linux x64 direct-edge 동적 실행 확인

## 한국어

### 연결 문서

- 설계: [20260913-666 설계](../design/20260913-666-linux-x64-direct-edge-dynamic-confirmation.md)
- 작업 지시: [20260913-666 작업 지시](../work-orders/20260913-666-linux-x64-direct-edge-dynamic-confirmation.md)
- 누적 분석: [Linux port frontier](../analysis/linux-port-frontier.md#3103-task-666--linux-x64-direct-edge-dynamic-execution-confirmation)

### 실행

기존 Linux x64 Debug binary에 source execution sentinel, HLE re-entry trace,
return-frame trace를 함께 적용하고 25초 bounded 실행을 수행했습니다.

```text
REPIU_EXECUTION_TRACE_START=0x000EFF2A
REPIU_EXECUTION_TRACE_END=0x000EFF2A
REPIU_EXECUTION_TRACE_LOG=1
REPIU_GUEST_WATCH=0x010EFF2A
REPIU_AOT_HLE_REENTRY_TRACE=0x010F0232
REPIU_LINUX_X64_RETURN_FRAME_TRACE=1
REPIU_GLIDE_SWAP_INTERVAL=0
timeout -k 1s 25s .../repiu pumpit2a
```

### 핵심 결과

source 후보가 동적으로 실행된 기록:

```text
[repiu-watch] event=fault guest=0x010EFF2A n=1 at=0x20001935 esi=0x00000001 esp=0x0158C84C ebx=0x0158C92C eflags=0x00200246
```

같은 실행에서 target HLE re-entry와 zero-return이 이어졌습니다.

```text
[repiu-hle-reentry] stage=hle-before ... current=0x010F0232 ... guest_esp=0x0158C84C ...
[repiu-hle-reentry] stage=hle-after ... current=0x010F0237 ... guest_esp=0x0158C860 ...
[repiu-hle-reentry] stage=resumed ... cache_target=0x20001917 guest_esp=0x0158C860 ...
[repiu-x64-return-frame] n=1 source=0x00000000 guest_eip=0x00000000 guest_esp=0x0158C864 ... status=0x010F0237 ... m4=0x00000000 p0=0x00000001 p4=0x00000001 ... producer=ret producer_site=0x010F0237 ...
[repiu-fault] unhandled signal=0x5 ... guest_stack_m4=0x0 guest_stack_0=0x1 ... esp=0x158c864 ...
```

### 판정

| 항목 | 상태 |
|---|---|
| 정적 `0x010EFF2A -> 0x010F0232` 후보 | **확인됨** |
| source candidate의 동적 도달 | **확인됨**: cache `0x20001935` breakpoint |
| HLE 전 ESP | **확인됨**: `0x0158C84C` |
| HLE 후 ESP | **확인됨**: `0x0158C860` |
| epilogue `+0x14` | **정상 원본 동작과 일치**: 5개의 pop |
| `RET` 반환 슬롯 | **확인됨**: zero |
| SIGTRAP 성격 | **확인됨**: 기존 x64 return fail-closed 경계 |
| zero-return의 upstream producer | **미확정** |

이번 작업에서는 코드나 guest semantics를 변경하지 않았습니다. 결과는
주소별 예외처리의 근거가 아니라, 일반적인 call/return frame 또는 반환 슬롯
producer를 조사해야 한다는 동적 provenance 증거입니다.

## English

### References

- Design: [Task 666 design](../design/20260913-666-linux-x64-direct-edge-dynamic-confirmation.md)
- Work order: [Task 666 work order](../work-orders/20260913-666-linux-x64-direct-edge-dynamic-confirmation.md)
- Cumulative analysis: [Linux port frontier](../analysis/linux-port-frontier.md#3103-task-666--linux-x64-direct-edge-dynamic-execution-confirmation)

### Run

The Linux x64 Debug binary was run for a bounded 25 seconds with the source
execution sentinel, HLE re-entry trace, and return-frame trace enabled.

### Key result

The static source candidate was dynamically reached:

```text
[repiu-watch] event=fault guest=0x010EFF2A n=1 at=0x20001935 esi=0x00000001 esp=0x0158C84C ebx=0x0158C92C eflags=0x00200246
```

The same run then reached the target HLE re-entry and the existing zero-return:

```text
[repiu-hle-reentry] stage=hle-before ... current=0x010F0232 ... guest_esp=0x0158C84C ...
[repiu-hle-reentry] stage=hle-after ... current=0x010F0237 ... guest_esp=0x0158C860 ...
[repiu-hle-reentry] stage=resumed ... cache_target=0x20001917 guest_esp=0x0158C860 ...
[repiu-x64-return-frame] n=1 source=0x00000000 guest_eip=0x00000000 guest_esp=0x0158C864 ... status=0x010F0237 ... m4=0x00000000 p0=0x00000001 p4=0x00000001 ... producer=ret producer_site=0x010F0237 ...
[repiu-fault] unhandled signal=0x5 ... guest_stack_m4=0x0 guest_stack_0=0x1 ... esp=0x158c864 ...
```

### Assessment

| Item | Status |
|---|---|
| Static `0x010EFF2A -> 0x010F0232` candidate | **Confirmed** |
| Dynamic arrival at source candidate | **Confirmed**: cache `0x20001935` breakpoint |
| ESP before HLE | **Confirmed**: `0x0158C84C` |
| ESP after HLE | **Confirmed**: `0x0158C860` |
| Epilogue `+0x14` | **Consistent with original semantics**: five pops |
| `RET` return slot | **Confirmed**: zero |
| SIGTRAP meaning | **Confirmed**: existing x64 return fail-closed boundary |
| Upstream zero-return producer | **Unresolved** |

No code or guest semantics changed in this task. The result is dynamic provenance
evidence for investigating a general call/return frame or return-slot producer,
not a basis for an address-specific exception.
