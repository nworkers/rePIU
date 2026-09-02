# 작업 지시 20260903-574 — Linux x64 두 바이트 opcode의 `ESP` 재인코딩

설계: [20260903-574](../design/20260903-574-linux-x64-two-byte-stack-pointer-reencode.md)

## 변경 대상

| 파일 | 변경 |
|---|---|
| `src/tools/instruction_census/main.cpp` | 정지 표 `kCopy` 행에 mnemonic 추가(측정, 완료) |
| `src/runtime/aot_long_mode_compatibility.cpp` | `kStackPointerToR15`의 opcode 위치와 map 조건 |
| `src/tools/aot_probe/long_mode_lowering_probe.cpp` | 바이트 단위 검사와 거절 유지 |
| `src/tools/aot_probe/linux_x64_guest_register_probe.cpp` | 방출 바이트 실행 |
| `ARCHITECTURE.md`, `docs/analysis/linux-port-frontier.md` | 반영 |

## 구현 단계

1. `opcode_offset`을 `instruction.raw.modrm.offset - 1`에서
   `instruction.raw.prefix_count`로 바꿉니다.
2. `opcode_map != DEFAULT` 거절을 설계 결정 2의 map별 레이아웃 단언으로
   교체합니다.
   - `DEFAULT`: `modrm.offset == prefix_count + 1`
   - `0F`: `modrm.offset == prefix_count + 2`
   - 그 밖: 거절
3. `long_mode_lowering_probe`에 다음을 추가합니다.
   - `0F B6 74 24 08` → `41 0F B6 74 24 08` 정확 일치
   - 기존 한 바이트 형식(`8B 44 24 08`)이 회귀하지 않을 것
4. `linux_x64_guest_register_probe`에 `movzx`가 guest stack을 읽는지 실행으로
   확인하는 검사를 추가합니다.

## 검증 절차

1. Linux x64 Release `repiu_core_probe` — 전부 통과.
2. Linux x64 Release `repiu_instruction_census` — `agrees=true`, 정지 표·도달
   범위 기록.
3. Linux i386 Release `repiu_core_probe` — 회귀 없음.
4. Win32 x86 Debug `repiu_aot_probe` — 회귀 없음.

## 완료 조건

- 위 네 검증이 모두 통과합니다.
- `stack-pointer/lowering-declined` `imul`/`movzx` 17건이 정지 표에서 사라집니다.
- 작업 로그와 frontier 3.19절을 남깁니다.

---

# Work order 20260903-574 — Long-mode `ESP` re-encoding for two-byte opcodes

Design: [20260903-574](../design/20260903-574-linux-x64-two-byte-stack-pointer-reencode.md)

## Files to change

| File | Change |
|---|---|
| `src/tools/instruction_census/main.cpp` | Mnemonic on the stop table's `kCopy` rows (measurement, done) |
| `src/runtime/aot_long_mode_compatibility.cpp` | The opcode position and map conditions in `kStackPointerToR15` |
| `src/tools/aot_probe/long_mode_lowering_probe.cpp` | Byte-level check and refusals |
| `src/tools/aot_probe/linux_x64_guest_register_probe.cpp` | Emitted-byte execution |
| `ARCHITECTURE.md`, `docs/analysis/linux-port-frontier.md` | Reflect the change |

## Implementation steps

1. Change `opcode_offset` from `instruction.raw.modrm.offset - 1` to
   `instruction.raw.prefix_count`.
2. Replace the `opcode_map != DEFAULT` refusal with decision 2's per-map layout
   assertion:
   - `DEFAULT`: `modrm.offset == prefix_count + 1`;
   - `0F`: `modrm.offset == prefix_count + 2`;
   - anything else: refuse.
3. Add to `long_mode_lowering_probe`:
   - `0F B6 74 24 08` → `41 0F B6 74 24 08`, exact match; and
   - the existing one-byte form (`8B 44 24 08`) does not regress.
4. Add a check to `linux_x64_guest_register_probe` that runs the emitted bytes
   and confirms the `movzx` reads the guest stack.

## Verification

1. Linux x64 Release `repiu_core_probe` — all pass.
2. Linux x64 Release `repiu_instruction_census` — `agrees=true`; record the stop
   table and reachable movement.
3. Linux i386 Release `repiu_core_probe` — no regression.
4. Win32 x86 Debug `repiu_aot_probe` — no regression.

## Completion criteria

- All four verifications pass.
- The 17 `stack-pointer/lowering-declined` `imul`/`movzx` stops are gone.
- A work log and frontier section 3.19 are written.
