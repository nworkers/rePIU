# 20260901-562 x64 return dispatch 작업 지시서

## 한국어

### 목적

`kReturn`을 방출하고 guest 복귀 주소를 cache 주소로 잇습니다. 설계는
[20260901-562](../design/20260901-562-x64-return-dispatch.md)입니다.

### 작업

- x64 return-dispatch thunk를 assembly로 추가한다. `and rsp, -16`으로 alignment를
  **강제**한다 — `jmp`로 도달하므로 진입 위상에 계약이 없다.
- thunk가 읽을 frame·context·resolver 포인터와 설치 API를 추가한다. **engine runtime이
  아직 없으므로 전역이며, 그것이 최종 형태가 아니라는 것을 헤더에 적는다.**
- return slot을 네 명령으로 방출한다. thunk 주소는 `movabs`로 R12에 싣는다 — rel32는
  거리가 보장되지 않고, `jmp [rip+disp]`는 code 안에 데이터를 남겨 검증기가 decode하려
  든다.
- 방출된 return을 세는 counter를 추가한다.
- **census가 `#if`를 복사하지 않고 emitter에게 묻게 한다.** return 방출은 host에 따라
  달라지는 첫 결과다.

### 검증

Linux x64에서 **call과 return이 이어지는지 실행**합니다. 값 셋이 실패 셋을 구분해야
합니다 — `0x4444`(이어짐), `0x3333`(피호출자에서 멈춤), `0x1111`(call도 안 감).
resolver가 물은 주소와 호출 횟수, guest stack 균형도 확인합니다.

기존 probe들의 기대가 바뀌는 것을 함께 처리합니다. 닫는 return이 더 이상 거부가
아니고, `long_mode_emission`이 "`kCopy` 외 전부 boundary"를 return으로 검사하고
있습니다.

Linux i386과 Win32는 회귀로 빌드·실행하며, 두 host에는 thunk가 없으므로 return이
계속 boundary여야 합니다.

## English

### Objective

Emit `kReturn` and join a guest return address to a cache address. The design is
[20260901-562](../design/20260901-562-x64-return-dispatch.md).

### Work items

- Add the x64 return-dispatch thunk in assembly, **forcing** alignment with
  `and rsp, -16`: it is reached by `jmp`, so nothing promises the entry phase.
- Add the frame, context and resolver pointers it reads, and an install API. **They are
  globals because x64 has no engine runtime yet, and the header says that is not the
  final shape.**
- Emit the return slot as four instructions, loading the thunk address into R12 with
  `movabs` -- rel32's distance is not guaranteed, and `jmp [rip+disp]` would leave data
  inside code that the verifier decodes.
- Count emitted returns.
- **Make the census ask the emitter rather than copy the `#if`.** Emitting a return is
  the first long-mode outcome that depends on the host.

### Verification

**Execute a call and a return joining** on Linux x64. Three values must separate three
failures: `0x4444` (joined), `0x3333` (stopped in the callee), `0x1111` (the call never
went). Check the address the resolver was asked about, the number of calls, and that the
guest stack balances.

Handle the expectations this changes in existing probes: the closing return is no longer
a refusal, and `long_mode_emission` was checking "everything but `kCopy` is a boundary"
using a return.

Build and run Linux i386 and Win32 as regressions; neither has the thunk, so returns must
stay boundaries there.
