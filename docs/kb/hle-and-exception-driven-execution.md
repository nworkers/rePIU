# HLE와 예외 기반 직접 실행

## HLE

High Level Emulation은 하드웨어나 운영체제의 모든 내부 동작을 재현하지 않고, guest가 관찰하는 interface와 결과를 더 높은 수준에서 구현하는 방식이다. rePIU에서는 DOS file service, DPMI query, selector shadow state, port router가 이에 해당한다.

HLE의 핵심은 contract 정확성이다. 함수 번호가 같아도 register, flags, memory side effect, error code가 다르면 원본 프로그램의 분기가 달라진다.

## 직접 실행과 fault boundary

원본 user-mode x86 명령은 host x86 CPU에서 직접 실행한다. Win32 user mode에서 허용되지 않는 privileged instruction이나 미매핑 address 접근은 exception이 된다. Windows의 structured exception handling 개요는 [Microsoft SEH 문서](https://learn.microsoft.com/en-us/windows/win32/debug/structured-exception-handling)를 참고한다.

handler는 exception context의 EIP와 register를 읽고 opcode를 decode한 뒤, guest-visible 결과를 register/memory/flags에 반영하고 EIP를 instruction length만큼 전진시킬 수 있다.

## 정확성과 위험

* 너무 좁은 handler는 실행을 자주 중단하지만 잘못된 상태를 만들 가능성이 낮다.
* 너무 넓은 handler는 fault를 숨기고 손상된 상태로 멀리 진행할 수 있다.
* 따라서 opcode뿐 아니라 EIP 범위, address domain, selector, 선행 state, file path 결과 같은 구조적 조건을 함께 사용한다.

## 전체 CPU emulation과의 차이

rePIU는 instruction fetch/decode/execute 전체를 software CPU로 수행하지 않는다. 대부분의 원본 instruction은 실제 CPU가 실행하고, 운영체제·DPMI·hardware boundary만 HLE가 가로챈다. 이 선택은 원본 게임 로직 보존이라는 프로젝트 원칙과 연결된다.

# HLE and Exception-Driven Direct Execution

High-level emulation implements guest-observable contracts without reproducing every internal detail. rePIU directly executes ordinary original x86 instructions and intercepts DOS, DPMI, privileged-I/O, and unmapped-memory boundaries through Win32 exceptions. See Microsoft’s [Structured Exception Handling documentation](https://learn.microsoft.com/en-us/windows/win32/debug/structured-exception-handling).

Correct register, flag, memory, and error-code effects matter. Handlers are therefore constrained by opcode, address domain, selector, prior state, and observed execution context rather than broadly swallowing faults.
