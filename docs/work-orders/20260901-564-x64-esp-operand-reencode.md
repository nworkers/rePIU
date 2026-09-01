# 20260901-564 x64 `ESP` operand 재인코딩 작업 지시서

## 한국어

### 목적

`ESP`를 이름으로 쓰는 명령을 `R15D`를 쓰도록 재인코딩합니다. Task 563이 이것을
도달 가능 block 1개의 원인으로 지목했습니다. 설계는
[20260901-564](../design/20260901-564-x64-esp-operand-reencode.md)입니다.

### 작업

- `ESP`가 ModRM `reg`, ModRM `rm`, SIB `base` 중 어디에 있는지 Zydis의
  `operand.encoding`으로 판별한다.
- 세 자리 모두 필드를 `111`로 바꾸고 대응하는 `REX` 비트를 세운다. 한 명령이 둘을
  동시에 가질 수 있으므로 비트는 독립적으로 세운다.
- `REX`는 legacy prefix 뒤·opcode 앞에 **삽입**한다. guest는 32비트 코드라 이미 있을
  수 없다.
- opcode에 register가 박힌 형태(`push esp`, `inc esp`)는 통과시키지 않는다. 각각 Task
  559와 557의 것이다.
- 설명되지 않는 `ESP` 사용은 계속 거부한다.

### 검증

Linux x64에서 세 자리를 **각각 실행**합니다. `add esp,16`은 값뿐 아니라 **실행이
돌아오는지**를 봅니다 — 재인코딩이 없었다면 host `RSP`가 움직여 돌아올 수 없습니다.

census로 **도달 가능 block**을 잽니다. 이 단위의 목적이 그것이므로 방출 가능 비율이
아니라 그 수가 성패를 말합니다.

옛 규칙을 검사하던 probe들의 기대를 함께 갱신합니다.

## English

### Objective

Re-encode instructions naming `ESP` to name `R15D`. Task 563 identified these as the
reason execution reaches one block. The design is
[20260901-564](../design/20260901-564-x64-esp-operand-reencode.md).

### Work items

- Use Zydis's `operand.encoding` to tell ModRM `reg`, ModRM `rm` and SIB `base` apart.
- Set the field to `111` and the matching `REX` bit in all three, independently, since one
  instruction can have two.
- **Insert** the `REX` between the legacy prefixes and the opcode; guest code is 32-bit and
  can never already carry one.
- Do not admit registers embedded in the opcode -- those are Tasks 559's and 557's.
- Keep refusing any `ESP` use these three places do not explain.

### Verification

Execute each of the three places on Linux x64. For `add esp,16`, check that the run
**comes back at all**, not only its value: without the re-encoding the host's `RSP` moves
and there is no way back.

Measure **reachable blocks** with the census; that is what this unit is for, so it decides
the outcome rather than the emittable fraction.

Update the probes that assert the old rule.
