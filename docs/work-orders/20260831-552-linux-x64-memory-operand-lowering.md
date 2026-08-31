# 20260831-552 Linux x64 memory operand lowering 작업 지시서

## 한국어

### 목적

Task 546 결정 4의 확정("guest memory는 하위 4 GiB")을 판정기에 반영하고, 그 결과가
실제 하드웨어에서 성립하는지 실행으로 확인합니다. 설계는
[20260831-552](../design/20260831-552-linux-x64-memory-operand-lowering.md)입니다.

### 작업

- `LongModeLowering`을 추가한다: `kNone`, `kAddressSizePrefix`, `kAbsoluteToSib`.
- memory operand를 가진 명령을 `kUnsupported`에서 `kNeedsReencode`로 옮기고 lowering을
  이름으로 붙인다.
- `ModRM mod=00, rm=101`은 `kAbsoluteToSib`로 분류한다. prefix만으로는 고쳐지지
  않으므로 `kAddressSizePrefix`와 같은 칸에 두지 않는다.
- segment override가 있으면 계속 `kUnsupported`로 둔다.
- lowering을 바이트로 만들어 주는 함수를 추가한다. 판정만 하고 변환을 남에게
  맡기면 판정과 변환이 갈라질 수 있다.
- Linux x64 전용 probe로 **두 lowering을 실행**해 의도한 주소를 읽는지 확인한다.

### 검증

Linux x64에서 lowering된 바이트를 하위 4 GiB의 실행 가능한 페이지에 써 넣고 호출해,
`kAddressSizePrefix`가 base register의 상위 절반을 무시하는지와 `kAbsoluteToSib`가
`RIP` 위치와 무관하게 같은 값을 읽는지 확인합니다. Linux i386과 Win32는 회귀로
빌드·실행합니다.

## English

### Objective

Reflect Task 546's settled decision 4 ("guest memory is placed below 4 GiB") in the
classifier, and confirm by execution that the result holds on real hardware. The design
is [20260831-552](../design/20260831-552-linux-x64-memory-operand-lowering.md).

### Work items

- Add `LongModeLowering`: `kNone`, `kAddressSizePrefix`, `kAbsoluteToSib`.
- Move instructions with a memory operand from `kUnsupported` to `kNeedsReencode` and
  name the lowering.
- Classify `ModRM mod=00, rm=101` as `kAbsoluteToSib`. A prefix does not fix it, so it
  does not belong in the same bucket as `kAddressSizePrefix`.
- Keep an instruction with a segment override at `kUnsupported`.
- Add a function that produces the lowered bytes. Judging without doing the transform
  invites the judgement and the transform to drift apart.
- Add a Linux x64-only probe that **executes both lowerings** and checks they read the
  intended address.

### Verification

On Linux x64, write the lowered bytes into an executable page below 4 GiB and call them:
check that `kAddressSizePrefix` ignores the upper half of the base register, and that
`kAbsoluteToSib` reads the same value regardless of where `RIP` sits. Build and run Linux
i386 and Win32 as regressions.
