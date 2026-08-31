# 20260831-549 Linux x64 core probe 정지 지점 분리 작업 로그

## 한국어

### 결과

Task 547과 Task 548이 기록한 "`pit_timer`에서 멈춤"은 틀린 귀속이었습니다. 실제
정지 지점은 `fault_handler`이며, `pit_timer`는 두 host에서 모두 통과합니다.

원인은 `repiu_core_probe`가 `std::cout`에 개행만 쓰고 flush하지 않는 것이었습니다.
`wsl.exe`를 통해 실행하면 stdout이 pipe가 되어 block buffering 되고, 죽거나 강제
중단된 실행은 buffer에 남은 내용을 잃습니다. 마지막으로 보이는 줄은 멈춘 지점이
아니라 마지막으로 flush된 경계입니다. 수정 전 HEAD를 그대로 빌드해 실행하면 출력이
`dos_handle_cache_all=true`에서 끊기는데, 이것이 두 세션이 `pit_timer`로 읽은 바로
그 화면입니다.

정지 지점을 볼 수 있게 만든 뒤 세 개의 결함이 드러났고, 모두 수정했습니다.

1. **x86-64 register write-back이 host 상위 32비트를 지웠습니다.** x64
   `StoreGuestCpuContext`가 `machine.gregs[REG_RIP] = registers.Eip`로 대입하면
   bits 32..63이 0이 됩니다. 이 host에서 실행·fault가 일어나는 page는 4 GiB보다
   훨씬 위에 있으므로, signal resume이 매핑된 적 없는 주소로 돌아가 즉시 다시
   fault했고 그 상태가 계속됐습니다. 이제 low half만 쓰고 host의 upper half는
   보존합니다. Task 546의 "host RIP is not guest EIP"를 실제로 구현한 것입니다.
2. **breakpoint rewind가 잘린 Eip로 host memory를 읽었습니다.**
   `RewindPastBreakpoint`가 `registers->Eip - 1`을 역참조하는데, 이 값은 x64에서
   RIP의 low half입니다. 이제 host context의 RIP를 직접 읽어 `0xCC` 바이트를
   찾습니다. i386에서는 두 값이 같으므로 동작이 바뀌지 않습니다.
3. **fault handler probe의 `stage` 저장이 -O2에서 제거됐습니다.** `stage`는 signal
   handler에게 건네지는 값인데 ordinary global이었습니다. 두 store 사이에 있는
   것은 *다른* 객체에 대한 `volatile` load뿐이라 아무 순서도 강제하지 않고, 그
   사이에서 `stage`를 읽는 코드가 없으므로 첫 store는 dead store입니다. GCC가 이를
   삭제했고, handler는 `kIdle` 상태로 실행되어 `default` 분기에서 `kNotHandled`를
   답했으며, fault layer는 규정대로 SIG_DFL을 복원했고 재시도된 read가 프로세스를
   죽였습니다. 이제 `stage`는 volatile lvalue로 저장·조회합니다.

세 번째는 Linux i386 Release에만 나타나던 회귀였습니다. x64 tree는 Debug라 같은
소스가 통과했으므로, i386과 x64의 차이가 아니라 Release와 Debug의 차이였습니다.

또한 x64 빌드 절차가 스크립트로 없었으므로 `scripts/build_linux_x64.sh`를
추가했습니다.

### 검증

| Host | 결과 |
|---|---|
| Linux x64 Debug | `core_probe_all=true`, 14/14, skipped 2 (i386 assembly) |
| Linux i386 Release | `core_probe_all=true`, 15/15 |
| Win32 x86 Debug | `core_probe_all=true`, 15/15 (회귀) |

두 세션이 기다리던 값이 처음으로 측정됐습니다.

```text
guest_cpu_context_all=true
linux_x64_aot_frame_all=true
fault_handler_all=true
host_thread_all=true
```

x64에서 실제 fault를 받아 재개하고, planted `int3`에서 rewind하고, trap flag로 한
instruction을 single-step한 뒤 `returned=0x5a5a1234`까지 도달합니다.
`host_thread_interrupt`도 통과하므로 다른 thread의 register를 읽고 편집해 되쓰는
경로도 x64에서 동작합니다.

## English

### Result

The "stopped at `pit_timer`" recorded by Tasks 547 and 548 was a wrong attribution. The
real stopping point is `fault_handler`; `pit_timer` passes on both hosts.

The cause is that `repiu_core_probe` writes newlines to `std::cout` and never flushes.
Run through `wsl.exe`, stdout is a pipe and buffers in whole blocks, so a run that dies
or is interrupted loses whatever the buffer still held. The last visible line is the last
flushed boundary, not the stopping point. Building HEAD unmodified and running it shows
output ending at `dos_handle_cache_all=true` -- which is exactly the screen both sessions
read as `pit_timer`.

Making the stopping point visible exposed three defects, all fixed here.

1. **The x86-64 register write-back zeroed the host's upper 32 bits.** Assigning
   `registers.Eip` to `machine.gregs[REG_RIP]` clears bits 32..63, and on this host the
   pages the process executes on and faults in sit far above 4 GiB. A signal resume
   therefore returned to an address that had never been mapped, refaulted at once, and
   kept doing so. The adapter now writes the low half and preserves the host's upper
   half, which is Task 546's "host RIP is not guest EIP" made concrete.
2. **The breakpoint rewind read host memory through a truncated Eip.**
   `RewindPastBreakpoint` dereferenced `registers->Eip - 1`, which on x64 is the low half
   of RIP. It now finds the `0xCC` byte through the host context's own RIP. On i386 the
   two are the same number, so behaviour there is unchanged.
3. **The fault-handler probe's `stage` store was deleted at -O2.** `stage` is handed to a
   signal handler but was an ordinary global. What sits between its two stores is a
   `volatile` load of a *different* object, which orders nothing, and nothing the
   compiler can see reads `stage` in between -- so the first store is dead and deleting
   it is correct. GCC deleted it; the handler then ran with the stage still `kIdle`, took
   the `default` arm, answered `kNotHandled`, the fault layer restored SIG_DFL as it must,
   and the retried read killed the process. `stage` is now stored and read through a
   volatile lvalue.

The third was a regression visible only on Linux i386 Release. The x64 tree is a Debug
tree, so the same source passed there: the split was Release versus Debug, not i386
versus x64.

`scripts/build_linux_x64.sh` was added, because no scripted x64 build procedure existed.

### Verification

| Host | Result |
|---|---|
| Linux x64 Debug | `core_probe_all=true`, 14 of 14, 2 skipped (i386 assembly) |
| Linux i386 Release | `core_probe_all=true`, 15 of 15 |
| Win32 x86 Debug | `core_probe_all=true`, 15 of 15 (regression) |

The values both earlier sessions were waiting for are measured for the first time:

```text
guest_cpu_context_all=true
linux_x64_aot_frame_all=true
fault_handler_all=true
host_thread_all=true
```

On x64 the process now takes real faults and resumes from them, rewinds onto a planted
`int3`, single-steps one instruction under the trap flag, and reaches
`returned=0x5a5a1234`. `host_thread_interrupt` passes as well, so reading, editing, and
writing back another thread's registers works on x64 too.
