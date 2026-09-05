# 작업 로그 20260905-598 — Linux x64 AOT `POP EBX` trace 출력

설계: [20260905-598](../design/20260905-598-linux-x64-pop-ebx-trace.md)  
작업 지시: [20260905-598](../work-orders/20260905-598-linux-x64-pop-ebx-trace.md)

## 결과

기존 execution trace capture가 terminal Linux x64 fault 이전에 보이도록
`REPIU_EXECUTION_TRACE_LOG=1` opt-in immediate stderr 출력을 추가했습니다.
설정하지 않은 기본 실행에서는 출력하지 않습니다.

Linux x64 build와 core probe는 성공했습니다.

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
exit_code=0
core_probe_total=20
core_probe_failures=0
core_probe_all=true
```

기본 실행(`REPIU_EXECUTION_TRACE_LOG` 미설정)은 `[repiu-exec-trace]` 없이
Task 597의 `0x010F4AD2` null write를 동일하게 재현했습니다.

두 sentinel 실행은 다음 capture를 남겼습니다.

```text
[repiu-exec-trace] #0 eip=0x010F0233 esp=0x0158CC60 stack=0x00000000 eax=0x8BADF00D ebx=0x011A7AEC edx=0x00000000 eflags=0x00200397
```

`0x010F0232` `POP ES`를 실행한 뒤 EIP가 `0x010F0233`이므로, `stack=0`은 아직
실행되지 않은 `POP EBX`의 실제 입력입니다. 따라서 EBX가 이후 zero가 되는 것은
guest stack data의 결과이며, Task 597의 `MOV [EBX],2` null write와 일치합니다.

두 번째 sentinel은 `POP EBX` 뒤 상태를 출력하지 못했습니다. 첫 capture 뒤 host
`0x402ACEB9`에서 `SIGTRAP` 후 `SIGILL` (`0F 0B`)이 발생했습니다. 두 번째 sentinel을
제거한 trace run에서는 original null-write frontier는 재현됐지만 capture는 나오지
않았습니다. 이는 dynamic AOT reentry/sentinel 진단의 제한으로 기록하며, 게임 실행
의미를 바꾸는 수정은 적용하지 않았습니다.

## 후속 작업

`AX=1E7F` DPMI request에 현재 반환하는 unsupported error가 guest error path를 만든
것인지, 아니면 별도 upstream writer가 zero stack word를 만든 것인지 원본 control-flow와
DOS4GW 확장 contract를 분석해야 합니다.

---

# Work log 20260905-598 — Linux x64 AOT `POP EBX` trace output

Design: [20260905-598](../design/20260905-598-linux-x64-pop-ebx-trace.md)  
Work order: [20260905-598](../work-orders/20260905-598-linux-x64-pop-ebx-trace.md)

## Result

Added opt-in immediate stderr output under `REPIU_EXECUTION_TRACE_LOG=1` so
existing execution-trace captures remain visible before a terminal Linux x64
fault. The setting is off by default.

The Linux x64 build and core probe succeeded:

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
exit_code=0
core_probe_total=20
core_probe_failures=0
core_probe_all=true
```

With `REPIU_EXECUTION_TRACE_LOG` unset, no `[repiu-exec-trace]` line was emitted
and the Task 597 null write at `0x010F4AD2` reproduced unchanged.

The two-sentinel run produced:

```text
[repiu-exec-trace] #0 eip=0x010F0233 esp=0x0158CC60 stack=0x00000000 eax=0x8BADF00D ebx=0x011A7AEC edx=0x00000000 eflags=0x00200397
```

Since `0x010F0232` `POP ES` has completed and EIP is `0x010F0233`, `stack=0`
is the actual not-yet-consumed input to `POP EBX`. EBX becoming zero is therefore
a result of guest stack data, consistent with Task 597's `MOV [EBX],2` null write.

The second sentinel did not print the after-`POP EBX` state. After the first
capture, host `0x402ACEB9` received `SIGTRAP` followed by `SIGILL` (`0F 0B`).
Removing the second sentinel reproduced the original null-write frontier but
did not capture a trace line. This is recorded as a dynamic-AOT reentry/sentinel
diagnostic limit; no change to game execution semantics was made.

## Follow-up

Determine whether the current unsupported error for DPMI `AX=1E7F` creates the
guest error path, or whether another upstream writer created the zero stack word,
by analyzing original control flow and the DOS4GW extension contract.
