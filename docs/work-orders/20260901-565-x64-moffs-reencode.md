# 20260901-565 x64 moffs 재인코딩 작업 지시서

## 한국어

### 목적

`MOV`의 moffs 형태(`A0`–`A3`)를 SIB 절대형으로 재인코딩합니다. 설계는
[20260901-565](../design/20260901-565-x64-moffs-reencode.md)입니다.

### 작업

- `A0`–`A3`를 거부 목록에서 옮기고 `kMoffsToSib` lowering을 추가한다.
- opcode를 ModRM 형태(`8A`/`8B`/`88`/`89`)로 바꾸고 `67`·ModRM `04`·SIB `25`·disp32를
  쓴다.
- 맨 형태와 `66` prefix 형태 둘 다 통과시킨다.
- census가 막고 있는 명령의 **바이트를 찍게** 한다. 추측 대신 사실로 정한다.
- walk가 **serviced boundary를 통과**하도록 고친다. `INT 21h`는 emitter가 못 내는 것이
  아니라 dispatcher가 처리하는 것이며, 벽이 아니라 문이다.

### 검증

Linux x64에서 moffs 읽기(`A1`)와 쓰기(`A3`)를 **실행**합니다. 읽기만 보면 저장 형태가
같은 주소에 닿는지 알 수 없습니다.

census로 **도달 가능 block**을 잽니다. Task 564와 같은 기준입니다.

Linux i386과 Win32는 회귀로 빌드·실행합니다.

## English

### Objective

Re-encode `MOV`'s moffs forms (`A0`–`A3`) into the SIB absolute form. The design is
[20260901-565](../design/20260901-565-x64-moffs-reencode.md).

### Work items

- Move `A0`–`A3` off the refusal list and add the `kMoffsToSib` lowering.
- Exchange the opcode for its ModRM form (`8A`/`8B`/`88`/`89`) and write `67`, ModRM `04`,
  SIB `25`, then the displacement.
- Admit both the bare form and the `66`-prefixed one.
- Make the census **print the bytes** of the instruction in the way, so it is a fact
  rather than a guess.
- Make the walk **pass through serviced boundaries**: an `INT 21h` is not something the
  emitter fails to produce but something the dispatcher handles -- a door, not a wall.

### Verification

**Execute** the moffs load (`A1`) and store (`A3`) on Linux x64; a read alone would not
show the store reaching the same address.

Measure **reachable blocks** with the census, by Task 564's criterion.

Build and run Linux i386 and Win32 as regressions.
