# 20260909-649 작업 로그: Linux x64 zero return slot writer provenance

설계: [20260909-649 설계](../design/20260909-649-linux-x64-zero-return-slot-writer.md)  
작업 지시: [20260909-649 작업 지시](../work-orders/20260909-649-linux-x64-zero-return-slot-writer.md)  
분석: [linux-port-frontier 3.86](../analysis/linux-port-frontier.md)

## 한국어

### 결과

HLE guest write trace가 현재 `GuestCpuContext`를 선택적으로 받도록 연결했습니다.
현재 guest context를 가진 `WriteGuestUInt8/16/32` 호출은 쓰기 직전의 guest EIP를
`execution/source`로 기록하고, EAX부터 EFLAGS까지의 레지스터를 함께 bounded ring에
남깁니다. context가 없는 host utility 호출은 기존 zero provenance를 유지합니다.

실제 `pumpit2a` 실행에서 `REPIU_GUEST_WRITE_TRACE=0x0158CC58`를 사용한 결과,
zero dword writer가 다음과 같이 특정되었습니다.

```text
[repiu-guest-write-trace-tail] event=hle n=0x0000002A watch=0x0158CC58 execution=0x010F9212 source=0x010F9212 destination=0x0158CC58 size=0x00000004 bytes=00000000 eax=0x010F920C ebx=0x011A7B16 ecx=0x00000000 edx=0x000000FF esi=0x011A7B28 edi=0x00000000 esp=0x0158CC5C eflags=0x00200306
```

`0x010F9212`는 기존 segment trace와 원본 연속 명령 분석에서 확인된 `PUSH FS`입니다.
segment HLE는 `context->guest_fs`를 `[ESP-4]`에 dword로 기록하며, 이 실행에서
`FS=0`이므로 `0x0158CC58`에 zero word가 기록되었습니다. 따라서 Task 648의
`0x010F1E56 RET`가 소비한 zero target의 직접 writer는 runtime zero-initialization
tail이 아니라 HLE 처리된 `PUSH FS`입니다. 다만 왜 이 slot이 최종 RET 시점에도
return slot로 남는지는 아직 stack balance/return boundary의 별도 문제입니다.

### 변경 파일

* `guest_memory_access.h/.cpp`: optional `GuestCpuContext`와 HLE trace provenance
* `instruction_emulation.cpp`: HLE stack, segment, memory-store, OR, FPU 경로 전달
* `aot_runtime_dispatch.cpp`, `linexe_glide_boundary.cpp`: context 보유 호출부 전달
* `ARCHITECTURE.md`: Linux x64 HLE guest-write provenance 구조 반영
* `docs/analysis/linux-port-frontier.md`: Task 649 확인 사실 누적
* `docs/design/20260909-649-linux-x64-zero-return-slot-writer.md`
* `docs/work-orders/20260909-649-linux-x64-zero-return-slot-writer.md`

### 검증

WSL Ubuntu-24.04에서 `CMAKE_BUILD_PARALLEL_LEVEL=2`로 Linux x64 Debug
`repiu_core_probe`와 `repiu`를 빌드했습니다. 빌드 중 WSL 종료나 메모리 부족은
재현되지 않았습니다.

```text
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

실제 실행은 기존과 동일하게 x64 transfer provenance를 출력했습니다.

```text
x64_transfer_producer_eip=0x010F1E56
x64_transfer_target_eip=0x00000000
x64_transfer_guest_esp=0x0158CC5C
x64_transfer_kind=ret
x64_transfer_failure=translation-failed
```

이후 의도된 unresolved thunk의 `UD2`에서 종료되었으며, zero target이나 RET
semantics는 수정하지 않았습니다.

### 남은 문제

이번 작업으로 zero word를 기록한 명령은 확인했지만, `PUSH FS`가 만든 stack slot이
후속 guest stack 조정에서 제거되지 않고 `RET`에 도달하는 이유는 해결하지 않았습니다.
다음 분석은 `0x010F9212` 이후 guest ESP 변화와 `0x010F1E56` 직전의 pop/adjust 경계를
원본 명령 흐름과 대조해야 합니다.

## English

### Result

The HLE guest-write trace now optionally receives `GuestCpuContext`. Calls to
`WriteGuestUInt8/16/32` that have the current guest context record the guest EIP as
`execution/source` and preserve EAX through EFLAGS in the bounded ring. Host utility
calls without a context retain the previous zero provenance.

The real `pumpit2a` run with `REPIU_GUEST_WRITE_TRACE=0x0158CC58` identified the
zero-dword writer:

```text
[repiu-guest-write-trace-tail] event=hle n=0x0000002A watch=0x0158CC58 execution=0x010F9212 source=0x010F9212 destination=0x0158CC58 size=0x00000004 bytes=00000000 eax=0x010F920C ebx=0x011A7B16 ecx=0x00000000 edx=0x000000FF esi=0x011A7B28 edi=0x00000000 esp=0x0158CC5C eflags=0x00200306
```

`0x010F9212` is the `PUSH FS` established by the existing segment trace and
original consecutive-instruction analysis. Segment HLE writes `context->guest_fs`
as a dword to `[ESP-4]`; `FS=0` in this run, so it wrote the zero word to
`0x0158CC58`. The direct writer of the zero target consumed by Task 648's
`0x010F1E56 RET` is therefore HLE `PUSH FS`, not runtime zero initialization.
Why that slot remains at the final RET position is still a separate stack-balance
and return-boundary question.

### Changed files

* `guest_memory_access.h/.cpp`: optional `GuestCpuContext` and HLE trace provenance
* `instruction_emulation.cpp`: context forwarding for HLE stack, segment, memory-store,
  OR, and FPU paths
* `aot_runtime_dispatch.cpp`, `linexe_glide_boundary.cpp`: forwarding at context-owning
  call sites
* `ARCHITECTURE.md`: Linux x64 HLE guest-write provenance structure
* `docs/analysis/linux-port-frontier.md`: accumulated Task 649 findings
* `docs/design/20260909-649-linux-x64-zero-return-slot-writer.md`
* `docs/work-orders/20260909-649-linux-x64-zero-return-slot-writer.md`

### Verification

Linux x64 Debug `repiu_core_probe` and `repiu` were built in WSL Ubuntu-24.04 with
`CMAKE_BUILD_PARALLEL_LEVEL=2`. No WSL termination or memory exhaustion recurred during
the build.

```text
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

The real run retained the existing x64 transfer provenance:

```text
x64_transfer_producer_eip=0x010F1E56
x64_transfer_target_eip=0x00000000
x64_transfer_guest_esp=0x0158CC5C
x64_transfer_kind=ret
x64_transfer_failure=translation-failed
```

It then ended at the intentional unresolved-thunk `UD2`; neither the zero target nor
RET semantics was modified.

### Remaining issue

This task identifies the instruction that wrote the zero word, but does not explain why
the stack slot created by `PUSH FS` survives later guest stack adjustment until `RET`.
The next analysis should compare guest ESP changes after `0x010F9212` with the original
instruction flow and the pop/adjust boundary immediately before `0x010F1E56`.
