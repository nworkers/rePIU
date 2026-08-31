# POSIX signal handler와 상태 공유 / Sharing state with a POSIX signal handler

## 한국어

### 왜 이 프로젝트에 필요한가

Linux host에서 guest 실행 경계는 signal handler입니다. page protection fault,
planted `int3`, trap flag single-step이 모두 `SIGSEGV`/`SIGTRAP`으로 도착하고,
handler는 그 자리에서 상태를 읽어 무엇을 할지 정합니다. 따라서 "normal 경로가 쓰고
handler가 읽는 값"은 이 프로젝트의 실행 모델 한가운데에 있습니다.

### 규칙

handler와 나누어 쓰는 객체는 `volatile sig_atomic_t`이거나 atomic이어야 합니다.
C 표준이 signal handler에서 접근을 허용하는 것도 그 두 가지뿐입니다.

ordinary 변수로 두면 컴파일러는 handler의 존재를 알 수 없으므로 그 값을 읽는 코드가
없다고 판단할 수 있고, store를 지우거나 register에 남겨두거나 순서를 바꿀 수
있습니다. 이는 컴파일러 버그가 아니라 abstract machine에 handler가 없다는 사실의
결과입니다.

### 자주 틀리는 지점: 옆에 있는 volatile 접근은 아무것도 보장하지 않는다

```c
g_state.stage = kReadFault;                      /* ordinary store */
observed = *(volatile unsigned char*)(page + 16); /* 여기서 fault */
g_state.stage = kIdle;                            /* ordinary store */
```

가운데의 `volatile` 접근은 *다른* 객체를 향합니다. volatile 접근끼리는 서로 순서가
유지되지만, ordinary 객체에 대한 store를 그 사이에 붙잡아 두지는 않습니다. 두 store
사이에서 `stage`를 읽는 코드가 없으므로 첫 store는 dead store이고, 삭제는 정당합니다.

GCC는 `-O2`에서 실제로 삭제했습니다. handler는 이전 값을 보고 "내 fault가 아니다"라고
답했으며, fault layer는 규정대로 기본 동작을 복원했고 재시도된 접근이 프로세스를
죽였습니다. 같은 소스가 `-O0`에서는 통과했습니다.

> 최적화 수준에 따라 나타났다 사라지는 fault handler 동작은 대개 이 형태입니다.

### 함께 기억할 것

* handler 안에서 부를 수 있는 함수는 async-signal-safe 함수뿐입니다. `malloc`,
  iostream, 대부분의 lock은 여기에 들지 않습니다. 조사용 출력이 필요하면 `write(2)`를
  씁니다.
* `SA_NODEFER`는 handler 실행 중 같은 signal의 masking을 끕니다. 중첩 전달이 정상인
  설계에서는 필요하지만, 재개해도 원인이 사라지지 않으면 무한 재진입이 됩니다.
* handler가 재개를 선택했는데 아무것도 바꾸지 않았다면, faulting instruction이 그대로
  다시 실행됩니다. 진행을 보장하는 편집(접근 허용, IP 전진, flag 해제) 없이 재개하면
  안 됩니다.

### 출처

* ISO C, `signal.h` — signal handler에서 접근 가능한 객체
* `signal-safety(7)`, `sigaction(2)`, `sigaltstack(2)` (Linux man-pages)
* 이 프로젝트에서 실제로 부딪힌 기록: [Linux 이식 frontier](../analysis/linux-port-frontier.md) Task 549

## English

### Why this project needs it

On a Linux host the guest execution boundary *is* a signal handler. Page-protection
faults, planted `int3`s, and trap-flag single steps all arrive as `SIGSEGV`/`SIGTRAP`,
and the handler reads shared state on the spot to decide what to do. "Written by the
normal path, read by the handler" therefore sits in the middle of this project's
execution model.

### The rule

An object shared with a handler must be `volatile sig_atomic_t` or atomic. Those are also
the only objects the C standard lets a handler touch.

Left as an ordinary variable, the compiler cannot know the handler exists, so it may
conclude nothing reads the value -- and delete the store, keep it in a register, or move
it. That is not a compiler bug; it follows from the handler being absent from the
abstract machine.

### The common mistake: a neighbouring volatile access guarantees nothing

```c
g_state.stage = kReadFault;                       /* ordinary store */
observed = *(volatile unsigned char*)(page + 16); /* faults here */
g_state.stage = kIdle;                            /* ordinary store */
```

The `volatile` access in between names a *different* object. Volatile accesses keep their
order relative to one another; they do not pin an ordinary store between them. Nothing
the compiler can see reads `stage` between the two stores, so the first is dead and
removing it is correct.

GCC did remove it at `-O2`. The handler saw the earlier value, answered "not my fault",
the fault layer restored the default action as it must, and the retried access killed the
process. The same source passed at `-O0`.

> A fault handler whose behaviour appears and disappears with the optimisation level is
> usually this.

### Worth remembering alongside

* Only async-signal-safe functions may be called from a handler. `malloc`, iostreams, and
  most locks are not among them; use `write(2)` when a handler has to say something.
* `SA_NODEFER` turns off masking of the same signal while the handler runs. It is needed
  where nested delivery is by design, but it turns "resumed without fixing the cause"
  into unbounded re-entry.
* A handler that chooses to resume without changing anything re-executes the faulting
  instruction. Never resume without an edit that guarantees progress: granting access,
  advancing the instruction pointer, or clearing a flag.

### Sources

* ISO C, `signal.h` -- objects a signal handler may access
* `signal-safety(7)`, `sigaction(2)`, `sigaltstack(2)` (Linux man-pages)
* Where this project hit it: [Linux port frontier](../analysis/linux-port-frontier.md), Task 549
