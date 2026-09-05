# 작업 로그 20260905-599 — Linux x64 DPMI `1E7Fh` 계약 탐침

설계: [20260905-599](../design/20260905-599-linux-x64-dpmi-1e7f-contract-probe.md)  
작업 지시: [20260905-599](../work-orders/20260905-599-linux-x64-dpmi-1e7f-contract-probe.md)

## 결과

`REPIU_DPMI_1E7F_TRACE=1`은 호출 직전의 완전한 guest register snapshot을
기록합니다. `REPIU_DPMI_1E7F_PROBE_SUCCESS=1`은 진단 전용으로 CF만 clear하고
EAX를 포함한 범용 레지스터를 보존합니다. 두 변수 모두 기본 비활성화이며,
기본 `AX=8001h`, CF=1 동작은 변하지 않았습니다.

기본 trace에서 확인한 입력은 다음과 같습니다.

```text
[repiu-dpmi-1e7f] eip=0x010F010C eax=0x00001E7F ebx=0x011A7AEC
ecx=0x00000000 edx=0x00000000 esi=0x011A7B28 edi=0x011A7B28
esp=0x0158CC5C eflags=0x00200396 probe-success=0
```

기본 경로는 `AX=8001h`, CF=1 뒤 `JB 0x010F022C`로 이동하고, 기존의
`MOV byte ptr [EBX], 2`, `EBX=0` 널 쓰기를 재현했습니다.

probe-success는 이 오류 분기를 통과했으나, 즉시 다음 독립 frontier에
도달했습니다.

```text
[repiu-dpmi-1e7f] ... probe-success=1
[repiu-fault] unhandled signal=0x4 rip=0x10f0117 eip=0x10f0117
bytes=0e 68 32 02 0f 01 89 e5 ...
```

첫 바이트 `0Eh`는 32-bit protected mode에서는 유효하지만 x86-64 long mode에서는
유효하지 않은 `PUSH CS`입니다. 따라서 성공 분기를 계속 관측하려면 다음 작업에서
guest stack에 대한 `PUSH CS`의 32-bit 의미를 HLE해야 합니다.

`1E7Fh`의 실제 사설 서비스 계약, 특히 성공 시 반환 레지스터 또는 메모리 효과는
여전히 **미확정**입니다. 이번 probe 결과는 CF=0이 성공 경로를 선택한다는 사실만
보이며, 이를 최종 서비스 구현으로 채택하지 않습니다.

## 검증

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
core_probe_total=20
core_probe_failures=0
core_probe_all=true
```

## 후속 작업

`PUSH CS` HLE를 별도 작업으로 설계하여, 현재 guest `CS` selector를 32-bit guest
stack에 push하고 다음 명령 경계를 관측합니다. `1E7Fh` probe는 실제 ABI가 확인될
때까지 opt-in 진단 기능으로 유지합니다.

---

# Work log 20260905-599 — Linux x64 DPMI `1E7Fh` contract probe

Design: [20260905-599](../design/20260905-599-linux-x64-dpmi-1e7f-contract-probe.md)  
Work order: [20260905-599](../work-orders/20260905-599-linux-x64-dpmi-1e7f-contract-probe.md)

## Result

`REPIU_DPMI_1E7F_TRACE=1` records the complete guest register snapshot before
the call. `REPIU_DPMI_1E7F_PROBE_SUCCESS=1` is diagnostic-only: it clears only
CF and preserves general registers including EAX. Both settings are disabled by
default, and the default `AX=8001h`, CF-set behavior is unchanged.

The default trace established these inputs:

```text
[repiu-dpmi-1e7f] eip=0x010F010C eax=0x00001E7F ebx=0x011A7AEC
ecx=0x00000000 edx=0x00000000 esi=0x011A7B28 edi=0x011A7B28
esp=0x0158CC5C eflags=0x00200396 probe-success=0
```

The default path retained `AX=8001h`, CF=1, took `JB 0x010F022C`, and
reproduced the existing `MOV byte ptr [EBX], 2`, `EBX=0` null write.

Probe-success bypassed that error branch but immediately reached a distinct
frontier:

```text
[repiu-dpmi-1e7f] ... probe-success=1
[repiu-fault] unhandled signal=0x4 rip=0x10f0117 eip=0x10f0117
bytes=0e 68 32 02 0f 01 89 e5 ...
```

The leading `0Eh` is `PUSH CS`, valid in 32-bit protected mode but invalid in
x86-64 long mode. Continuing the success-path observation therefore requires a
separate HLE for the 32-bit guest-stack semantics of `PUSH CS`.

The actual private-service contract of `1E7Fh`, including successful output
registers or memory effects, remains **unresolved**. This probe only proves that
CF=0 selects the success path; it is not adopted as the service implementation.

## Verification

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
core_probe_total=20
core_probe_failures=0
core_probe_all=true
```

## Follow-up

Design `PUSH CS` HLE as a separate unit, push the current guest CS selector to
the 32-bit guest stack, and observe the next instruction boundary. Keep the
`1E7Fh` probe opt-in until the real ABI is established.
