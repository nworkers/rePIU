# 설계 20260903-575 — Linux x64 `repiu` 링크

## 목적

x64 `repiu` 실행 파일을 만듭니다. 3.20절 측정에서 링크가 **심볼 두 개**에서
멈추는 것이 확인되었고, 이 단위는 그 둘만 해결합니다.

```text
undefined reference to `RecoverGuestStackException'
undefined reference to `RecoverHostStackException'
```

**게스트가 돌게 만드는 단위가 아닙니다.** 그것은 3.20절 표의 항목 2와 3이고,
이 단위는 항목 1입니다.

## 왜 링크가 목표로서 의미가 있는가

지금까지 x64의 진척은 전부 census의 **정적 측정**입니다. 실행 파일이 생기면
Linux i386이 Task 508 이후 그랬던 것처럼 **실제로 돌려서** 재기 시작할 수
있습니다 — 종료 코드, 로더 로그, 어디까지 갔는지가 정적 분석이 아니라 관측이
됩니다.

## 두 심볼의 계약

둘 다 **호출되지 않습니다.** fault handler가 `Eip`에 주소를 심어서, 폴트에서
resume할 때 그 자리에서 실행되게 하는 복귀 지점입니다.

| 심볼 | i386 동작 |
|---|---|
| `RecoverGuestStackException` | host segment register를 복원하고 `CallGuestEntryWithStack`의 꼬리(pop 4개 + `ret 4`)를 실행해 `RECOVERED`를 반환 |
| `RecoverHostStackException` | `xor eax, eax` · `ret`. 스택 전환이 없었으므로 되돌릴 것이 없고 정의된 반환만 갚음 |

`REPIU_THUNK_RESOLVER_CALL`은 x64에서 아무것도 아닌 것으로 확장되므로
(`thunk_calling_convention.h`), x64 정의는 호출 규약을 따질 필요가 없습니다.

## 설계 결정 1 — x64 정의는 복귀가 아니라 `ud2`입니다

x64에서 이 둘이 **도달 가능하지 않다**는 것이 이 결정의 근거입니다. 두 곳 모두
`use_guest_stack` 분기 안이거나 guest thread가 도는 중에만 닿는 fault 경로인데,
`GuestEntryThreadProc`이 non-i386에서 `return 4`이므로(Task 544) x64에서는 guest
thread가 시작되지 않습니다.

그러면 x64 정의는 두 가지 중 하나여야 합니다.

- **조용히 돌아가는 것** — i386 동작을 흉내 내 `ret`합니다. 도달했을 때
  "복구된 것처럼" 보이고 프레임은 쓰레기입니다. 이 프로젝트가 반복해서 겪은
  실패 형태이고 받아들이지 않습니다.
- **크게 죽는 것** — `ud2`. 도달 자체가 결함이므로 그 자리에서 끝냅니다.

후자를 택합니다. `ud2`는 스택 정렬도 ABI도 요구하지 않고, core dump에서 주소가
그대로 남습니다.

fault handler가 `SIGILL`을 다루지만(`kHandledSignals`), x64에서는 그 handler가
설치되는 경로 자체가 guest entry 안이라 설치되지 않습니다. 설치된 상태에서
닿더라도 callback이 처리하지 않으면 handler가 `SIG_DFL`을 복원하고 돌아가
`ud2`가 재실행되며 기본 동작으로 죽습니다 — 무한 루프가 아닙니다.

## 설계 결정 2 — 별도 파일에 두고 CMake에서 x64로 게이트합니다

`src/platform/linux/guest_stack_recover_x64.S`를 새로 만들고
`if(CMAKE_SIZEOF_VOID_P GREATER 4)` 블록에 넣습니다. Task 562가
`aot_dbt_return_thunk_x64.S`를 같은 자리에 넣은 것과 같은 구조입니다.

i386 `guest_stack_switch.S`에 `#if`로 x64 분기를 넣지 않습니다. 그 파일은 32비트
실행 계약이고, Task 545가 그것을 x64 빌드에서 통째로 분리한 판단을 뒤집을 이유가
없습니다.

## 이 단위가 고치지 않는 것 — 그리고 그것이 왜 중요한가

링크가 되어도 **복구 경로는 x64에서 옳지 않습니다.** 주소를 심는 코드가
이렇습니다.

```cpp
context->Eip = static_cast<RegisterField>(
    reinterpret_cast<std::uintptr_t>(&RecoverGuestStackException));
```

`context->Eip`는 32비트입니다. x64에서 함수 주소는 64비트이므로 이 대입은
**잘림**이고, 이어서 `guest_cpu_context.cpp`의
`machine.gregs[REG_RIP] = merge(gregs[REG_RIP], registers.Eip)`가 상위 32비트를
폴트 지점의 것으로 채웁니다. 두 주소의 상위 절반이 다르면 결과는 쓰레기입니다.

이것은 3.20절 항목 2와 **같은 부류의 문제**이고 같은 단위에서 다뤄야 합니다.
이번 단위는 그것을 고치지 않으며, `ud2`를 택한 이유의 절반이 이것입니다 —
잘린 주소로 뛰어도 그 자리에 있는 것은 복구가 아니라 정지입니다.

## 검증

1. **x64 링크** — `build_linux_x64.sh --target repiu`가 실행 파일을 만들어야
   합니다. 미해결 심볼 0.
2. **x64 실행** — 만들어진 `repiu`를 실제로 돌려 관측합니다. 게스트는 돌지
   않아야 하고(Task 544의 `return 4`), 그 거절이 로그에 나타나야 합니다.
   **이것이 이 단위가 여는 새 측정 수단입니다.**
3. **i386 회귀** — `repiu` i386 링크와 `repiu_core_probe`가 그대로여야 합니다.
   새 파일은 x64 게이트 안이므로 i386 빌드에 들어가지 않습니다.
4. **Win32 회귀** — `repiu_aot_probe`.

---

# Design 20260903-575 — Linking `repiu` on Linux x64

## Objective

Produce an x64 `repiu` executable. Section 3.20's measurement found the link
stopping at **two symbols**, and this unit resolves only those.

```text
undefined reference to `RecoverGuestStackException'
undefined reference to `RecoverHostStackException'
```

**This is not the unit that makes a guest run.** That is items 2 and 3 of
section 3.20's table; this is item 1.

## Why linking is a worthwhile goal on its own

Every x64 result so far is the census's **static** measurement. An executable
means measurement can come from **running it**, as it has on Linux i386 since
Task 508 — exit codes, loader logs, and how far it gets become observations
rather than static analysis.

## What the two symbols promise

Neither is **called**. They are recovery points whose addresses the fault
handler plants in `Eip`, so that resuming from the fault runs them.

| Symbol | What it does on i386 |
|---|---|
| `RecoverGuestStackException` | Restores the host segment registers, then runs the tail of `CallGuestEntryWithStack` (four pops and `ret 4`), returning `RECOVERED` |
| `RecoverHostStackException` | `xor eax, eax` · `ret`. No stack switch was made, so nothing is undone and only a defined return is owed |

`REPIU_THUNK_RESOLVER_CALL` expands to nothing on x64
(`thunk_calling_convention.h`), so the x64 definitions need no calling-convention
attribute.

## Decision 1 — the x64 definitions are `ud2`, not a recovery

The basis is that neither is **reachable** on x64. Both sites sit inside a
`use_guest_stack` branch or a fault path reached only while a guest thread runs,
and `GuestEntryThreadProc` returns 4 on non-i386 (Task 544), so no guest thread
starts on x64.

That leaves two possible x64 definitions:

- **One that quietly returns** — imitating the i386 behaviour with a `ret`.
  Reaching it would look like a recovery while the frame is rubbish. That is the
  failure shape this project keeps meeting, and it is not accepted here.
- **One that dies loudly** — `ud2`. Arriving at all is the defect, so it ends
  there.

The latter is chosen. `ud2` needs no stack alignment and no ABI, and the address
survives into a core dump.

The fault handler does handle `SIGILL` (`kHandledSignals`), but on x64 the path
that installs it is inside the guest entry and never runs. Even installed,
a callback that does not claim the fault makes the handler restore `SIG_DFL` and
return, so the `ud2` re-executes and dies by default action — not a loop.

## Decision 2 — a separate file, gated to x64 in CMake

`src/platform/linux/guest_stack_recover_x64.S` is new and goes in the
`if(CMAKE_SIZEOF_VOID_P GREATER 4)` block, the same structure Task 562 used for
`aot_dbt_return_thunk_x64.S`.

No `#if` branch is added inside the i386 `guest_stack_switch.S`. That file is a
32-bit execution contract, and there is no reason to reverse Task 545's decision
to separate it from the x64 build wholesale.

## What this unit does not fix — and why that matters

Linking does not make the recovery path correct on x64. The code planting the
address reads:

```cpp
context->Eip = static_cast<RegisterField>(
    reinterpret_cast<std::uintptr_t>(&RecoverGuestStackException));
```

`context->Eip` is 32 bits. A function address on x64 is 64, so that assignment
**truncates**, and `guest_cpu_context.cpp` then does
`machine.gregs[REG_RIP] = merge(gregs[REG_RIP], registers.Eip)`, filling the top
half from the faulting site. Where the two addresses differ above bit 31 the
result is rubbish.

This is **the same class of problem** as section 3.20's item 2 and belongs in
that unit. This one does not fix it, and that is half the reason for `ud2`:
jumping to a truncated address should find a stop, not a recovery.

## Verification

1. **x64 link** — `build_linux_x64.sh --target repiu` must produce an
   executable, with zero unresolved symbols.
2. **x64 run** — actually run the resulting `repiu` and observe. The guest must
   not run (Task 544's `return 4`) and that refusal must appear in the log.
   **This is the new measurement instrument the unit opens.**
3. **i386 regression** — the i386 `repiu` link and `repiu_core_probe` must be
   unchanged. The new file is inside the x64 gate and does not enter the i386
   build.
4. **Win32 regression** — `repiu_aot_probe`.
