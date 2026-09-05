# 작업 지시 20260905-604 — Linux x64 관찰된 far-return frame HLE

## 목표

관찰된 object 3 `66 CB` frame을 selector metadata로 검증하고, Linux x64에서
`ESP += 8`을 포함한 정확한 protected-mode far return HLE를 수행합니다.

## 구현 범위

1. `GuestDescriptor`와 relocated selector binding에 LE object flags 기반 code-mode
   metadata를 전달합니다.
2. `66 CB`를 현재 16-bit default executable code에서만 선택하는 HLE를 추가합니다.
3. stack의 offset dword와 padded CS dword를 검증하고 target selector/offset를
   selector table로 변환합니다.
4. 유효한 경우 `EIP`, `SegCs`, `ESP += 8`을 갱신하고, 실패 시 boundary를 유지합니다.
5. 순수 frame resolver probe와 runtime을 추가 검증합니다.
6. Task 603의 `ESP += 6` 미확정 기록을 새 Intel semantics와 관찰 결과로 보완합니다.

## 제외 범위

* 16-bit operand-size far return 일반화
* privilege transition/call-gate/outer-stack semantics
* 별도 SS B-bit descriptor 모델링
* `INT 31h AX=1E7Fh` ABI 변경
* `0xFF` 반환 source 보정

## 검증 절차

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
build/linux_x64_debug/repiu_core_probe
```

probe-success runtime 환경:

```text
REPIU_DOS_INT_TRACE=1
REPIU_DPMI_1E7F_TRACE=1
REPIU_DPMI_1E7F_PROBE_SUCCESS=1
REPIU_LINUX_X64_RETURN_TRACE=1
```

## 완료 기준

* frame probe가 `offset=0x010F0232`, `selector=0x0024`, `stack_bytes=8`을 확인합니다.
* 잘못된 target과 32-bit current code가 fail-closed로 거절됩니다.
* Linux x64 빌드와 core probe가 성공합니다.
* runtime이 `0x01100040` boundary를 통과해 다음 wrapper 경계로 진행합니다.

---

# Work order 20260905-604 — Linux x64 observed far-return frame HLE

## Goal

Validate the observed object 3 `66 CB` frame with selector metadata and execute
the protected-mode far return on Linux x64 with the correct `ESP += 8` effect.

## Scope

1. Carry LE object-flag code-mode metadata into guest descriptors and relocated
   selector bindings.
2. Add an HLE that selects `66 CB` only in current 16-bit-default executable code.
3. Validate the offset dword and padded CS dword, then translate the target
   selector/offset through the selector table. Permit the observed raw relocated
   linear frame representation only inside the target descriptor window.
4. On success update `EIP`, `SegCs`, and `ESP += 8`; on failure retain the boundary.
5. Add pure frame-resolution coverage and verify the runtime.
6. Supplement Task 603's unresolved `ESP += 6` note with the Intel semantics and
   observed frame evidence.

## Out of scope

* General 16-bit operand-size far returns
* Privilege transitions, call gates, or outer-stack semantics
* A separate SS B-bit descriptor model
* Changes to the private `INT 31h AX=1E7Fh` ABI
* Repairing the `0xFF` return source

## Verification

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
build/linux_x64_debug/repiu_core_probe
```

Probe-success runtime environment:

```text
REPIU_DOS_INT_TRACE=1
REPIU_DPMI_1E7F_TRACE=1
REPIU_DPMI_1E7F_PROBE_SUCCESS=1
REPIU_LINUX_X64_RETURN_TRACE=1
```

## Done criteria

* The frame probe confirms raw `offset=0x010F0232`, `selector=0x0024`, and
  `stack_bytes=8`.
* Invalid targets and a 32-bit-default current code are rejected fail-closed.
* Linux x64 build and core probe pass.
* Runtime crosses the `0x01100040` boundary and reaches the next wrapper boundary.

## 추가 판단

실행 진단에서 raw frame offset `0x010F0232`는 일반적인 selector-relative
offset으로는 `0x0024` descriptor의 limit을 벗어나지만, relocated target의
linear window 안에 있음이 확인되었습니다. 따라서 resolver는 selector-relative
해석을 먼저 시도하고, target descriptor window 안에 있는 경우에만 관찰된
absolute-linear 표현을 제한적으로 허용합니다. 이 판단으로 `66 CB`는 8바이트
frame을 소비하여 `0x010F0232`로 이동하며, 이후 기존 frontier에서 정지합니다.

## Additional judgment

Runtime diagnosis showed that raw frame offset `0x010F0232` is outside the
`0x0024` descriptor limit when interpreted as a selector-relative offset, but is
inside the relocated target's linear window. The resolver therefore tries the
selector-relative form first and permits the observed absolute-linear form only
when it is bounded by the target descriptor. With this judgment, `66 CB` consumes
the 8-byte frame and transfers to `0x010F0232`; execution then stops at the
existing frontier.
