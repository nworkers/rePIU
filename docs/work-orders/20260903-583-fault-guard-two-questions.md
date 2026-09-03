# 작업 지시 20260903-583 — 가드가 실제로 묻는 것을 묻게 한다

설계: [20260903-583](../design/20260903-583-fault-guard-two-questions.md)

## 변경 대상

| 파일 | 변경 |
|---|---|
| `src/engine/execution/thread_context.h` | `cache_entry_active` 불리언 하나 |
| `src/engine/execution/execution_trampoline.cpp` | 진입 가드, 포기 지점 두 곳, x64 thread proc 설정·해제 |
| `src/platform/linux/guest_stack_recover_x64.S` | 낡은 도달 불가 주석 정정(주석만) |
| `docs/analysis/linux-port-frontier.md` | 3.30절 |

## 구현 단계

1. `ThreadContext`에 `bool cache_entry_active = false;`를 더합니다. 무엇을
   뜻하는지 주석으로 남깁니다 — "x64 cache 진입으로 게스트가 실행 중".
2. `GuestCacheEntryThreadProc`에서 `CallGuestCacheEntryTimed` **앞뒤로**
   설정·해제합니다. 해제는 이른 반환 경로에서도 빠지지 않아야 합니다.
3. 진입 가드를 `guest_stack_entered`와 `cache_entry_active`를 아는 형태로
   다시 씁니다. **i386에서 새 항은 항상 거짓이어야 합니다.**
4. 4028행과 4378행에서 `active_call_state == nullptr`이면 역참조 대신
   `kNotHandled`를 반환합니다. `NoteVehExitSite`로 표시해 Task 582의 추적이
   그 자리도 이름으로 부를 수 있게 합니다.
5. 새 exit site 이름이 필요하면 `veh_exit_site.h`에 **추가**합니다(재정렬 금지).
6. `guest_stack_recover_x64.S`의 주석에서 "no guest thread starts on x64"를
   Task 578 이후의 사실로 정정합니다. `ud2` 본문은 건드리지 않습니다.

## 검증 절차

1. **i386 회귀가 먼저입니다.** `pumpit2a`를 45초 돌려 `last_eip=0x010F2786`과
   Task 582와 같은 규모의 `single_step`·`aot`를 확인하고,
   `REPIU_FAULT_EXIT_TRACE=1`에서 `[repiu-exit]` 0줄을 확인합니다.
2. x64를 `REPIU_FAULT_EXIT_TRACE=1 REPIU_GUEST_WATCH=0x010F1728`로 돌리고
   출력 전부를 기록합니다.
3. `site=guest-stack-not-entered`가 사라졌는지 확인합니다.
4. `event=priv`가 나오는지 확인합니다. 나오지 않으면 **어디서 멈췄는지**를
   기록합니다 — 그것이 이 단위의 산출물입니다.
5. Linux i386 · x64 `repiu_core_probe`.
6. Win32 configure·build, `repiu_core_probe`, `repiu_aot_probe`(pumpit1).

## 완료 조건

- x64가 `guest-stack-not-entered`에서 더는 나가지 않습니다.
- **i386이 불변임이 측정으로 확인됩니다.**
- x64의 다음 정지점이 관측으로 기록됩니다.
- 낡은 주석이 정정됩니다.
- 작업 로그와 frontier 3.30절.

---

# Work order 20260903-583 — Let the guard ask what it actually needs

Design: [20260903-583](../design/20260903-583-fault-guard-two-questions.md)

## Files to change

| File | Change |
|---|---|
| `src/engine/execution/thread_context.h` | One `cache_entry_active` boolean |
| `src/engine/execution/execution_trampoline.cpp` | The entry guard, the two give-up sites, set/clear in the x64 thread procedure |
| `src/platform/linux/guest_stack_recover_x64.S` | Correct the stale unreachability comment (comment only) |
| `docs/analysis/linux-port-frontier.md` | Section 3.30 |

## Implementation steps

1. Add `bool cache_entry_active = false;` to `ThreadContext`, with a comment
   saying what it means — "a guest is executing through the x64 cache entry".
2. Set and clear it **around** `CallGuestCacheEntryTimed` in
   `GuestCacheEntryThreadProc`. The clear must not be skipped on early returns.
3. Rewrite the entry guard in terms of `guest_stack_entered` and
   `cache_entry_active`. **The new term must always be false on i386.**
4. At lines 4028 and 4378, return `kNotHandled` when `active_call_state` is null
   instead of dereferencing. Tag with `NoteVehExitSite` so Task 582's trace can
   name that place too.
5. If a new exit-site name is needed, **append** it to `veh_exit_site.h` (no
   reordering).
6. Correct `guest_stack_recover_x64.S`'s "no guest thread starts on x64" comment
   to the post-Task-578 fact. Leave the `ud2` bodies alone.

## Verification

1. **The i386 regression comes first.** Run `pumpit2a` for 45 seconds and check
   `last_eip=0x010F2786` with `single_step`/`aot` of the same magnitude Task 582
   recorded, plus zero `[repiu-exit]` under `REPIU_FAULT_EXIT_TRACE=1`.
2. Run x64 with `REPIU_FAULT_EXIT_TRACE=1 REPIU_GUEST_WATCH=0x010F1728` and
   record all output.
3. Confirm `site=guest-stack-not-entered` is gone.
4. Check whether `event=priv` appears. If it does not, record **where it
   stopped** — that is this unit's product.
5. Linux i386 and x64 `repiu_core_probe`.
6. Win32 configure and build, `repiu_core_probe`, `repiu_aot_probe` (pumpit1).

## Completion criteria

- x64 no longer leaves at `guest-stack-not-entered`.
- **i386 is confirmed unchanged by measurement.**
- x64's next stopping point is recorded as an observation.
- The stale comment is corrected.
- The work log and frontier section 3.30.
