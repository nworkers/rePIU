# 작업 지시 20260903-577 — x64 fault 경로의 guest `ESP`

설계: [20260903-577](../design/20260903-577-x64-guest-esp-context.md)

## 변경 대상

| 파일 | 변경 |
|---|---|
| `src/platform/linux/guest_cpu_context.cpp` | x64 분기의 `Esp` ↔ `R15` 매핑, `RSP` 미기록 |
| `src/tools/aot_probe/guest_cpu_context_probe.cpp` | x64 단언을 새 계약으로 교체 |
| `docs/analysis/linux-port-frontier.md` | 3.23절과 3.20 항목 2 정정 |

## 구현 단계

1. x64 load에서 `registers->Esp`를 `REG_R15`에서 읽습니다.
2. x64 store에서 `machine.gregs[REG_R15]`에 zero-extend해 씁니다.
3. x64 store에서 `machine.gregs[REG_RSP]` 기록을 **제거**합니다.
4. probe의 x64 단언을 교체합니다.
   - `Esp`가 `R15`로 왕복
   - store 전에 `RSP`에 심어 둔 값이 store 뒤에도 보존

## 검증 절차

1. Linux x64 Release `repiu_core_probe` — 전부 통과.
2. x64 `repiu` 실행 — Task 576과 같은 지점에서 정지(guest entry 울타리, exit 0).
3. Linux i386 Release `repiu` 링크 + `repiu_core_probe` — 회귀 없음.
4. Win32 x86 Debug `repiu_aot_probe` — 회귀 없음.

## 완료 조건

- probe가 `Esp` ↔ `R15` 왕복과 `RSP` 보존을 값으로 고정합니다.
- 위 네 검증이 통과합니다.
- 작업 로그, frontier 3.23절, 3.20 항목 2 정정을 남깁니다.

---

# Work order 20260903-577 — Guest `ESP` in the x64 fault path

Design: [20260903-577](../design/20260903-577-x64-guest-esp-context.md)

## Files to change

| File | Change |
|---|---|
| `src/platform/linux/guest_cpu_context.cpp` | The x64 branch's `Esp` ↔ `R15` mapping, and no `RSP` write |
| `src/tools/aot_probe/guest_cpu_context_probe.cpp` | Replace the x64 assertions with the new contract |
| `docs/analysis/linux-port-frontier.md` | Section 3.23 and the correction to 3.20's item 2 |

## Implementation steps

1. In the x64 load, read `registers->Esp` from `REG_R15`.
2. In the x64 store, write `machine.gregs[REG_R15]` zero-extended.
3. In the x64 store, **remove** the `machine.gregs[REG_RSP]` write.
4. Replace the probe's x64 assertions:
   - `Esp` round-trips through `R15`; and
   - a value planted in `RSP` before the store survives it.

## Verification

1. Linux x64 Release `repiu_core_probe` — all pass.
2. x64 `repiu` run — stops at the same place as Task 576 (the guest-entry fence,
   exit 0).
3. Linux i386 Release `repiu` link and `repiu_core_probe` — no regression.
4. Win32 x86 Debug `repiu_aot_probe` — no regression.

## Completion criteria

- The probe pins the `Esp` ↔ `R15` round trip and `RSP` preservation by value.
- All four verifications pass.
- A work log, frontier section 3.23, and the correction to 3.20's item 2 are
  written.
