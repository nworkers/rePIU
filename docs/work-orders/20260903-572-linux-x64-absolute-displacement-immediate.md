# 작업 지시 20260903-572 — Linux x64 absolute displacement + immediate

설계: [20260903-572](../design/20260903-572-linux-x64-absolute-displacement-immediate.md)

## 배경

Task 571 기준선에서 첫 정지 지점은 `0x10fc2fa`
(`80 3d a6 93 15 01 01`)이고, 거절 이유는 `rip-relative/lowering-declined`입니다.
같은 이유의 거절이 865건으로 전체 거절 1,609건의 53.8%입니다.

## 변경 대상

| 파일 | 변경 |
|---|---|
| `src/runtime/aot_long_mode_compatibility.cpp` | `LowerLongModeBytes`의 `kAbsoluteToSib` 분기 폭 조건 교체 |
| `src/tools/aot_probe/long_mode_lowering_probe.cpp` | lowering 바이트 단위 검사와 거절 유지 검사 추가 |
| `src/tools/aot_probe/linux_x64_guest_register_probe.cpp` | 실제 방출 바이트 실행 probe 추가 |
| `docs/analysis/linux-port-frontier.md` | 3.16절 추가 |

## 구현 단계

1. `kAbsoluteToSib` 분기에서 `modrm_offset + 1U + 4U != length` 조건을 설계 결정 2의
   네 조건으로 교체합니다.
   - `raw.disp.size == 32`, `raw.disp.offset == modrm_offset + 1`
   - immediate 총 바이트 수 == `length - (disp.offset + 4)`
   - `is_relative` immediate 없음
   - `length + 2 <= kMaxLoweredBytes`
2. disp32를 쓴 뒤 `[disp.offset + 4, length)` 구간을 그대로 이어 씁니다.
3. `long_mode_lowering_probe`에 다음을 추가합니다.
   - `80 3d a6 93 15 01 01` → `67 80 3c 25 a6 93 15 01 01` 정확 일치
   - immediate 없는 기존 형식(`8b 0d <disp32>`)이 회귀하지 않을 것
   - `disp.size != 32`인 형식이 계속 거절될 것
4. `linux_x64_guest_register_probe`에 방출 바이트 실행 검사를 추가합니다. 같은 값과
   다른 값 두 경우의 ZF, 읽은 주소, guest `ESP` 보존을 고정합니다.

## 검증 절차

1. Linux x64 Release `repiu_core_probe` — 전부 통과, `long_mode_lowering`과
   `linux_x64_guest_register` 포함.
2. Linux x64 Release `repiu_instruction_census` — `agrees=true`, 새 first stop 기록,
   emittable/complete/reachable 변화 기록.
3. Linux i386 Release `repiu_core_probe` — 회귀 없음.
4. Win32 x86 Debug `repiu_aot_probe` — 회귀 없음.

## 완료 조건

- 위 네 검증이 모두 통과합니다.
- `rip-relative/lowering-declined` 865건이 census에서 사라집니다.
- 작업 로그와 frontier 3.16절을 남깁니다.

---

# Work order 20260903-572 — Linux x64 absolute displacement with an immediate

Design: [20260903-572](../design/20260903-572-linux-x64-absolute-displacement-immediate.md)

## Background

At the Task 571 baseline the first stop is `0x10fc2fa`
(`80 3d a6 93 15 01 01`), refused as `rip-relative/lowering-declined`. That same
reason accounts for 865 refusals, 53.8% of the 1,609 total.

## Files to change

| File | Change |
|---|---|
| `src/runtime/aot_long_mode_compatibility.cpp` | Replace the width condition in `LowerLongModeBytes`'s `kAbsoluteToSib` branch |
| `src/tools/aot_probe/long_mode_lowering_probe.cpp` | Add byte-level lowering checks and refusal-still-holds checks |
| `src/tools/aot_probe/linux_x64_guest_register_probe.cpp` | Add an emitted-byte execution probe |
| `docs/analysis/linux-port-frontier.md` | Add section 3.16 |

## Implementation steps

1. In the `kAbsoluteToSib` branch, replace `modrm_offset + 1U + 4U != length`
   with design decision 2's four conditions:
   - `raw.disp.size == 32` and `raw.disp.offset == modrm_offset + 1`;
   - total immediate bytes == `length - (disp.offset + 4)`;
   - no `is_relative` immediate; and
   - `length + 2 <= kMaxLoweredBytes`.
2. After writing the disp32, append the range `[disp.offset + 4, length)`
   unchanged.
3. Add to `long_mode_lowering_probe`:
   - `80 3d a6 93 15 01 01` → `67 80 3c 25 a6 93 15 01 01`, exact match;
   - the existing no-immediate form (`8b 0d <disp32>`) does not regress; and
   - a form with `disp.size != 32` is still refused.
4. Add an emitted-byte execution check to `linux_x64_guest_register_probe`,
   pinning ZF for both the equal and unequal cases, the address actually read,
   and guest `ESP` preservation.

## Verification

1. Linux x64 Release `repiu_core_probe` — all pass, including
   `long_mode_lowering` and `linux_x64_guest_register`.
2. Linux x64 Release `repiu_instruction_census` — `agrees=true`, record the new
   first stop and the emittable/complete/reachable movement.
3. Linux i386 Release `repiu_core_probe` — no regression.
4. Win32 x86 Debug `repiu_aot_probe` — no regression.

## Completion criteria

- All four verifications pass.
- The 865 `rip-relative/lowering-declined` refusals are gone from the census.
- A work log and frontier section 3.16 are written.
