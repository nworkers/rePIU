# Task 507 작업 지시 — Linux 종료 경로

설계: [20260827-507](../design/20260827-507-linux-shutdown-recovery.md) ·
작업 로그: [20260827-507](../work-logs/20260827-507-linux-shutdown-recovery.md)

## 0. 시작 전에 — 사슬을 다시 확인하십시오

설계가 확정한 정지 사슬은 이렇습니다. **먼저 재현하고 시작하십시오.** 다르면 코드가 움직인
것이고 이 지시서가 낡은 것입니다.

```bash
grep -rn "NO_SIGNAL_HANDLERS" src/            # 결과 없음 = SDL이 TERM을 가져감
grep -rn "requires Win32" src/                # 결과 없음 (506이 지웠음)
sed -n '4953,5062p' src/platform/win32/execution/execution_trampoline.cpp
```

블록의 본문이 `#if defined(_WIN32)`이고, 끝에서 `CloseHostThread`를 부르며, POSIX의
`CloseHostThread`가 조건 없이 `pthread_join` 한다는 세 가지를 눈으로 확인하십시오. **이 셋이
같이 있어야 정지가 설명됩니다.**

## 1. `DetachHostThread`를 계층에 더하십시오

`include/repiu/platform/host_thread.h`에 선언하고 `src/platform/host_thread.cpp`에 구현합니다.

* Windows: `CloseHandle`.
* POSIX: `pthread_detach` 후 **레코드를 지우지 마십시오.** 살아 있는 스레드가 끝날 때 그
  레코드에 씁니다.

**헤더 주석이 이 작업의 절반입니다.** `CloseHostThread` 바로 아래에 두고, 두 함수가 서로를
가리키게 하십시오 — 하나는 "스레드가 끝났음을 확인한 caller"의 것이고, 다른 하나는 "확인할 수
없는 caller"의 것입니다. 그 구분이 없으면 다음 사람이 `CloseHostThread`의 join을 "버그"로 읽고
지웁니다. 그 join은 옳습니다.

## 2. 종료 블록을 `InterruptHostThread`로 옮기십시오

`execution_trampoline.cpp`의 종료 블록입니다.

| 지금 | 바꿀 것 |
|---|---|
| `SuspendThread` / `GetThreadContext` / `SetThreadContext` / `ResumeThread` | `InterruptHostThread` 한 번 |
| 콜백 자리의 `context.hle_message = ...` | **요청한 스레드로 올릴 것** |
| EIP 검사 | **그대로 둘 것** |

콜백에 넘길 것은 작은 구조체 하나입니다 — `ThreadContext*`, 스냅샷을 채울지 여부, 채울 대상,
그리고 회수했는지 돌려줄 `bool`.

**콜백 안에서 할당하지 마십시오.** Linux에서 이 콜백은 대상 스레드의 시그널 핸들러 안에서
돕니다. `std::string` 대입 하나가 malloc 잠금을 잡고 있는 스레드 자신 위에서 다시 malloc을
부르는 일이 됩니다. 헤더가 굵게 적어 둔 경고이고, 이 작업이 그 경고의 첫 편집 사용자입니다.

시한은 짧게 잡으십시오. 3d-21이 표본기에 쓴 200 ms가 기준이고, **답하지 않는 것 자체가
소견입니다** — 그때는 회수하지 못한 것으로 처리하고 다음으로 갑니다.

## 3. 기다리지 않고 내려가십시오

블록 끝의 정리를 이렇게 나눕니다.

| 스레드가 멈췄나 | 무엇을 부르나 |
|---|---|
| 예 (회수 후 join 성공, 또는 Windows에서 `TerminateThread` 후 join 성공) | `CloseHostThread` |
| 아니오 | `DetachHostThread` |

`TerminateThread` 최후 수단은 **Windows 안에 그대로 둡니다.** 대응물을 만들지 않는다는 것이
3d-18의 결정이고 이 작업은 그것을 바꾸지 않습니다.

Linux에서 회수하지 못한 경우 남길 메시지는 사실 그대로여야 합니다 — 무엇이 종료를 요청했고,
게스트 스레드가 멈추지 않았다는 것. **"정리했다"고 적지 마십시오.**

## 4. `_M_IX86` 하나

`live_telemetry_snapshot.cpp`의 `CopySnapshotFromContextRecord`를
`defined(_M_IX86) || defined(__i386__)`로 맞춥니다. `native_phase_sampler.cpp`가 이미 쓰는
형태입니다.

**나머지 `_M_IX86` 자리는 건드리지 마십시오.** 설계가 목록으로 남겼고, 별도 단위입니다.

## 5. 검증

### Linux

| 대상 | 기준 |
|---|---|
| i386 빌드 | `repiu` 링크까지 성공 |
| `repiu_core_probe` | `core_probe_total=15 failures=0` |
| DOS/4GW 샘플 (`legacy`, `dynamic`) | exit 2, 초점 0x10, opcode 0x80 |
| **`pumpit1` 예산 만료** | `REPIU_EXECUTION_TIMEOUT_MS` 실행이 스스로 종료 |
| **`pumpit1` SIGTERM** | TERM 뒤 스스로 종료 |

실행 시 유의(505·506에서 확립):

* **저장소 루트에서** 실행합니다. 로더가 `roms`·`build/runtime_mounts`를 상대 경로로 찾습니다.
* 종료 시험에서는 `REPIU_STALL_TIMEOUT_MS=0`으로 감시견을 끄십시오. 감시견이 먼저 끝내면
  **무엇을 시험한 것인지 말할 수 없게 됩니다.**
* 종료했다는 것을 종료 코드 하나로 판정하지 마십시오. **프로세스가 사라졌는지**를 보십시오 —
  `wait` 후 `kill -0`이 실패해야 합니다.

### Windows

가능하면 Debug 빌드와 `repiu_core_probe`·`repiu_aot_probe`를 돌리고, WSL interop이 막혀
불가능하면 **사유를 로그에 적으십시오.** 506이 같은 이유로 미수행을 남겼습니다.

## 6. 완료 조건

**종료를 요청한 Linux 실행이 스스로 끝납니다.** 회수에 성공한 실행도, 회수가 거절된 실행도
끝나야 합니다. 회수 성공 한 줄은 완료 조건이 아닙니다.

---

# Task 507 Work Order — The Linux shutdown path

Design: [20260827-507](../design/20260827-507-linux-shutdown-recovery.md) ·
Work log: [20260827-507](../work-logs/20260827-507-linux-shutdown-recovery.md)

## 0. Before starting — confirm the chain again

This is the stall chain the design established. **Reproduce it first**; if it differs, the code has
moved and this order is stale.

```bash
grep -rn "NO_SIGNAL_HANDLERS" src/            # nothing = SDL takes TERM
grep -rn "requires Win32" src/                # nothing (506 removed them)
sed -n '4953,5062p' src/platform/win32/execution/execution_trampoline.cpp
```

See all three with your own eyes: the block's body is `#if defined(_WIN32)`, it ends by calling
`CloseHostThread`, and POSIX's `CloseHostThread` joins unconditionally. **The stall is only explained
by the three together.**

## 1. Add `DetachHostThread` to the layer

Declared in `include/repiu/platform/host_thread.h`, implemented in `src/platform/host_thread.cpp`.

* Windows: `CloseHandle`.
* POSIX: `pthread_detach`, and **do not delete the record** — a thread still alive writes into it
  when it ends.

**The header comment is half this task.** Put it directly below `CloseHostThread` and have the two
point at each other: one is for a caller that has established the thread exited, the other for a
caller that cannot. Without that distinction the next reader takes `CloseHostThread`'s join for a bug
and removes it. That join is correct.

## 2. Move the shutdown block onto `InterruptHostThread`

In `execution_trampoline.cpp`:

| Today | Replace with |
|---|---|
| `SuspendThread` / `GetThreadContext` / `SetThreadContext` / `ResumeThread` | one `InterruptHostThread` |
| `context.hle_message = ...` inside that region | **move it up to the requesting thread** |
| The EIP test | **keep it** |

Pass the callback one small structure: the `ThreadContext*`, whether to fill a snapshot, where, and a
`bool` reporting whether recovery happened.

**Do not allocate inside the callback.** On Linux it runs inside the target thread's signal handler,
where one `std::string` assignment means calling malloc on top of a thread that may be holding
malloc's own lock. The header warns about this in bold, and this task is the first editing caller.

Keep the deadline short. 3d-21's sampler uses 200 ms, and **an answer that does not arrive is itself
a finding**: treat it as recovery not performed and move on.

## 3. Go down without waiting

Split the cleanup at the end of the block:

| Did the thread stop? | Call |
|---|---|
| Yes (recovered and joined, or on Windows terminated and joined) | `CloseHostThread` |
| No | `DetachHostThread` |

The `TerminateThread` last resort **stays inside the Windows branch.** Building no counterpart is
3d-18's decision and this task does not revisit it.

When Linux could not recover, the message must say what happened — what asked for shutdown, and that
the guest thread did not stop. **Do not write that it was cleaned up.**

## 4. One `_M_IX86`

Bring `CopySnapshotFromContextRecord` in `live_telemetry_snapshot.cpp` to
`defined(_M_IX86) || defined(__i386__)`, the form `native_phase_sampler.cpp` already uses.

**Leave the other `_M_IX86` sites alone.** The design lists them; they are their own unit.

## 5. Verification

### Linux

| Target | Criterion |
|---|---|
| i386 build | links as far as `repiu` |
| `repiu_core_probe` | `core_probe_total=15 failures=0` |
| The DOS/4GW sample (`legacy`, `dynamic`) | exit 2, focus 0x10, opcode 0x80 |
| **`pumpit1`, budget expiry** | a `REPIU_EXECUTION_TIMEOUT_MS` run exits by itself |
| **`pumpit1`, SIGTERM** | the process exits by itself after TERM |

When running (established in 505 and 506):

* Run **from the repository root**; the loader resolves `roms` and `build/runtime_mounts` relatively.
* Disable the watchdog with `REPIU_STALL_TIMEOUT_MS=0` for the shutdown tests. If the watchdog ends
  the run first, **there is no saying what was tested.**
* Do not judge "it exited" from an exit code. Check that **the process is gone**: after the wait,
  `kill -0` must fail.

### Windows

Run the Debug build with `repiu_core_probe` and `repiu_aot_probe` if possible; if WSL interop blocks
it, **record the reason in the log**, as 506 did for the same reason.

## 6. Completion criteria

**A Linux run asked to stop stops by itself** — both the run that was recovered and the run whose
recovery was refused. One line reporting a successful recovery is not the criterion.
