# 작업 지시 20260903-573 — Linux x64 indirect call slot

설계: [20260903-573](../design/20260903-573-linux-x64-indirect-call.md)

## 배경

Task 572 뒤 정지 표에서 `kIndirectExit` 60건이 가장 큽니다. 형식을 붙여 다시 재니
전부 `FF /2` indirect call이고, 34건은 return 지점에 plan block이 이미 있습니다.

## 변경 대상

| 파일 | 변경 |
|---|---|
| `src/tools/instruction_census/main.cpp` | 정지 표에 indirect exit 형식과 return 지점 유무 표기(측정, 완료) · `RecordIsEmitted`와 walk의 fallthrough 조건 |
| `include/repiu/runtime/aot_code_cache.h` | `LongModeIndirectCallEmittable` 선언 · `long_mode_indirect_call_count` |
| `src/runtime/aot_code_cache.cpp` | `EmitLongModeIndirectCall`과 emittable 판정 |
| `src/tools/aot_probe/linux_x64_guest_register_probe.cpp` | 방출 바이트 실행 probe |
| `docs/analysis/linux-port-frontier.md` | 3.18절 |

## 구현 단계

1. `LongModeIndirectCallEmittableImpl`을 추가합니다. 허용 조건:
   - `kind == kIndirectExit`, `bytes[0] == 0xFF`, ModRM `reg == 010`
   - `mod != 3` (memory 형식)
   - operand가 guest `ESP`를 가리키지 않을 것
   - 합성한 `8B` 명령의 lowering이 성공하고 그 출력이 `67 8B`로 시작할 것
2. `EmitLongModeIndirectCall`을 추가하고 설계 결정 2의 순서로 방출합니다.
   - `mov r14d, <operand>`: 합성 → `LowerLongModeBytes` → `0x44`를 index 1에 삽입
   - `push <return>`: `68 <guest_address + length>` → `LowerLongModeBytes`
   - `movabs r12, <thunk>` · `jmp r12` (Task 562와 같은 바이트)
3. long-mode 방출 분기에 `kIndirectExit` 경로를 추가하고
   `long_mode_indirect_call_count`를 셉니다.
4. census `RecordIsEmitted`에 `kIndirectExit` → `LongModeIndirectCallEmittable`을
   추가하고, walk의 tail 처리에서 `kIndirectExit`는 `FF /2`일 때만 return 지점을
   push하도록 합니다.
5. probe를 추가합니다(설계 검증 1의 네 항목).

## 검증 절차

1. Linux x64 Release `repiu_core_probe` — 전부 통과.
2. Linux x64 Release `repiu_instruction_census` — `agrees=true`, 정지 표·도달
   범위·`edge outside the plan` 변화 기록.
3. Linux i386 Release `repiu_core_probe` — 회귀 없음.
4. Win32 x86 Debug `repiu_aot_probe` — 회귀 없음.

## 완료 조건

- 위 네 검증이 모두 통과합니다.
- probe가 load/push 순서와 push가 R14D를 건드리지 않는다는 전제를 값으로
  고정합니다.
- 작업 로그와 frontier 3.18절을 남깁니다.

---

# Work order 20260903-573 — Linux x64 indirect call slot

Design: [20260903-573](../design/20260903-573-linux-x64-indirect-call.md)

## Background

`kIndirectExit` is the largest row in the stop table after Task 572, at 60.
Re-measured with the form attached, all 60 are `FF /2` indirect calls and 34
already have a plan block at their return site.

## Files to change

| File | Change |
|---|---|
| `src/tools/instruction_census/main.cpp` | Indirect-exit form and return-site presence in the stop table (measurement, done) · `RecordIsEmitted` and the walk's fallthrough condition |
| `include/repiu/runtime/aot_code_cache.h` | Declare `LongModeIndirectCallEmittable` · `long_mode_indirect_call_count` |
| `src/runtime/aot_code_cache.cpp` | `EmitLongModeIndirectCall` and the emittable predicate |
| `src/tools/aot_probe/linux_x64_guest_register_probe.cpp` | Emitted-byte execution probe |
| `docs/analysis/linux-port-frontier.md` | Section 3.18 |

## Implementation steps

1. Add `LongModeIndirectCallEmittableImpl`, admitting only:
   - `kind == kIndirectExit`, `bytes[0] == 0xFF`, ModRM `reg == 010`;
   - `mod != 3` (a memory form);
   - an operand that does not name guest `ESP`; and
   - a synthesised `8B` instruction whose lowering succeeds and begins `67 8B`.
2. Add `EmitLongModeIndirectCall`, emitting in decision 2's order:
   - `mov r14d, <operand>`: synthesise → `LowerLongModeBytes` → insert `0x44` at
     index 1;
   - `push <return>`: `68 <guest_address + length>` → `LowerLongModeBytes`;
   - `movabs r12, <thunk>` · `jmp r12`, the same bytes as Task 562.
3. Add the `kIndirectExit` path to the long-mode emission branch and count
   `long_mode_indirect_call_count`.
4. Add `kIndirectExit` → `LongModeIndirectCallEmittable` to the census's
   `RecordIsEmitted`, and make the walk push a return site for a
   `kIndirectExit` tail only when it is `FF /2`.
5. Add the probe covering the four checks in the design's verification 1.

## Verification

1. Linux x64 Release `repiu_core_probe` — all pass.
2. Linux x64 Release `repiu_instruction_census` — `agrees=true`; record the stop
   table, the reachable set, and the `edge outside the plan` movement.
3. Linux i386 Release `repiu_core_probe` — no regression.
4. Win32 x86 Debug `repiu_aot_probe` — no regression.

## Completion criteria

- All four verifications pass.
- The probe pins the load/push order and the premise that the push leaves R14D
  alone, both by value.
- A work log and frontier section 3.18 are written.
