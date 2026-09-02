# 작업 기록 20260903-577 — x64 fault 경로의 guest `ESP`

설계: [20260903-577](../design/20260903-577-x64-guest-esp-context.md) ·
작업 지시: [20260903-577](../work-orders/20260903-577-x64-guest-esp-context.md)

## 정정 — 3.20절 항목 2는 절반이 틀린 진단이었습니다

3.20절은 이 항목을 "`Eip`와 `Esp` 둘 다 틀린 값"으로 적었습니다. 코드를 다시 읽고
실행 로그를 보니 **`Eip`는 틀리지 않았습니다.**

엔진은 fault 시점의 `Eip`를 guest EIP가 아니라 **cache 주소로 취급합니다** —
`IsAotCacheAddress(context, Eip)`로 판정하고 `AotGuestAddressForExecutionAddress`로
변환합니다. i386에서도 같습니다. code cache가 게스트 바이트를 host 주소에 담고
있으니 그 안에서 난 fault의 EIP는 i386에서도 cache 주소입니다.

x64에서 `REG_RIP`의 하위 32비트가 그 cache 주소가 되려면 cache가 4 GiB 아래에
있어야 하는데, Task 576의 실제 실행이 확인해 줍니다.

```text
[loader] Win32 AOT cache base/bytes/entry: 0x20000000/326468/0x20000000
```

**이 세션에서 가정이 측정으로 반증된 세 번째입니다** — Task 574의 SIB 기대값,
Task 575의 주소 잘림, 그리고 이번 `Eip`. 세 번 다 "타입이 맞으니 값도 맞겠지"
방향이 아니라 그 반대, **"틀렸을 것"이라고 적어 둔 쪽이 틀렸습니다.**

## 근인 — `Esp`는 두 방향으로 틀렸습니다

i386에서는 guest ESP와 host ESP가 한 레지스터라 `REG_UESP`를 읽는 것이 옳습니다.
x64에서는 Task 546 결정 3이 host RSP를 SysV 스택으로 두고 Task 558이 guest ESP를
`R15D`에 두므로 둘은 다른 레지스터입니다.

**읽기**: 엔진은 `Esp`를 guest 주소로 씁니다 — 게스트 스택에서 `[Esp+0x08]`,
`[Esp+0x10]`을 읽고, `guest_return_esp`에 저장하고, `in_range(Esp, ...)`로 게스트
arena 안인지 검사합니다. host RSP는 그 arena 밖입니다.

**쓰기가 더 나쁩니다.** 엔진은 `Esp`를 **수정합니다**(`ZYDIS_REGISTER_ESP` 쓰기,
`RecoverToHost`). 그 값이 `REG_RSP`로 돌아가면 **host 스택 포인터가 게스트 주소로
옮겨지고**, 커널이 그 컨텍스트로 resume합니다.

## 구현

- 읽기: `registers->Esp = Register(machine, REG_R15);`
- 쓰기: `machine.gregs[REG_R15]`에 **zero-extend**
- `machine.gregs[REG_RSP]` 기록 **제거**

zero-extend인 이유는 Task 558의 불변식입니다 — `R15`의 상위 절반은 0이어야
합니다. guest ESP를 통한 접근이 `[r15]`로 방출되고 거기서는 64비트 전체가
주소이기 때문이고, emitter는 `lea r15d, ...`로만 써서 하드웨어에 맡깁니다.
`merge`는 상위 절반을 있던 대로 두는데 그것은 불변식을 **유지하는 것이 아니라
가정하는 것**입니다.

## 검증 — 음성 테스트로 검사가 유효한지 먼저 확인했습니다

probe는 원래 x64에서 `gregs[REG_RSP] == kEsp`를 단언했습니다. **바꾸려는 그
가정을 검사하고 있었습니다.** 새 계약 두 가지로 바꿨습니다.

1. `Esp`가 `R15`로 왕복하고, **64비트 전체**를 비교합니다. 하위 32비트만 봤다면
   `merge`로 상위에 쓰레기를 남긴 구현도 통과했을 것입니다.
2. store 전에 `RSP`에 심어 둔 표식(`0x00007FFF12345678`, 4 GiB 위)이 store 뒤에도
   그대로일 것.

두 번째가 없으면 `RSP`를 여전히 덮어쓰는 구현이 통과합니다. 그래서 **그것을 실제로
확인했습니다** — `REG_RSP` 기록을 일시적으로 되살려 빌드하니:

```text
guest_cpu_context_round_trip=false
!! guest_cpu_context failed
```

되돌린 뒤 다시 통과합니다. 검사가 옛 동작을 잡는다는 것이 추론이 아니라 관측
입니다.

### 결과

| 항목 | 결과 |
|---|---|
| Linux x64 `repiu_core_probe` | 20/20, failures 0 |
| Linux x64 `repiu_core_probe` (음성 테스트) | `round_trip=false` — 검사가 유효 |
| x64 `repiu` 실행 (`pumpit2a`) | Task 576과 **같은 지점** 정지, exit 0 |
| Linux i386 `repiu` 링크 | 성공 |
| Linux i386 `repiu_core_probe` | 19/19, failures 0, `guest_cpu_context_all=true` |
| Win32 `repiu_aot_probe` | `_all=true` 41개, `_all=false` 0개 |

x64 실행의 정지 지점이 **움직이지 않아야** 맞습니다. 이 단위는 게스트를 돌리기
시작하지 않으므로, 움직였다면 그것이 회귀입니다. 같은 곳에서 멈춥니다.

```text
[loader] Win32 minimal execution message:
         minimal original entry execution requires a 32-bit host
```

## 남은 것

| # | 항목 | 상태 |
|---|---|---|
| 0 | 엔진이 long-mode 방출을 켜는 것 | 해결 (576) |
| 1 | 심볼 두 개 | 해결 (575) |
| 2 | fault 경로 `Esp` | **해결** (`Eip`는 애초에 옳았음) |
| 3 | guest entry (`return 4`) | 미해결 — **다음** |
| 4 | dispatch thunk 5개 | 불필요 |
| 5 | `ValidateAotCodeCacheHleCoverage`의 i386 전제 | 미해결 |

항목 3을 열면 게스트가 실제로 돌기 시작하고, 그때 항목 5가 dynamic append에서
기다립니다.

## 아직 확인하지 않음

- 이 변경은 fault 경로가 **실제로 실행되는 상황**에서 검증되지 않았습니다.
  probe는 손으로 만든 `ucontext_t`를 왕복시키고, 실제 guest fault는 항목 3이
  열려야 발생합니다.
- x87 상태는 건드리지 않았습니다. x64 분기는 `_libc_fpstate`를 이미 다루지만
  guest 실행으로 검증된 적은 없습니다.

---

# Work log 20260903-577 — Guest `ESP` in the x64 fault path

Design: [20260903-577](../design/20260903-577-x64-guest-esp-context.md) ·
work order: [20260903-577](../work-orders/20260903-577-x64-guest-esp-context.md)

## Correction — half of section 3.20's item 2 was a wrong diagnosis

Section 3.20 recorded this item as "`Eip` and `Esp`, both wrong values". Reading
the code again and checking the run log, **`Eip` is not wrong.**

The engine treats a faulting `Eip` as **a cache address**, not a guest EIP: it
tests `IsAotCacheAddress(context, Eip)` and translates with
`AotGuestAddressForExecutionAddress`. The same holds on i386 — the code cache
holds guest bytes at a host address, so a fault inside it reports a cache address
there too.

For `REG_RIP`'s low 32 bits to be that cache address on x64, the cache must sit
below 4 GiB, and Task 576's real run confirms it:

```text
[loader] Win32 AOT cache base/bytes/entry: 0x20000000/326468/0x20000000
```

**This is the third assumption measurement has refuted this session** — Task
574's expected SIB byte, Task 575's address truncation, and now `Eip`. In all
three the error ran the same direction: not "the types match so the value must",
but the opposite — **what had been written down as wrong turned out to be
right.**

## Root cause — `Esp` is wrong in both directions

On i386 guest ESP and host ESP are one register, so reading `REG_UESP` is
correct. On x64 they are two: Task 546's decision 3 keeps host RSP as the SysV
stack and Task 558 puts guest ESP in `R15D`.

**Reading**: the engine spends `Esp` as a guest address — reading `[Esp+0x08]`
and `[Esp+0x10]` off the guest stack, storing it as `guest_return_esp`, testing
`in_range(Esp, ...)` against the guest arena. Host RSP is outside that arena.

**Writing is worse.** The engine **modifies** `Esp` (a `ZYDIS_REGISTER_ESP`
write, `RecoverToHost`). Sending that value back into `REG_RSP` **moves the host
stack pointer to a guest address**, and the kernel resumes on that context.

## Implementation

- Load: `registers->Esp = Register(machine, REG_R15);`
- Store: write `machine.gregs[REG_R15]`, **zero-extended**
- **Remove** the `machine.gregs[REG_RSP]` write

Zero-extension because of Task 558's invariant: `R15`'s upper half must be zero,
since an access through guest ESP is emitted as `[r15]` where all 64 bits are the
address, and the emitter keeps it so by writing only `lea r15d, ...`. `merge`
leaves the upper half as it found it, which **assumes the invariant rather than
maintaining it**.

## Verification — the check was tested before it was trusted

The probe used to assert `gregs[REG_RSP] == kEsp` on x64. It was **checking the
very assumption being changed.** It now asserts the new contract:

1. `Esp` round-trips through `R15`, compared as **all 64 bits**. Comparing only
   the low half would have passed an implementation that merged and left rubbish
   above.
2. A marker planted in `RSP` before the store (`0x00007FFF12345678`, above
   4 GiB) survives it.

Without the second, an implementation that still overwrote `RSP` would pass — so
**that was actually confirmed**. Temporarily restoring the `REG_RSP` write and
rebuilding gives:

```text
guest_cpu_context_round_trip=false
!! guest_cpu_context failed
```

and reverting passes again. That the check catches the old behaviour is an
observation, not an inference.

### Results

| Item | Result |
|---|---|
| Linux x64 `repiu_core_probe` | 20/20, 0 failures |
| Linux x64 `repiu_core_probe` (negative test) | `round_trip=false` — the check works |
| x64 `repiu` run (`pumpit2a`) | stops at the **same place** as Task 576, exit 0 |
| Linux i386 `repiu` link | succeeds |
| Linux i386 `repiu_core_probe` | 19/19, 0 failures, `guest_cpu_context_all=true` |
| Win32 `repiu_aot_probe` | 41 `_all=true`, 0 `_all=false` |

The x64 stopping point **must not move**: this unit does not start running a
guest, so a moved stop would be a regression. It stops in the same place.

```text
[loader] Win32 minimal execution message:
         minimal original entry execution requires a 32-bit host
```

## What is left

| # | Item | Status |
|---|---|---|
| 0 | The engine enabling long-mode emission | done (576) |
| 1 | Two symbols | done (575) |
| 2 | The fault path's `Esp` | **done** (`Eip` was right all along) |
| 3 | Guest entry (`return 4`) | open — **next** |
| 4 | The five dispatch thunks | not needed |
| 5 | `ValidateAotCodeCacheHleCoverage`'s i386 assumption | open |

Opening item 3 starts a guest actually running, and item 5 is waiting there on
the dynamic-append path.

## Not yet verified

- This change has not been verified with the fault path **actually executing**.
  The probe round-trips a hand-built `ucontext_t`; a real guest fault needs item
  3 open.
- x87 state was not touched. The x64 branch already handles `_libc_fpstate`, but
  it has never been verified against a running guest.
