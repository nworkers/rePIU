# Task 623 작업 로그: Linux x64 HLE 경계 쓰기 provenance

## 한국어

### 변경 내용

* 설계 문서와 작업 지시서를 추가했다.
* `REPIU_GUEST_WATCH`가 가리키는 세그먼트 push HLE에서 opcode, selector,
  destination, value, ESP/EIP 전후를 출력하도록 진단을 추가했다.
* watch가 꺼진 일반 실행의 HLE semantics와 출력은 변경하지 않았다.

### 검증 결과

Linux x64 빌드가 성공했다.

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

core probe 결과:

```text
guest_ret_imm16=true adjustment=8
linux_x64_guest_register_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

`REPIU_GUEST_WATCH=0x011A643F`로 `pumpit2a`를 재현한 결과:

```text
[repiu-segment-hle-watch] eip=0x011A643F opcode=0x06 selector=0x0024 destination=0x0158CC44 value=0x00000024 esp=0x0158CC48->0x0158CC44 next_eip=0x011A6440 size=1
[repiu-fault] unhandled signal=0xb rip=0x11a6440 eip=0x11a6440 access=0x37016be9 bytes=00 00 f0 e3 6b 01 09 00 00 00 05 00 00 00 00 10 guest_stack_m8=0x128cc2c guest_stack_m4=0x4 guest_stack_0=0x24 guest_stack_p4=0x0 eax=0x37016be9 ebx=0x4 ecx=0x128cc2c edx=0x0 esi=0x1 edi=0x128cc2c esp=0x158cc44 eflags=0x210302
```

### 결론

`PUSH ES` HLE의 selector, stack write, ESP/EIP 전환이 32-bit guest
semantics와 일치함을 확인했다. fault는 HLE 수행 후 `0x011A6440`의
`00 00`에서 발생하므로 HLE를 수정하거나 fault를 무시할 근거는 없다.
다음 조사 대상은 `0x011A643A` 직전 EAX 값 `0x37016BE9`의 원본 guest
instruction/register provenance다.

## English

### Changes

* Added the design and work-order documents.
* Added diagnostic output for opcode, selector, destination, value, ESP before
  and after, and EIP before and after at the segment-push HLE selected by
  `REPIU_GUEST_WATCH`.
* Preserved HLE semantics and output when the watch is disabled.

### Verification

The Linux x64 build succeeded:

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

The core probe passed:

```text
guest_ret_imm16=true adjustment=8
linux_x64_guest_register_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

The `pumpit2a` reproduction with `REPIU_GUEST_WATCH=0x011A643F` reported:

```text
[repiu-segment-hle-watch] eip=0x011A643F opcode=0x06 selector=0x0024 destination=0x0158CC44 value=0x00000024 esp=0x0158CC48->0x0158CC44 next_eip=0x011A6440 size=1
[repiu-fault] unhandled signal=0xb rip=0x11a6440 eip=0x11a6440 access=0x37016be9 bytes=00 00 f0 e3 6b 01 09 00 00 00 05 00 00 00 00 10 guest_stack_m8=0x128cc2c guest_stack_m4=0x4 guest_stack_0=0x24 guest_stack_p4=0x0 eax=0x37016be9 ebx=0x4 ecx=0x128cc2c edx=0x0 esi=0x1 edi=0x128cc2c esp=0x158cc44 eflags=0x210302
```

### Conclusion

The selector, stack write, and ESP/EIP transitions of `PUSH ES` HLE match the
32-bit guest semantics. The fault occurs after HLE at the `00 00` bytes at
`0x011A6440`, so there is no basis to modify the HLE or suppress the fault.
The next investigation target is the original guest instruction/register
provenance of `EAX=0x37016BE9` before `0x011A643A`.
