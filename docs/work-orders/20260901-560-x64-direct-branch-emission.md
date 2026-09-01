# 20260901-560 x64 direct branch emission 작업 지시서

## 한국어

### 목적

long-mode emission에서 `kDirectJump`와 `kConditionalBranch`를 방출합니다. 설계는
[20260901-560](../design/20260901-560-x64-direct-branch-emission.md)입니다.

### 작업

- census의 `not-a-copy-record`를 plan kind로 나눠 보고한다. 다음 단위의 순서를 인상이
  아니라 수로 정하기 위해서다.
- `EmitLongModeDirectBranch`를 추가해 `E9 rel32`와 `0F 8x rel32`를 방출한다.
- timer safe point는 x64에서 붙이지 않는다. block fallthrough가 이미 같은 이유로
  제외하고 있으므로 그 판단을 넓히는 것이다.
- target이 cache 밖이면 32비트 dispatch stub이 아니라 boundary로 보낸다. 덮기 전에
  그 위치의 바이트가 예상한 opcode인지 확인한다.
- 방출된 분기와 미해결 edge를 각각 센다. 방출된 분기와 실행되는 분기는 다르다.
- census가 emitter와 계속 일치하는지 `agrees=`로 확인한다.

### 검증

Linux x64에서 **분기를 실행**합니다. block 세 개짜리 이미지를 만들어 조건을 바꿔 두
번 호출하고, 성립할 때와 성립하지 않을 때 서로 다른 값이 나오는지 확인합니다. 한
방향만 보면 분기가 아니라 직진을 확인한 것입니다.

census로 방출 가능 비율·완결 block·미해결 edge 수를 재고, Linux i386과 Win32를 회귀로
빌드·실행합니다.

## English

### Objective

Emit `kDirectJump` and `kConditionalBranch` under long-mode emission. The design is
[20260901-560](../design/20260901-560-x64-direct-branch-emission.md).

### Work items

- Split the census's `not-a-copy-record` by plan kind, so the next unit's order comes
  from counts rather than from an impression.
- Add `EmitLongModeDirectBranch`, emitting `E9 rel32` and `0F 8x rel32`.
- Do not emit the timer safe point on x64. The block fallthrough already leaves it out for
  the same reason; this extends that judgement.
- Send an out-of-cache target to a boundary rather than the 32-bit dispatch stub, checking
  the byte at the derived position against the opcode it must be before overwriting.
- Count emitted branches and unresolved edges separately. A branch that was emitted and a
  branch that runs are not the same thing.
- Keep the census agreeing with the emitter through the `agrees=` line.

### Verification

**Execute a branch** on Linux x64: build a three-block image, call it twice with the
condition changed, and confirm the two directions produce different values. Checking one
direction would be checking a straight line.

Measure the emittable fraction, complete blocks, and unresolved edges with the census, and
build and run Linux i386 and Win32 as regressions.
