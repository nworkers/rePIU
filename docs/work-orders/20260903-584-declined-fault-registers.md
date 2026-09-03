# 작업 지시 20260903-584 — 거절된 폴트의 레지스터를 찍는다

설계: [20260903-584](../design/20260903-584-declined-fault-registers.md)

## 변경 대상

| 파일 | 변경 |
|---|---|
| `src/engine/telemetry/fault_exit_trace.cpp` | `[repiu-regs]` 둘째 줄 |
| `docs/analysis/linux-x64-fault-context.md` | 신규 — x64 fault context의 세그먼트 제약 |
| `docs/analysis/README.md` | 색인 |
| `docs/analysis/linux-port-frontier.md` | 3.31절 |

## 구현 단계

1. `RecordFaultExit`에 둘째 줄을 더합니다. 같은 게이트·같은 상한 아래에 두고,
   첫 줄과 **같은 호출 안에서** 이어 찍습니다.
2. `access=`는 `fault.access.valid`가 참일 때만 값을 찍고, 거짓이면 `none`으로
   찍습니다.
3. GPR 8개, `eflags`, 그리고 platform이 실제로 주는 `cs`·`fs`·`gs`를 찍습니다.
   **`ds`·`es`·`ss`는 찍지 않습니다** — x64에서 합성된 0이므로 관측이 아닙니다.
4. 찍지 않는 이유를 소스 주석에 남깁니다. 그 줄을 다시 읽는 사람이 "왜 빠졌지"를
   묻게 두지 않습니다.
5. 게이트가 꺼져 있으면 어떤 실행도 한 줄도 달라지지 않아야 합니다.

## 검증 절차

1. 추적 없이 i386 `repiu`를 45초 돌려 `[repiu-exit]`·`[repiu-regs]` 0줄과
   `last_eip=0x010F2786`을 확인합니다.
2. `REPIU_FAULT_EXIT_TRACE=1`로 x64를 돌리고 두 줄을 그대로 기록합니다.
3. **`access`와 `esi`를 비교하고 결과를 기록합니다.** 이것이 이 단위의
   답입니다.
4. Linux i386 · x64 `repiu_core_probe`.
5. Win32 build, `repiu_core_probe`, `repiu_aot_probe`(pumpit1).

## 완료 조건

- `access == esi`인지 아닌지가 관측으로 기록됩니다.
- x64 fault context가 `DS`·`ES`·`SS`를 주지 않는다는 제약이 `docs/analysis/`에
  남습니다.
- 수정은 하지 않습니다.
- 작업 로그와 frontier 3.31절.

---

# Work order 20260903-584 — Print the registers of a declined fault

Design: [20260903-584](../design/20260903-584-declined-fault-registers.md)

## Files to change

| File | Change |
|---|---|
| `src/engine/telemetry/fault_exit_trace.cpp` | The second `[repiu-regs]` line |
| `docs/analysis/linux-x64-fault-context.md` | New — the x64 fault-context segment constraint |
| `docs/analysis/README.md` | Index entry |
| `docs/analysis/linux-port-frontier.md` | Section 3.31 |

## Implementation steps

1. Add the second line to `RecordFaultExit`, behind the same gate and limit, and
   emitted **within the same call** as the first.
2. Print `access=` only when `fault.access.valid`; otherwise print `none`.
3. Print the eight GPRs, `eflags`, and the `cs`/`fs`/`gs` the platform actually
   provides. **Do not print `ds`, `es` or `ss`** — on x64 they are synthesized
   zeros and not observations.
4. Record in a source comment why they are absent, so the next reader of that
   line is not left asking.
5. With the gate off, no run may differ by a line.

## Verification

1. Run i386 `repiu` for 45 seconds without the trace; confirm zero
   `[repiu-exit]` and `[repiu-regs]` lines and `last_eip=0x010F2786`.
2. Run x64 with `REPIU_FAULT_EXIT_TRACE=1` and record both lines verbatim.
3. **Compare `access` with `esi` and record the result.** That is this unit's
   answer.
4. Linux i386 and x64 `repiu_core_probe`.
5. Win32 build, `repiu_core_probe`, `repiu_aot_probe` (pumpit1).

## Completion criteria

- Whether `access == esi` is recorded as an observation.
- The constraint that the x64 fault context carries no `DS`/`ES`/`SS` is
  recorded under `docs/analysis/`.
- No repair.
- The work log and frontier section 3.31.
