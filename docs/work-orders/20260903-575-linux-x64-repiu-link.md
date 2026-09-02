# 작업 지시 20260903-575 — Linux x64 `repiu` 링크

설계: [20260903-575](../design/20260903-575-linux-x64-repiu-link.md)

## 변경 대상

| 파일 | 변경 |
|---|---|
| `src/platform/linux/guest_stack_recover_x64.S` | 신규. 두 복귀 심볼의 x64 정의 |
| `CMakeLists.txt` | x64 게이트 블록에 새 파일 추가 |
| `docs/analysis/linux-port-frontier.md` | 3.21절 |

## 구현 단계

1. `guest_stack_recover_x64.S`를 만들고 `RecoverGuestStackException`,
   `RecoverHostStackException`을 각각 `ud2`로 정의합니다. 두 심볼이 왜 도달
   불가능한지, 왜 `ret`가 아닌지 주석으로 남깁니다.
2. `.note.GNU-stack` 섹션을 넣습니다(다른 `.S`와 동일).
3. `CMakeLists.txt`의 `if(CMAKE_SIZEOF_VOID_P GREATER 4)` 블록에 추가합니다.

## 검증 절차

1. **x64 링크**: `./scripts/build_linux_x64.sh --config Release
   --build-dir build/linux_x64_repiu --target repiu` — 실행 파일 생성, 미해결
   심볼 0.
2. **x64 실행**: 생성된 `repiu`를 실제로 돌리고 종료 코드와 로그를 기록합니다.
   게스트는 돌지 않아야 하며 그 거절이 관측되어야 합니다.
3. **i386 회귀**: `./scripts/build_linux_i386.sh --config Release --target repiu`
   와 `repiu_core_probe`.
4. **Win32 회귀**: `repiu_aot_probe`.

## 완료 조건

- x64 `repiu` 실행 파일이 생기고 실제로 실행해 관측 결과를 기록합니다.
- i386과 Win32에 회귀가 없습니다.
- 작업 로그와 frontier 3.21절을 남깁니다.

---

# Work order 20260903-575 — Linking `repiu` on Linux x64

Design: [20260903-575](../design/20260903-575-linux-x64-repiu-link.md)

## Files to change

| File | Change |
|---|---|
| `src/platform/linux/guest_stack_recover_x64.S` | New. The x64 definitions of the two recovery symbols |
| `CMakeLists.txt` | Add the new file to the x64 gate block |
| `docs/analysis/linux-port-frontier.md` | Section 3.21 |

## Implementation steps

1. Create `guest_stack_recover_x64.S` defining `RecoverGuestStackException` and
   `RecoverHostStackException` as `ud2`. Comment why each is unreachable and why
   it is not a `ret`.
2. Include the `.note.GNU-stack` section, as the other `.S` files do.
3. Add it to `CMakeLists.txt`'s `if(CMAKE_SIZEOF_VOID_P GREATER 4)` block.

## Verification

1. **x64 link**: `./scripts/build_linux_x64.sh --config Release --build-dir
   build/linux_x64_repiu --target repiu` — an executable, zero unresolved
   symbols.
2. **x64 run**: actually run the resulting `repiu` and record the exit code and
   log. The guest must not run and that refusal must be observed.
3. **i386 regression**: `./scripts/build_linux_i386.sh --config Release --target
   repiu` and `repiu_core_probe`.
4. **Win32 regression**: `repiu_aot_probe`.

## Completion criteria

- An x64 `repiu` executable exists and has actually been run, with the result
  recorded.
- No regression on i386 or Win32.
- A work log and frontier section 3.21 are written.
