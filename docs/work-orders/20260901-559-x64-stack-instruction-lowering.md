# 20260901-559 x64 stack 명령 lowering 작업 지시서

## 한국어

### 목적

`PUSH`/`POP`/`PUSHFD`/`POPFD`/`LEAVE`를 `R15D`를 쓰는 시퀀스로 낮춥니다. 설계는
[20260901-559](../design/20260901-559-x64-stack-instruction-lowering.md)입니다.

### 작업

- `LongModeLowering`에 stack 시퀀스 값을 추가한다.
- `ESP` 조정은 **`LEA`로만** 한다. `SUB`/`ADD`는 flag를 바꾸고 guest `PUSH`/`POP`은 바꾸지
  않는다.
- `R14D`를 emitter scratch로 확정하고 헤더에 적는다.
- `6A imm8`은 **부호확장**해서 imm32로 쓴다.
- `push esp`·`pop esp` 특례를 설계대로 처리한다.
- `LowerLongModeBytes`가 낸 **명령 개수**를 함께 보고하고, 방출 루프가 entry별로 기억해
  검증이 개수까지 확인하게 한다. `AotAddressMapEntry`에 필드를 더하지 않는다.
- probe: 시퀀스를 long mode 디코더로 확인하고, **실행**해서 guest stack 메모리·`ESP`
  이동·**flag 보존**·`pushfd`/`popfd` 왕복을 본다.

### 검증

- 세 호스트 `repiu_core_probe` 통과. x64는 실행 항목까지 포함.
- census를 다시 돌려 `operand-width` 감소분을 기록한다.
- i386 경로는 `enable_long_mode_emission` 기본 `false`라 영향받지 않는다.

## English

### Objective

Lower `PUSH`, `POP`, `PUSHFD`, `POPFD` and `LEAVE` into sequences that use `R15D`. The
design is [20260901-559](../design/20260901-559-x64-stack-instruction-lowering.md).

### Work items

- Add stack-sequence values to `LongModeLowering`.
- Adjust `ESP` with **`LEA` only**. `SUB`/`ADD` change flags and guest `PUSH`/`POP` do not.
- Settle `R14D` as the emitter's scratch and record it in the header.
- **Sign-extend** `6A imm8` to imm32.
- Handle the `push esp` and `pop esp` special cases per the design.
- Have `LowerLongModeBytes` also report the **instruction count** it produced, and have the
  emit loop remember it per entry so verification checks the count too. Do not add a field
  to `AotAddressMapEntry`.
- Probe: confirm the sequences with a long-mode decoder, and **execute** them to check
  guest stack memory, the `ESP` movement, **flag preservation**, and a `pushfd`/`popfd`
  round trip.

### Verification

- `repiu_core_probe` passes on all three hosts, including the execution items on x64.
- Re-run the census and record the drop in `operand-width`.
- The i386 path is unaffected because `enable_long_mode_emission` defaults to `false`.
