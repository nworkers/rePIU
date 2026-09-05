# 작업 로그: Linux x64 mixed-mode far return frame

## 한국어

### 목표

Task 603에서 확인한 guest `66 CB` mixed-mode far return 경계를 원본 guest
코드를 수정하지 않고 Linux x64 fault HLE 경로에서 처리합니다.

### 변경 사항

* LE object flags를 relocated selector binding과 `GuestDescriptor`에 전달했습니다.
* 32-bit operand-size far return의 8바이트 frame resolver를 추가했습니다.
* selector-relative target을 먼저 해석하고, 관찰된 relocated-linear target은
  target descriptor의 mapped linear window 안에서만 제한적으로 허용했습니다.
* shared/fault HLE 경로에 `66 CB` handler를 연결했습니다.
* 정상 frame, 상대 offset, 비실행 target, limit 초과, current 32-bit default를
  검증하는 core probe를 추가했습니다.

### 진단 결과

첫 fault-path 실행은 frame의 raw target `0x010F0232`를 selector-relative
offset으로 해석할 때 `0x0024` descriptor limit을 벗어나므로 거부했습니다.
descriptor metadata를 확인한 결과 target base는 `0x01010000`, limit은
`0x000EBBDF`였고, raw 값은 relocated linear window 안에 있었습니다. bounded
absolute-linear fallback을 추가한 뒤 다음 결과를 얻었습니다.

```text
[repiu-far-return] stage=resolved eip=0x010F0232 esp=0x0158CC5C
```

기존 ESP `0x0158CC54` 대비 `+8`이므로 frame 소비가 확인되었습니다. 이후
실행은 다음 기존 frontier에서 정지했습니다.

```text
cache=0x2004FB6B
guest=0x010F4AD2
bytes=67 C6 03 02
ebx=0
```

이는 `MOV byte ptr [EBX], 2`이며, 이번 작업에서는 null 주소 write를 삼키거나
원본 instruction을 수정하지 않았습니다.

### 검증

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
exit_code=0

far_return_all=true
core_probe_total=23
core_probe_failures=0
core_probe_all=true
```

`INT 31h AX=1E7Fh`, generic near `RET`, 그리고 AOT-DBT superblock HLE 경로는
변경하지 않았습니다. 다음 작업의 frontier는 `0x010F4AD2`에서의 `EBX=0`
write semantics이며, `1E7Fh` private success ABI는 계속 미해결입니다.

## English

### Objective

Handle the guest `66 CB` mixed-mode far-return boundary identified in Task 603
through the Linux x64 fault HLE path without modifying original guest code.

### Changes

* Carried LE object flags into relocated selector bindings and `GuestDescriptor`.
* Added an 8-byte frame resolver for 32-bit operand-size far returns.
* Tried selector-relative targets first and allowed the observed relocated-linear
  target only within the target descriptor's mapped linear window.
* Connected the `66 CB` handler to the shared and fault HLE paths.
* Added core-probe coverage for valid frames, relative offsets, non-executable
  targets, limit overflow, and a 32-bit-default current code.

### Diagnosis

The first fault-path run rejected raw target `0x010F0232` because it exceeded the
`0x0024` descriptor limit as a selector-relative offset. Descriptor metadata
showed target base `0x01010000` and limit `0x000EBBDF`; the raw value is inside
the relocated linear window. After adding the bounded absolute-linear fallback,
runtime reported:

```text
[repiu-far-return] stage=resolved eip=0x010F0232 esp=0x0158CC5C
```

The previous ESP was `0x0158CC54`, confirming the expected `+8` frame
consumption. Execution then stopped at the existing frontier:

```text
cache=0x2004FB6B
guest=0x010F4AD2
bytes=67 C6 03 02
ebx=0
```

This is `MOV byte ptr [EBX], 2`. The task does not swallow the null write or
modify the original instruction.

### Verification

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
exit_code=0

far_return_all=true
core_probe_total=23
core_probe_failures=0
core_probe_all=true
```

`INT 31h AX=1E7Fh`, generic near `RET`, and the AOT-DBT superblock HLE path were
left unchanged. The next frontier is the `EBX=0` write semantics at
`0x010F4AD2`; the private `1E7Fh` success ABI remains unresolved.
