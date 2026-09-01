# 20260901-561 x64 direct call emission 작업 지시서

## 한국어

### 목적

long-mode emission에서 `kDirectCall`을 방출합니다. 설계는
[20260901-561](../design/20260901-561-x64-direct-call-emission.md)입니다.

### 작업

- `kDirectCall`의 push를 `{0x68, fallthrough}`로 합성해 기존 stack lowering에
  통과시킨다. push 바이트를 직접 쓰지 않는다 — 시퀀스는 한 곳에만 있어야 한다.
- jump는 Task 560의 `E9 rel32` slot을 그대로 쓴다.
- census가 `kDirectCall`을 방출로 세도록 맞추고 `agrees=`를 유지한다.
- **미해결 slot을 slot 시작부터 무력화한다.** call은 push가 jump보다 앞에 있으므로
  jump만 덮으면 push가 실행된다.
- 무력화된 entry의 의도 명령 수를 실제와 맞춘다. 검증기는 entry가 말한 개수와 바이트가
  decode되는 개수를 비교한다.

### 검증

Linux x64에서 **call을 실행**합니다. 확인할 것은 셋입니다.

1. 점프가 피호출자로 갔는가 (건너뛴 block의 값이 아닌가)
2. **guest 복귀 주소가 guest stack 메모리에 실제로 들어갔는가** — register가 아니라
   메모리를 직접 읽는다
3. `ESP`가 -4 됐는가

그리고 **미해결 call 경로를 바이트로 검사**합니다. 실행되지 않는 오류 경로는 작동한다는
증거가 없는 코드이므로, slot 시작 바이트가 `0xCC`인지와 emitter가 거부를 보고했는지
확인합니다.

Linux i386과 Win32는 회귀로 빌드·실행합니다.

## English

### Objective

Emit `kDirectCall` under long-mode emission. The design is
[20260901-561](../design/20260901-561-x64-direct-call-emission.md).

### Work items

- Synthesise the call's push as `{0x68, fallthrough}` and pass it through the existing
  stack lowering rather than writing the bytes here; the sequence must have one home.
- Reuse Task 560's `E9 rel32` slot for the jump.
- Count `kDirectCall` as emitted in the census and keep `agrees=` true.
- **Neutralise an unresolved slot from its first byte.** A call's push precedes its jump,
  so overwriting the jump alone lets the push run.
- Set the neutralised entry's intended instruction count to what the bytes now are. The
  verifier compares the count an entry claims against what its bytes decode to.

### Verification

**Execute a call** on Linux x64, checking three things: that the jump reached the callee
rather than the block it had to step over; that **the guest return address really reached
guest stack memory**, read from memory rather than from a register; and that `ESP` moved
by -4.

Then **check the unresolved-call path as bytes**. An error path that never runs is code
with no evidence it works, so confirm the slot's first byte is `0xCC` and that the emitter
reported the refusal.

Build and run Linux i386 and Win32 as regressions.
