# 작업 로그 20260905-600 — Linux x64 guest `PUSH CS` HLE

설계: [20260905-600](../design/20260905-600-linux-x64-push-cs-hle.md)  
작업 지시: [20260905-600](../work-orders/20260905-600-linux-x64-push-cs-hle.md)

## 결과

기존 segment-push HLE를 `PUSH CS` (`0Eh`)까지 확장했습니다. CS 값은 고정하지
않고, 현재 linear EIP를 포함하는 present selector descriptor를 유일하게 찾아
결정합니다. 해당 descriptor가 없거나 겹치면 HLE는 실패합니다.

성공 시 기존 segment push와 같은 32-bit guest stack 동작을 적용합니다.

```text
ESP -= 4
[ESP] = zero_extend(current-CS-selector)
EIP += 1
```

selector table에 `FindSelectorForLinearAddress` 공용 조회를 추가했고, core probe는
PIU object-2 주소 `0x010F0117`이 selector `0x0024`를 선택하는 것, 범위 밖 주소와
겹친 descriptor를 거절하는 것을 검증했습니다.

```text
segment_push_code_selector=true,unique=true,absent=true,overlap_rejected=true
core_probe_total=21
core_probe_failures=0
core_probe_all=true
```

`REPIU_DPMI_1E7F_PROBE_SUCCESS=1` 실행에서 이전 `0x010F0117` (`0Eh`) SIGILL은
재현되지 않았습니다. guest는 후속 명령을 실행하여 새 frontier에 도달했습니다.

```text
[repiu-fault] unhandled signal=0x4 rip=0x10f016b eip=0x10f016b
bytes=66 ea 04 00 2c 00 8d 40 00 55 57 56 53 06 89 e5
```

`66 EA 04 00 2C 00`은 32-bit guest의 operand-size-override far jump이며,
selector `0x002C`, offset `0x0004`를 대상으로 합니다. long mode에서는 invalid이므로
다음 작업은 selector table을 이용한 far control-transfer HLE가 됩니다.

## 검증

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
repiu_core_probe: 21 total, 0 failures
```

## 후속 작업

`66 EA ptr16:16` guest far jump의 selector:offset을 selector table로 linear target으로
변환하고, 현재 AOT 재진입 규약에 맞춰 해당 target을 실행하도록 별도 설계합니다.
`INT 31h AX=1E7Fh`의 실제 성공 ABI는 계속 미확정이며 probe-success는 진단 전용입니다.

---

# Work log 20260905-600 — Linux x64 guest `PUSH CS` HLE

Design: [20260905-600](../design/20260905-600-linux-x64-push-cs-hle.md)  
Work order: [20260905-600](../work-orders/20260905-600-linux-x64-push-cs-hle.md)

## Result

Extended the existing segment-push HLE to guest `PUSH CS` (`0Eh`). Rather than
using a fixed selector, it uniquely finds the present selector-table descriptor
containing the current linear EIP. The HLE fails when that descriptor is absent
or overlapping.

On success it uses the existing 32-bit guest-stack behavior:

```text
ESP -= 4
[ESP] = zero_extend(current-CS-selector)
EIP += 1
```

Added the reusable selector-table query `FindSelectorForLinearAddress`. Its core
probe verifies that PIU object-2 address `0x010F0117` selects `0x0024`, and that
an absent address or overlapping descriptor is rejected.

```text
segment_push_code_selector=true,unique=true,absent=true,overlap_rejected=true
core_probe_total=21
core_probe_failures=0
core_probe_all=true
```

With `REPIU_DPMI_1E7F_PROBE_SUCCESS=1`, the former `0x010F0117` (`0Eh`) SIGILL
did not recur. The guest executed subsequent instructions and reached a new
frontier:

```text
[repiu-fault] unhandled signal=0x4 rip=0x10f016b eip=0x10f016b
bytes=66 ea 04 00 2c 00 8d 40 00 55 57 56 53 06 89 e5
```

`66 EA 04 00 2C 00` is the 32-bit guest operand-size-override far jump targeting
selector `0x002C`, offset `0x0004`. It is invalid in long mode, so the next unit
is a selector-table-based HLE for far control transfer.

## Verification

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
repiu_core_probe: 21 total, 0 failures
```

## Follow-up

Design a separate HLE for guest `66 EA ptr16:16`: translate its selector:offset
through the selector table and execute the resulting linear target under the
current AOT re-entry contract. The real success ABI of `INT 31h AX=1E7Fh`
remains unresolved; probe-success remains diagnostic only.
