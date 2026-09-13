# Task 669 설계: Linux x64 PUSH r/m32 guest-stack lowering

## 한국어

### 목적

Task 668은 DOS `AH=43h`가 `.ovl`을 조회해 `0x0002/CF=1`을 반환하고,
F0B50 wrapper가 그 실패를 zero로 바꾸는 것을 확인했습니다. 원본 호출부는
`0x010EFE55 PUSH 0x010EFE2C`로 올바른 인자 포인터를 guest stack에 기록하지만,
F0B50 내부의 `0x010EFED4 PUSH [EBP+0x18]`는 x64 AOT에서 일반 memory
lowering으로 처리됩니다.

long mode에는 guest의 32-bit `PUSH r/m32`와 같은 implicit guest-stack
동작이 없습니다. 현재 경로는 host RSP를 사용하고 guest ESP를 갱신하지
않으므로, callee가 `[ESP+0x20]`에서 읽는 인자가 달라질 수 있습니다. 이번
작업은 이 공통 ISA 차이를 일반 lowering으로 해결하는 것이 목적입니다.

### 설계 결정

bare 32-bit `FF /6 PUSH r/m32`를 `kStackSequence`로 분류하고 다음과 같이
내립니다.

1. register source이면 guest register 값을 사용합니다. `PUSH ESP` 형태는
   decrement 전에 `R15D`를 `R14D`에 보존합니다.
2. memory source이면 guest ESP를 decrement하기 전에 memory operand를
   `R14D`로 읽습니다. 이 순서는 source 주소가 guest ESP를 기반으로 하는
   경우에도 원본 PUSH의 주소 계산 순서를 보존합니다.
3. absolute `disp32` memory form은 `0x67`만 붙이지 않고 SIB absolute form으로
   재작성합니다. 그렇지 않으면 long mode의 `ModRM rm=101`이 RIP-relative로
   남기 때문입니다.
4. guest ESP를 `LEA R15D,[R15D-4]`로 갱신하고 `R14D` 또는 source register를
   guest stack `[R15D]`에 32-bit로 기록합니다.
5. 일반 memory operand의 32-bit addressing prefix와 필요한 `R15D` base
   rewrite를 observer가 아니라 lowering 자체에서 처리합니다. segment 또는
   operand-size prefix가 있는 형태는 이번 범위에서 계속 boundary로 둡니다.

이 lowering은 flags를 변경하지 않는 `LEA`를 사용하고, host RSP에는 접근하지
않습니다. `CALL`, `JMP`, `RET`의 control-flow resolver 정책은 변경하지
않습니다.

### 범위와 비범위

- 범위: bare `PUSH r/m32` register/memory form의 공통 long-mode lowering
- 범위: `[ESP+disp]` 및 absolute `disp32` source를 포함한 guest-stack 주소 계산
- 비범위: 특정 guest EIP/ESP에 대한 조건문
- 비범위: DOS/HLE file path 규칙 변경
- 비범위: 16-bit `PUSH`, segment override, control-flow `FF /2,/3`
- 비범위: return resolver 또는 zero-result 정책 변경

### 검증 기준

long-mode compatibility probe가 `FF /6`를 `kStackSequence`로 분류하고,
register/memory/ESP-based/absolute-memory source의 lowered bytes를 long-mode decoder로
검증해야 합니다. Linux x64 `repiu`와 core probe를 빌드한 뒤 bounded
`pumpit2a`에서 AH=43h path가 `.ovl`이 아닌 원본 module path로 바뀌고,
기존 zero-return frontier를 통과하는지 확인합니다.

## English

### Purpose

Task 668 established that DOS `AH=43h` queries `.ovl`, returns `0x0002/CF=1`,
and the F0B50 wrapper converts that failure to zero. The original caller writes
the expected argument pointer with `PUSH 0x010EFE2C` at `0x010EFE55`, but the
`PUSH [EBP+0x18]` at `0x010EFED4` inside F0B50 is currently treated as ordinary
memory lowering by the x64 AOT emitter.

Long mode has no implicit guest-stack equivalent of a 32-bit `PUSH r/m32`.
The current path can use host RSP without advancing guest ESP, changing the
argument read by the callee at `[ESP+0x20]`. This task fixes that general ISA
width/stack mismatch through common lowering.

### Design decision

Classify bare 32-bit `FF /6 PUSH r/m32` as `kStackSequence` and lower it as
follows:

1. For a register source, use the guest register value. For `PUSH ESP`, preserve
   the pre-decrement value in `R14D` from `R15D`.
2. For a memory source, load the guest memory operand into `R14D` before changing
   guest ESP. This preserves the original ordering even when the source address
   is based on guest ESP.
3. Rewrite an absolute `disp32` memory form to the SIB absolute encoding rather
   than merely adding `0x67`; otherwise long mode keeps `ModRM rm=101` as
   RIP-relative.
4. Adjust guest ESP with `LEA R15D,[R15D-4]` and store `R14D` or the source
   register as a 32-bit dword at guest stack `[R15D]`.
5. Perform 32-bit address-size handling and any required `R15D` base rewrite in
   the lowering itself. Forms with segment or operand-size prefixes remain
   boundaries in this task.

The lowering uses `LEA`, which preserves flags, and never touches host RSP.
Control-flow resolver policy for `CALL`, `JMP`, and `RET` is unchanged.

### Scope and non-goals

- Scope: common long-mode lowering for bare `PUSH r/m32` register/memory forms.
- Scope: source addressing based on guest ESP and absolute `disp32` memory forms.
- Non-goal: add any guest EIP/ESP-specific condition.
- Non-goal: change DOS/HLE file-path rules.
- Non-goal: handle 16-bit PUSH, segment overrides, or control-flow `FF /2,/3`.
- Non-goal: change return resolution or zero-result policy.

### Verification criteria

The long-mode compatibility probe must classify `FF /6` as `kStackSequence`
and decode the lowered register, memory, ESP-based, and absolute-memory forms
with a long-mode
decoder. Build the Linux x64 `repiu` and core probe, then run bounded `pumpit2a`
to confirm that AH=43h receives the original module path rather than `.ovl` and
that execution passes the existing zero-return frontier.
