# Task 667 설계: Linux x64 AOT native write provenance

## 한국어

### 목적

Task 666은 `0x010F0237 RET`가 guest stack의 `0x0158C860` 슬롯에서 0을 읽는다는
것을 확인했지만, 기존 stack-write ring은 PUSH와 CALL만 기록합니다. 따라서 일반
`MOV`·FPU 등 AOT native memory write가 해당 슬롯을 마지막으로 덮었는지 확인할 수
없습니다.

### 설계 결정

새로운 opt-in `REPIU_LINUX_X64_MEMORY_WRITE_TRACE=1`을 추가합니다. Linux x64
long-mode emitter가 `kCopy` 명령 중 명시적 memory-write를 판별하면, 변환된 guest
명령 직후에 observer 호출을 삽입합니다. observer는 다음을 수행합니다.

1. 현재 guest GPR과 EIP를 임시 AOT dispatch frame에 저장합니다.
2. 원본 guest 명령을 LEGACY_32 Zydis decoder로 다시 해석합니다.
3. memory operand의 base/index/scale/displacement로 32-bit guest destination을
   계산합니다.
4. destination이 기존 `REPIU_GUEST_WRITE_TRACE` watch 주소를 포함할 때만 기존
   write-trace ring에 `native-aot` 사건을 기록합니다.

이 경로는 guest memory를 변경하지 않으며, observer 호출 전후에 guest caller-saved
register와 flags를 복구합니다. trace가 꺼져 있으면 cache bytes와 실행 경로는
변경되지 않습니다. segment-override 전용 emitter와 암시적 string write는 이
작업에서 억지로 추정하지 않고 기록하지 않습니다.

### 범위와 비범위

- 범위: AOT native `kCopy` explicit memory-write의 destination provenance 관찰
- 범위: 기존 `REPIU_GUEST_WRITE_TRACE=<address>` ring/출력 형식 재사용
- 비범위: guest 명령 의미 변경, stack pointer 보정, return resolver 정책 변경
- 비범위: 특정 guest EIP를 조건으로 한 예외 처리
- 비범위: write-watch page protection 경로 변경

### 검증 기준

기존 bounded failure run에서 `REPIU_GUEST_WRITE_TRACE=0x0158C860`와 새 trace를
함께 켜도 page protection fault로 실행이 조기 종료되지 않아야 합니다. 이후
`native-aot` 기록의 execution/source EIP와 값이 실제 반환 슬롯의 마지막 writer와
일치하는지 확인합니다. trace를 끈 일반 빌드에서는 기존 core probe와 Linux x64
빌드가 통과해야 합니다.

### 추가 진단 모드

설정값을 `verbose` 또는 `2`로 지정하면 observer 호출의 처음 128개에 대해
디코드 성공 여부, 계산된 목적지, 크기, watch 주소 일치 여부를 추가 출력합니다.
이를 통해 observer가 생성되지 않았거나 도달하지 않은 경우와 명령어는 실행됐지만
목적지가 watch 주소와 일치하지 않은 경우를 구분할 수 있습니다.

watch 주소와 일치하는 쓰기는 원본 명령의 제한된 바이트와 guest 레지스터 snapshot도
출력하여 명령이 계산한 source 값과 실제 저장 값을 비교할 수 있도록 합니다.

캐시 진입 전에 dispatch frame의 context 필드를 초기화하여 observer가 별도의
전역 context 조회 없이 guest runtime 범위를 소유한 동일한 `ThreadContext`를
사용하도록 합니다.

## English

### Purpose

Task 666 confirmed that the `RET` at `0x010F0237` reads zero from guest-stack slot
`0x0158C860`, but the existing stack-write ring records only PUSH and CALL. It cannot
show whether an ordinary `MOV`, FPU, or other AOT-native memory write overwrote the
slot last.

### Design decision

Add opt-in `REPIU_LINUX_X64_MEMORY_WRITE_TRACE=1`. When the Linux x64 long-mode
emitter identifies an explicit memory-writing `kCopy` instruction, it emits an
observer call immediately after the lowered guest instruction. The observer:

1. saves the current guest GPRs and EIP in the temporary AOT dispatch frame;
2. decodes the original guest instruction with the LEGACY_32 Zydis decoder;
3. computes the 32-bit guest destination from base, index, scale, and displacement;
4. records a `native-aot` event in the existing write-trace ring only when the
   destination contains the `REPIU_GUEST_WRITE_TRACE` watch address.

The shared HLE string handlers also record successful `REP STOSD` and `REP MOVS`
bulk writes that contain the watch address. This covers writes that are not
represented by an explicit AOT memory operand without changing their data path.

When the setting is `verbose` (or `2`), the observer additionally prints the
first 128 observer calls, including decode success, computed destination, size,
and watch-address match. This separates “the observer was not emitted or not
reached” from “the instruction was reached but its destination did not match.”
Every matching write also prints a bounded original-byte and guest-register
snapshot so the instruction's source value can be compared with the stored value.

The path does not modify guest memory and restores guest caller-saved registers and
flags around the observer call. While the native observer is enabled, only the
selected watch page is removed from AOT write-watch protection; other coherence
watches remain active. When disabled, cache bytes and execution are unchanged.
Segment-override emitters and implicit string writes are deliberately not guessed in
this task.

The dispatch installation seeds the frame's context field before entering the
cache, so observers can use the same `ThreadContext` that owns the guest runtime
range without introducing a second context lookup mechanism.

### Scope and non-goals

- Scope: provenance for explicit memory writes in AOT-native `kCopy` instructions
- Scope: reuse the existing `REPIU_GUEST_WRITE_TRACE=<address>` ring and output
- Scope: temporarily exclude the selected watch page from write-watch protection
- Scope: provenance for successful HLE `REP STOSD` and `REP MOVS` writes
- Scope: bounded observer-call diagnostics under the `verbose` setting
- Non-goal: change guest instruction semantics, stack pointers, or return resolution
- Non-goal: add an EIP-specific exception
- Non-goal: change any other AOT coherence page-protection policy

### Verification criteria

The existing bounded failure run with `REPIU_GUEST_WRITE_TRACE=0x0158C860` and the
new trace enabled must not terminate early merely because the diagnostic is active.
The resulting `native-aot` or HLE string-write execution/source EIP and value must
identify the actual last writer of the return slot. With the trace disabled, the normal Linux x64 build
and existing core probes must continue to pass.
