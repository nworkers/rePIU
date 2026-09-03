# 작업 지시 20260903-582 — 폴트가 어디로 빠져나가는지 이름 붙이기

설계: [20260903-582](../design/20260903-582-fault-exit-attribution.md)

## 변경 대상

| 파일 | 변경 |
|---|---|
| `include/repiu/engine/veh_exit_site.h` | 값 두 개 추가와 이름 두 개 |
| `src/engine/telemetry/fault_exit_trace.h` | 신규 — 게이트와 기록 함수 |
| `src/engine/telemetry/fault_exit_trace.cpp` | 신규 — 게이트, 상한, 출력 |
| `CMakeLists.txt` | 새 source 등록 |
| `src/engine/execution/execution_trampoline.cpp` | 조기 반환 두 곳 표시, `GuestThreadFaultCallback` 출력 |
| `docs/analysis/linux-port-frontier.md` | 3.29절 |

## 구현 단계

1. `veh_exit_site.h` 끝에 `kForeignThread`와 `kGuestStackNotEntered`를
   **추가**합니다. 기존 값을 재정렬하지 않습니다 — 값이 `ThreadContext`에
   기록되고 host report가 찍는 안정 식별자입니다.
2. `kVehExitSiteCount`가 새 마지막 값을 기준으로 계산되는지 확인합니다.
3. `VehExitSiteName`에 두 이름을 더합니다. `foreign-thread`,
   `guest-stack-not-entered`.
4. `DispatchGuestFault`의 thread id 검사와 `use_guest_stack` 검사에서, 반환
   **직전에** `NoteVehExitSite`로 각각을 표시합니다. 회전 블록은 옮기지
   않습니다.
5. `fault_exit_trace.{h,cpp}`에 `REPIU_FAULT_EXIT_TRACE` 게이트와
   `RecordFaultExit(context, fault)`를 만듭니다. 게이트를 가장 먼저 확인하고,
   전체 상한까지만 찍습니다.
6. `GuestThreadFaultCallback`에서 `disposition != kResume`일 때 호출합니다.
   `RecoverToHost`보다 **앞**이어야 합니다 — 복구가 상태를 바꾸기 전의 값을
   찍어야 하기 때문입니다.
7. 게이트가 꺼져 있으면 어떤 실행도 **한 줄도** 달라지지 않아야 합니다.

## 검증 절차

1. 추적 없이 i386 `repiu`를 `pumpit2a`로 돌리고 `[repiu-exit]` 0줄과 Task 581과
   같은 진행을 확인합니다.
2. `REPIU_FAULT_EXIT_TRACE=1`로 x64 `repiu`를 돌리고 죽기 직전의
   `[repiu-exit]` 줄을 그대로 기록합니다. **`site=`가 이 단위의 답입니다.**
3. 같은 추적을 i386에서 켜고, `REPIU_GUEST_WATCH=0x010F1728`을 함께 걸어 그
   `sti` 폴트가 `[repiu-exit]`를 내지 않는 것을 확인합니다(대조군).
4. Linux i386 · x64 `repiu_core_probe`.
5. Win32 x86 Debug 빌드와 `repiu_core_probe`, `repiu_aot_probe`(pumpit1).

## 완료 조건

- **x64가 폴트를 거절하는 지점에 이름이 붙고, 관측으로 기록됩니다.**
- 설계가 적은 세 후보 중 어느 것인지가 정해지거나, `kUnknown`이면 다음에 이름
  붙일 곳이 정해집니다.
- 수정은 하지 않습니다.
- 작업 로그와 frontier 3.29절.

---

# Work order 20260903-582 — Naming where a fault leaves

Design: [20260903-582](../design/20260903-582-fault-exit-attribution.md)

## Files to change

| File | Change |
|---|---|
| `include/repiu/engine/veh_exit_site.h` | Two appended values and their names |
| `src/engine/telemetry/fault_exit_trace.h` | New — gate and record function |
| `src/engine/telemetry/fault_exit_trace.cpp` | New — gate, limit, output |
| `CMakeLists.txt` | Register the new source |
| `src/engine/execution/execution_trampoline.cpp` | Tag the two early returns; print in `GuestThreadFaultCallback` |
| `docs/analysis/linux-port-frontier.md` | Section 3.29 |

## Implementation steps

1. **Append** `kForeignThread` and `kGuestStackNotEntered` at the end of
   `veh_exit_site.h`. Do not reorder existing values — they are stable
   identifiers written into `ThreadContext` and printed by the host report.
2. Confirm `kVehExitSiteCount` is computed from the new last value.
3. Add both names to `VehExitSiteName`: `foreign-thread`,
   `guest-stack-not-entered`.
4. Tag each of `DispatchGuestFault`'s thread-id check and `use_guest_stack`
   check with `NoteVehExitSite` **immediately before** returning. Do not move
   the rotation block.
5. Build `fault_exit_trace.{h,cpp}` with the `REPIU_FAULT_EXIT_TRACE` gate and
   `RecordFaultExit(context, fault)`. Check the gate first; print up to a whole
   trace limit.
6. Call it from `GuestThreadFaultCallback` when `disposition != kResume`, and
   **before** `RecoverToHost` — the values must be read before recovery changes
   them.
7. With the gate off, no run may differ by **a single line**.

## Verification

1. Run the i386 `repiu` on `pumpit2a` without the trace; confirm 0
   `[repiu-exit]` lines and the same progress Task 581 recorded.
2. Run the x64 `repiu` with `REPIU_FAULT_EXIT_TRACE=1` and record the
   `[repiu-exit]` line verbatim just before it dies. **Its `site=` is this
   unit's answer.**
3. Turn the same trace on for i386 alongside `REPIU_GUEST_WATCH=0x010F1728` and
   confirm the `sti` fault produces no `[repiu-exit]` — the control.
4. Linux i386 and x64 `repiu_core_probe`.
5. Win32 x86 Debug build with `repiu_core_probe` and `repiu_aot_probe`
   (pumpit1).

## Completion criteria

- **The point where x64 declines the fault has a name, recorded as an
  observation.**
- Either one of the design's three candidates is settled, or `kUnknown` settles
  where the next name goes.
- No repair.
- The work log and frontier section 3.29.
