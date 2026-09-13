# Task 667 작업 로그: Linux x64 AOT native write provenance

## 한국어

### 작업 목적

`0x010F0237 RET`가 guest stack의 `0x0158C860`에서 0을 읽는 현상에 대해,
특정 guest EIP를 예외 처리하지 않고 실제 마지막 memory writer를 공통 경로에서
확인합니다.

### 구현 내용

- Linux x64 AOT `kCopy` 명령 중 명시적 memory-write 뒤에 opt-in observer를 연결했습니다.
- observer는 원본 LEGACY_32 명령을 Zydis로 다시 디코드하고 base/index/scale/displacement로
  guest 목적지를 계산합니다.
- 기존 `REPIU_GUEST_WRITE_TRACE=<address>` ring에 `native-aot` 이벤트를 추가했습니다.
- `REP STOSD`와 `REP MOVS` HLE bulk write도 동일한 watch 주소를 포함하면 기록하도록 했습니다.
- observer 사용 시 선택된 watch page만 AOT page write-watch에서 제외하고, 다른 coherence watch는 유지합니다.
- dispatch frame 설치 시 `context`를 초기화했습니다.
- observer 호출 경계에서 GPR/flags와 x87/SSE 상태를 보존하도록 동적 host stack 정렬 및
  `FXSAVE64/FXRSTOR64`를 추가했습니다.
- `verbose` 값은 첫 128개 observer 호출의 decode/destination/match 상태를 출력합니다.

### 검증

Linux x64 `repiu`와 `repiu_core_probe`를 빌드하고 probe를 실행했습니다.

```text
[100%] Built target repiu
[100%] Built target repiu_core_probe
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

다음 bounded runtime을 실행했습니다.

```text
REPIU_GUEST_WRITE_TRACE=0x0158C860
REPIU_LINUX_X64_MEMORY_WRITE_TRACE=verbose
REPIU_LINUX_X64_STACK_TRACE=1
REPIU_LINUX_X64_RETURN_FRAME_TRACE=1
timeout -k 1s 35s build/linux_x64_debug/repiu pumpit2a
```

확인된 마지막 writer는 다음과 같습니다.

```text
[repiu-guest-write-trace] event=native-aot n=1 watch=0x0158C860 execution=0x010EFEDC source=0x010EFEDC destination=0x0158C860 size=4 bytes=00000000
```

매칭 상세 로그는 다음과 같습니다.

```text
[repiu-native-write-match] guest=0x010EFEDC bytes=89451885C0587442 destination=0x0158C860 size=4 eax=0x00000000 ebx=0x0158C92C ecx=0xFFFFFFEC edx=0x0138C679 esi=0x00000001 edi=0x010FB81E ebp=0x0158C848 esp=0x0158C848
```

앞의 `89 45 18`은 일반적인 `MOV [EBP+0x18], EAX`이며, 당시 `EAX=0`이고
`EBP=ESP=0x0158C848`이므로 목적지가 `0x0158C860`이 됩니다. 뒤에는
`TEST EAX,EAX`, 인자 정리용 `POP EAX`(`0x010EFEE1`), 그리고 공통
`JMP 0x010F0232`가 이어집니다. 따라서 현재 실행에서 보인 `ESP=0x0158C84C`는
공통 AOT/HLE 재진입기가 임의로 보정한 값이라고 단정할 수 없고, 원본 호출
규약과 DOS/HLE 반환값을 함께 확인해야 합니다.

이후 실행은 observer가 유발한 `SIGSEGV` 없이 기존 unresolved return thunk의
의도적인 `SIGTRAP`으로 종료했습니다. 따라서 writer의 provenance는 확인했지만,
zero return을 정상적인 유효 return address로 바꾸는 수정은 이 작업에서 수행하지
않았습니다. 정상 게임 실행은 여전히 미확정입니다.

### 관찰 중 발생한 결함과 수정

첫 observer 구현은 dispatch frame의 `context`가 비어 있어 디코드하지 못했습니다.
이를 공통 설치 단계에서 초기화했습니다. 이후 C++ observer 호출이 guest XMM/FPU
상태를 보존하지 않아 `SIGSEGV`가 발생했으며, 이는 특정 명령 예외가 아니라 공통
ABI 보존 누락이었습니다. 동적 stack 정렬과 `FXSAVE64/FXRSTOR64`를 적용해 수정했습니다.

추가로 `REPIU_AOT_TRANSFER_TARGET_TRACE=0x010F0232`를 사용해 다음 공통
address-map breakpoint를 확인했습니다.

```text
[repiu-aot-transfer-target] kind=breakpoint lookup=address-map source=0x00000000 target=0x010F0232 bytes=000000000000 cache=0x20002CAB esp=0x0158C84C
```

`source=0`은 address-map 조회 필드이며 원본 직접 분기의 부재를 뜻하지 않습니다.
정적 xref의 `0x010EFF2A` 및 `0x010EFFE4` 직접 `JMP`는 guest ESP를 바꾸지
않으므로, 현재 증거상 네 바이트는 AOT direct-branch emitter나 공통 HLE
재진입기가 추가한 것이 아닙니다. zero 값의 원천은 앞선 `0x010F0B50` 계열
호출의 DOS `INT 21h AH=43h` 파일 속성 조회 결과로 좁혀졌습니다. 다음 작업은
특정 EIP/ESP 예외가 아니라 이 공통 DOS/HLE 파일 조회 결과를 추적하는 것입니다.

## English

### Purpose

Identify the actual last memory writer for the zero read at guest stack slot
`0x0158C860` consumed by `RET` at `0x010F0237`, using a common mechanism rather
than an exception for one guest EIP.

### Implementation

- Attached an opt-in observer after explicit memory-writing Linux x64 AOT `kCopy` instructions.
- Re-decoded the original LEGACY_32 instruction with Zydis and computed its guest destination
  from base, index, scale, and displacement.
- Added the `native-aot` event to the existing `REPIU_GUEST_WRITE_TRACE=<address>` ring.
- Added matching write records for successful HLE `REP STOSD` and `REP MOVS` bulk writes.
- Excluded only the selected watch page from AOT page write-watch while the observer is enabled;
  other coherence watches remain active.
- Initialized `context` when installing the dispatch frame.
- Preserved GPRs/flags and x87/SSE state across the observer call with dynamic host-stack alignment
  and `FXSAVE64/FXRSTOR64`.
- Added a bounded verbose mode that reports decode, destination, and match status for the first
  128 observer calls.

### Verification

Built and ran the Linux x64 `repiu` and `repiu_core_probe` targets.

```text
[100%] Built target repiu
[100%] Built target repiu_core_probe
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

The bounded run recorded this last writer:

```text
[repiu-guest-write-trace] event=native-aot n=1 watch=0x0158C860 execution=0x010EFEDC source=0x010EFEDC destination=0x0158C860 size=4 bytes=00000000
```

The match detail was:

```text
[repiu-native-write-match] guest=0x010EFEDC bytes=89451885C0587442 destination=0x0158C860 size=4 eax=0x00000000 ebx=0x0158C92C ecx=0xFFFFFFEC edx=0x0138C679 esi=0x00000001 edi=0x010FB81E ebp=0x0158C848 esp=0x0158C848
```

The leading `89 45 18` is the ordinary `MOV [EBP+0x18], EAX`. At that point
`EAX=0` and `EBP=ESP=0x0158C848`, so the computed destination is `0x0158C860`.
It is followed by `TEST EAX,EAX`, the argument-cleanup `POP EAX` at `0x010EFEE1`,
and a shared `JMP 0x010F0232`. Therefore the observed `ESP=0x0158C84C` cannot yet
be attributed to an arbitrary correction by common AOT/HLE reentry; the original
call convention and DOS/HLE result must be checked together.

The run then reached the existing intentional `SIGTRAP` in the unresolved return
thunk without an observer-induced `SIGSEGV`. Writer provenance is confirmed, but this
task did not replace the zero with a fabricated valid return address. Normal game
execution remains unresolved.

### Defects found during observation

The first observer implementation could not decode because the dispatch frame's
`context` was unset; the common installation step now initializes it. The following
C++ observer call corrupted guest XMM/FPU state and caused `SIGSEGV`. That was a
common ABI state-preservation omission, not an instruction-specific exception.
Dynamic stack alignment and `FXSAVE64/FXRSTOR64` corrected it.

An additional bounded run with `REPIU_AOT_TRANSFER_TARGET_TRACE=0x010F0232`
recorded this common address-map breakpoint:

```text
[repiu-aot-transfer-target] kind=breakpoint lookup=address-map source=0x00000000 target=0x010F0232 bytes=000000000000 cache=0x20002CAB esp=0x0158C84C
```

The `source=0` value is the address-map lookup field, not evidence that the
original direct branches are absent. Static xrefs show direct `JMP`s from
`0x010EFF2A` and `0x010EFFE4`; neither changes guest ESP. The zero value is now
narrowed to the preceding `0x010F0B50`-family DOS `INT 21h AH=43h` file-attribute
query result. The next task should trace that common DOS/HLE service result rather
than add an EIP- or ESP-specific exception.
