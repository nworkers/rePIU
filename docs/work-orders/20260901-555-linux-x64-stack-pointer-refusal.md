# 20260901-555 x64 lowering의 stack pointer 구멍 작업 지시서

## 한국어

### 목적

guest `ESP`를 host `RSP`로 읽는 구멍을 fail-closed로 닫습니다. 설계는
[20260901-555](../design/20260901-555-linux-x64-stack-pointer-refusal.md)입니다.

### 작업

- `LongModeDivergence`에 `kStackPointerRegister`를 추가한다.
- stack pointer(`RSP`·`ESP`·`SP`·`SPL`)를 이름 부르는 명령을 `kUnsupported`로 거절한다.
  memory base, memory index, register operand를 모두 본다. 읽기·쓰기 구분하지 않는다.
- 이 검사는 memory operand 판정보다 **앞에** 둔다. `add esp,16`은 memory operand가 없어서
  지금 `kIdenticalBytes`로 통과하므로, memory 경로 안에 두면 잡히지 않는다.
- Task 552 lowering이 서 있는 전제("lowering 시점에 guest GPR n이 host GPR n에 있다")를
  헤더에 문장으로 적는다.
- 판정기 probe와 방출 probe에 항목을 더한다. `mov eax,[ebx+8]`이 여전히 lowering 되는
  것도 같은 자리에서 확인한다.

### 검증

세 호스트에서 `repiu_core_probe` 전부 통과. 판정기는 호스트 무관이므로 세 호스트의
출력이 같아야 한다. i386 경로는 `enable_long_mode_emission`이 기본 `false`이므로
영향받지 않는다.

## English

### Objective

Close, fail-closed, the hole that reads the host `RSP` where the guest meant `ESP`. The
design is [20260901-555](../design/20260901-555-linux-x64-stack-pointer-refusal.md).

### Work items

- Add `kStackPointerRegister` to `LongModeDivergence`.
- Refuse as `kUnsupported` any instruction naming the stack pointer (`RSP`, `ESP`, `SP`,
  `SPL`) -- as a memory base, a memory index, or a register operand, read or written.
- Put the check **before** the memory-operand judgement. `add esp,16` has no memory operand
  and passes as `kIdenticalBytes` today, so a check inside the memory path would miss it.
- Write down, in the header, the premise Task 552's lowering stands on: at the moment a
  lowered instruction runs, guest GPR *n* is in host GPR *n*.
- Add items to the classifier probe and the emission probe, including that
  `mov eax,[ebx+8]` is still lowered, so the refusal is shown to be targeted.

### Verification

`repiu_core_probe` passes on all three hosts. The classifier is host-independent, so the
three outputs must agree. The i386 path is unaffected because
`enable_long_mode_emission` defaults to `false`.
