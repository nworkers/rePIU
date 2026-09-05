# 작업 로그 20260905-597 — Linux x64 unhandled AOT fault opcode 진단

설계: [20260905-597](../design/20260905-597-linux-x64-unhandled-aot-opcode.md)  
작업 지시: [20260905-597](../work-orders/20260905-597-linux-x64-unhandled-aot-opcode.md)

## 결과

Linux x64 `fault_handler`에 faulting host RIP의 opcode bytes와 guest stack
window를 추가했습니다. `REPIU_AOT_FAULT_TRACE=1`은 faulting cache 주소의
reverse map을 확인하되, 반복 fault 시 최초 16건만 출력하도록 제한했습니다.

재빌드 명령:

```text
wsl.exe -d Ubuntu-24.04 -- bash -lc "cd /mnt/e/MYWORK/Projects/rePIU && cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2"
```

빌드 결과는 exit code `0`이며 `repiu`, `repiu_core_probe` 모두 생성됐습니다.

## Core probe 검증

```text
core_probe_total=20
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
core_probe_host=x64 (Task 545: i386 assembly probes are not built)
```

Linux x64 guest register, stack/pop, segment load/pop, long-mode emission과
fault-handler probe가 모두 통과했습니다.

## 실제 실행

실행 명령:

```text
wsl.exe -d Ubuntu-24.04 -- bash -lc "cd /mnt/e/MYWORK/Projects/rePIU && REPIU_GUEST_WATCH=0x010F4AD2 REPIU_DOS_INT_TRACE=1 timeout -k 1s 5s ./build/linux_x64_debug/repiu pumpit2a"
```

프로세스는 기존과 같은 unhandled `SIGSEGV`로 종료되어 외부 실행 결과는
exit code `1`입니다. 다만 fault는 다음처럼 정확히 분류됐습니다.

```text
[repiu-dos-int] #3 int=31 ax=1E7F
[repiu-guest-int3] #1 eip=0x010F022C eax=0x00008001 ...
[repiu-watch] event=fault guest=0x010F4AD2 n=1 at=0x2004FB6B ... ebx=0x00000000 ...
[repiu-fault] unhandled signal=0xb rip=0x2004fb6b eip=0x2004fb6b access=0x0 bytes=67 c6 03 02 e9 00 00 00 00 3e be b6 7a 1a 01 3e guest_stack_m8=0x10f4ad1 guest_stack_m4=0xff guest_stack_0=0x0 guest_stack_p4=0x138007c ...
```

`0x2004FB6B`는 registered AOT map 안의 guest `0x010F4AD2`로 역매핑됩니다.
앞선 guest 명령 `0x010F4AD1`은 `POP EDX`이며 `guest_stack_m4=0xFF`와
fault context `EDX=0xFF`가 일치합니다. fault bytes `67 C6 03 02`는
`MOV byte ptr [EBX],02h`이므로 `EBX=0`과 `si_addr=0`은 null write로
일치합니다.

따라서 이번 실행에서 확인된 것은 AOT map 단절이나 raw guest 재개가 아니라,
등록된 AOT translation이 guest 명령을 실행한 뒤 guest `EBX=0` 주소에 쓰려다
fault한 것입니다. `EBX=0`을 만든 upstream 경로, 특히 앞선 `POP EBX`의 실제
입력값은 이 fault 시점 window만으로 확정하지 않았습니다. 원본 guest bytes를
수정하거나 null write를 무시하는 변경은 적용하지 않았습니다.

## 후속 작업

guest `0x010F0233`의 실제 `POP EBX` 입력과 직후 register state를 별도 watch/
probe로 관측하여, 원본 stack data와 segment-pop/stack-transition 상태 손실을
구분해야 합니다.

---

# Work log 20260905-597 — Linux x64 unhandled AOT fault opcode diagnostics

Design: [20260905-597](../design/20260905-597-linux-x64-unhandled-aot-opcode.md)  
Work order: [20260905-597](../work-orders/20260905-597-linux-x64-unhandled-aot-opcode.md)

## Result

The Linux x64 `fault_handler` now reports opcode bytes at the faulting host RIP
and a guest stack window. `REPIU_AOT_FAULT_TRACE=1` reports reverse-map ownership
for the faulting cache address and is capped at the first 16 faults per process
to prevent repeated faults from flooding the log.

Rebuild command:

```text
wsl.exe -d Ubuntu-24.04 -- bash -lc "cd /mnt/e/MYWORK/Projects/rePIU && cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2"
```

The build completed with exit code `0` and produced both `repiu` and
`repiu_core_probe`.

## Core probe verification

```text
core_probe_total=20
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
core_probe_host=x64 (Task 545: i386 assembly probes are not built)
```

All Linux x64 guest-register, stack/pop, segment-load/pop, long-mode emission,
and fault-handler probes passed.

## Runtime execution

The rebuilt runtime was launched with:

```text
wsl.exe -d Ubuntu-24.04 -- bash -lc "cd /mnt/e/MYWORK/Projects/rePIU && REPIU_GUEST_WATCH=0x010F4AD2 REPIU_DOS_INT_TRACE=1 timeout -k 1s 5s ./build/linux_x64_debug/repiu pumpit2a"
```

The process still exits at the same unhandled `SIGSEGV`, so the external result
is exit code `1`. The fault is now classified precisely:

```text
[repiu-dos-int] #3 int=31 ax=1E7F
[repiu-guest-int3] #1 eip=0x010F022C eax=0x00008001 ...
[repiu-watch] event=fault guest=0x010F4AD2 n=1 at=0x2004FB6B ... ebx=0x00000000 ...
[repiu-fault] unhandled signal=0xb rip=0x2004fb6b eip=0x2004fb6b access=0x0 bytes=67 c6 03 02 e9 00 00 00 00 3e be b6 7a 1a 01 3e guest_stack_m8=0x10f4ad1 guest_stack_m4=0xff guest_stack_0=0x0 guest_stack_p4=0x138007c ...
```

`0x2004FB6B` reverse-maps through the registered AOT map to guest
`0x010F4AD2`. The preceding guest instruction `0x010F4AD1` is `POP EDX`, and
`guest_stack_m4=0xFF` agrees with fault-context `EDX=0xFF`. The fault bytes
`67 C6 03 02` decode as `MOV byte ptr [EBX],02h`, so `EBX=0` and `si_addr=0`
are consistent with a null write.

The run therefore shows neither an AOT map gap nor raw guest reentry. A
registered AOT translation executed the guest instruction and faulted while
writing through guest `EBX=0`. The upstream path that produced `EBX=0`, including
the exact input to the earlier `POP EBX`, is not proven by this fault-time window.
No change was made to original guest bytes, and the null write was not swallowed.

## Follow-up

Observe the actual input and immediate register state at guest `0x010F0233`'s
`POP EBX` with a dedicated watch/probe, separating original stack data from any
state loss in the segment-pop or stack-transition path.
