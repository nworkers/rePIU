# Task 508 작업 지시 — 회수를 거절당한 종료

설계: [20260828-508](../design/20260828-508-refused-recovery-teardown.md) ·
작업 로그: [20260828-508](../work-logs/20260828-508-refused-recovery-teardown.md)

## 0. 시작 전에 — 거절을 먼저 재현하십시오

**회수에 성공하는 실행에서는 이 문제가 나타나지 않습니다.** 20초 예산으로는 게스트가 자산
디코드 중이라 거의 항상 회수됩니다. 거절을 보려면 **렌더 루프에 들어간 뒤**에 종료를
요청해야 합니다 — 그때 게스트는 프레임의 상당 부분을 Glide 게이트 안에서 보냅니다.

```bash
bash build/task508-repro.sh 3 60000     # 60초 예산, 3회
```

`[repiu-shutdown]` 줄에서 `stopped=0`을 찾으십시오. 그 줄이 있는 실행의 종료 상태가
`133`(128+SIGTRAP)이면 재현된 것입니다. **`stopped=1`만 나오면 아직 재현이 아닙니다** —
예산을 늘리거나 반복하십시오.

## 1. 종료 블록을 두 갈래로 나누십시오

`src/platform/win32/execution/execution_trampoline.cpp`의 종료 블록, `hotspot-dump` 바로
뒤입니다.

| 조건 | 무엇을 하나 |
|---|---|
| `gracefully_interrupted == false` | `probe-dump` → `DetachHostThread` → `_Exit`. **그게 전부입니다** |
| 그 외 | 지금의 순서 그대로 (`glide-close` … `done`) |

`remove_vectored_handler()`를 **부르지 마십시오.** 이것이 작업의 전부이고 나머지는 그 결정의
결과입니다. 핸들러가 남아 있으면 계속 도는 게스트 스레드의 INT3과 단일 스텝은 평소처럼
처리되고, 프로세스는 `_Exit`으로 끝납니다.

`attempt->guest_thread_stopped = false`를 이 갈래 안에서 세우십시오. `main.cpp`의 AOT 캐시
해제 게이트가 읽는 필드이고, `_Exit` 때문에 실제로는 닿지 않지만 **그 방어를 지우지
마십시오** — 507이 그 자리에 둔 이유가 그대로 유효합니다.

## 2. 단계 표시를 유지하십시오

`mark_shutdown_step`은 이 경로에서 유일하게 남는 기록입니다. 새 갈래에도
`probe-dump`와 새 표시 하나(`immediate-exit`)를 남기십시오. **표시 없이 `_Exit`하지
마십시오** — 그러면 다음 사람이 "어디서 죽었는가"를 다시 처음부터 찾습니다.

## 3. 주석이 이 작업의 절반입니다

`remove_vectored_handler()`가 왜 이 갈래에서 호출되지 않는지를 **호출되지 않는 자리**에
적으십시오. 다음 사람이 "정리를 빼먹었다"고 읽고 되돌리기 쉬운 모양이기 때문입니다. 적을
것은 하나입니다 — **아직 그 핸들러를 필요로 하는 스레드가 돌고 있다.**

## 4. 검증

### Linux

| 항목 | 기준 |
|---|---|
| `repiu` i386 빌드 | 성공 |
| `repiu_core_probe` | `core_probe_total=15`, 실패 0 |
| DOS/4GW 샘플 (`legacy`/`dynamic`) | exit 2, focus 0x10, opcode 0x80 — 3d-19 기준선 |
| **60초 예산 3회 이상** | `stopped=0`인 실행에서 **SIGTRAP 0회**, 종료 상태가 예산 만료 코드 |
| 20초 예산 5회 | 회귀 없음 (회수 성공 경로가 그대로 정상 종료) |
| SIGTERM 3회 | 프로세스가 스스로 종료 |

### Windows

**이번에는 실행할 수 있습니다.** 506·507은 WSL 안에서 작업해 `powershell.exe`를 부를 수
없었지만, Windows 호스트에서 작업한다면 그 제약이 없습니다.

| 항목 | 기준 |
|---|---|
| Debug 빌드 | 성공 |
| `repiu_core_probe` | 실패 0 |
| 종료 경로 회귀 | 예산 만료 실행이 지금과 같은 종료 코드·메시지 |

**Windows에서 이 갈래는 `TerminateThread`가 실패해야 닿습니다.** 회귀에서 확인할 것은 새
갈래가 아니라 **기존 갈래가 그대로인가**입니다.

## 5. 문서

* frontier 6절의 "종료 시 SIGTRAP" 행을 해결로 옮기고, 남은 것(핸들러 미반환)과 구분하십시오.
* `linux-shutdown-check` 가이드에 **`stopped=0`을 어떻게 만드는가**를 넣으십시오. 이 작업에서
  가장 시간을 쓴 것이 거절 재현이고, 절차에 없으면 다음 사람이 같은 시간을 씁니다.

---

# Task 508 work order — the refused-recovery shutdown

Design: [20260828-508](../design/20260828-508-refused-recovery-teardown.md) ·
Work log: [20260828-508](../work-logs/20260828-508-refused-recovery-teardown.md)

## 0. Before starting — reproduce the refusal first

**A run whose recovery succeeds does not show this at all.** With a 20-second budget the guest is
still decoding assets and is recovered nearly every time. Seeing a refusal means asking to stop
**after the render loop has been entered**, where the guest spends much of a frame inside the Glide
gate.

```bash
bash scripts/task508_refused_recovery_repro.sh 3 60000 task508
```

Look for `stopped=0` on the `[repiu-shutdown]` line. If such a run's exit status is `133`
(128 + SIGTRAP), it has reproduced. **`stopped=1` everywhere is not yet a reproduction** -- raise the
budget or repeat.

## 1. Split the shutdown block in two

In `src/platform/win32/execution/execution_trampoline.cpp`, in the shutdown block, immediately after
`hotspot-dump`.

| Condition | What runs |
|---|---|
| `gracefully_interrupted == false` | `probe-dump`, `DetachHostThread`, `_Exit`. **That is all** |
| otherwise | today's sequence unchanged (`glide-close` … `done`) |

**Do not call `remove_vectored_handler()`** on that arm. That is the whole of the task; everything
else follows from it. With the handler still installed, the INT3s and single steps of a guest thread
that keeps running are handled as they always were, and the process ends at `_Exit`.

Set `attempt->guest_thread_stopped = false` inside that arm. It is the field `main.cpp` reads to gate
the AOT cache release, and although `_Exit` means it is never actually reached, **do not delete that
defence** -- 507's reason for putting it there still holds.

## 2. Keep the step markers

`mark_shutdown_step` is the only record left on this path. Emit `probe-dump` and one new marker
(`immediate-exit`) on the new arm too. **Do not `_Exit` without a marker** -- that makes the next
person work out "where did it die" from scratch again.

## 3. The comment is half the task

Write down why `remove_vectored_handler()` is *not* called, **at the place it is not called**,
because the shape is an easy one for the next reader to take for a forgotten cleanup and restore.
There is one thing to say: **a thread that still needs that handler is still running.**

## 4. Verification

### Linux

| Item | Criterion |
|---|---|
| `repiu` i386 build | succeeds |
| `repiu_core_probe` | `core_probe_total=15`, zero failures |
| The DOS/4GW sample (`legacy`, `dynamic`) | exit 2, focus 0x10, opcode 0x80 — 3d-19's baseline |
| **Three or more 60-second-budget runs** | **zero SIGTRAP** among the `stopped=0` runs, and the exit status is the budget-expiry code |
| Five 20-second-budget runs | no regression: the successful-recovery path still ends normally |
| Three SIGTERM runs | the process ends by itself |

### Windows

**This time it can be run.** Tasks 506 and 507 worked from inside WSL and could not invoke
`powershell.exe`; working from the Windows host removes that constraint.

| Item | Criterion |
|---|---|
| Debug build | succeeds |
| `repiu_core_probe` | zero failures |
| Shutdown-path regression | a budget-expiry run gives the same exit code and message as today |

**On Windows this arm is reached only if `TerminateThread` failed.** What the regression checks is
therefore not the new arm but **that the existing one is unchanged**.

## 5. Documentation

* Move the frontier's section 6 row for "a SIGTRAP on teardown" to resolved, and keep it distinct
  from what remains (the handler that does not return).
* Add **how to produce `stopped=0`** to the `linux-shutdown-check` guide. Reproducing the refusal is
  what took the longest in this task, and without it in the procedure the next person spends the same
  time again.
