# 작업 지시 20260903-576 — 엔진에 long-mode 방출 연결

설계: [20260903-576](../design/20260903-576-engine-long-mode-emission.md)

## 변경 대상

| 파일 | 변경 |
|---|---|
| `include/repiu/runtime/aot_code_cache.h` | `HostRequiresLongModeEmission()` 선언 |
| `src/runtime/aot_code_cache.cpp` | 정의 |
| `include/repiu/engine/aot_code_cache.h` | `AotCodeCachePlacement::long_mode_emission_enabled` |
| `src/engine/aot_code_cache.cpp` | placement가 image에서 물려받고, append가 되읽음 |
| `src/host/win32/main.cpp` | 로더가 `aot_build_options`에 설정 |
| `docs/analysis/linux-port-frontier.md` | 3.22절 |

## 구현 단계

1. `HostRequiresLongModeEmission()`을 추가합니다. `__x86_64__` 또는 `_M_X64`이면
   `true`, 아니면 `false`.
2. `AotCodeCachePlacement`에 `long_mode_emission_enabled`를 더하고
   `PlaceAotCodeCache`에서 `image.long_mode_emission_enabled`를 복사합니다.
3. dynamic append의 `build_options.enable_long_mode_emission`을 placement에서
   읽습니다.
4. 로더에서 `aot_build_options.enable_long_mode_emission =
   HostRequiresLongModeEmission();`으로 설정합니다.

## 검증 절차

1. **x64**: `repiu` 재빌드 → 실행. Task 575와 같은 ROM 세트(`pumpit2a`)로
   비교하고, timer safe point site 수와 새 정지 지점을 기록합니다.
2. **i386**: `repiu` 링크 + `repiu_core_probe` 회귀 없음.
3. **Win32**: `repiu_aot_probe` 회귀 없음.
4. **census**: 숫자 불변(`agrees=true`, emittable·도달 가능 block 동일).

## 완료 조건

- x64 실행이 Task 575보다 더 나아가고, 어디서 멈추는지 기록됩니다.
- i386·Win32·census에 회귀가 없습니다.
- 작업 로그와 frontier 3.22절을 남깁니다.

---

# Work order 20260903-576 — Wiring long-mode emission into the engine

Design: [20260903-576](../design/20260903-576-engine-long-mode-emission.md)

## Files to change

| File | Change |
|---|---|
| `include/repiu/runtime/aot_code_cache.h` | Declare `HostRequiresLongModeEmission()` |
| `src/runtime/aot_code_cache.cpp` | Define it |
| `include/repiu/engine/aot_code_cache.h` | `AotCodeCachePlacement::long_mode_emission_enabled` |
| `src/engine/aot_code_cache.cpp` | Placement inherits from the image; append reads it back |
| `src/host/win32/main.cpp` | The loader sets it in `aot_build_options` |
| `docs/analysis/linux-port-frontier.md` | Section 3.22 |

## Implementation steps

1. Add `HostRequiresLongModeEmission()`: `true` under `__x86_64__` or `_M_X64`,
   `false` otherwise.
2. Add `long_mode_emission_enabled` to `AotCodeCachePlacement` and copy
   `image.long_mode_emission_enabled` into it in `PlaceAotCodeCache`.
3. Have the dynamic-append path read
   `build_options.enable_long_mode_emission` from the placement.
4. Set `aot_build_options.enable_long_mode_emission =
   HostRequiresLongModeEmission();` in the loader.

## Verification

1. **x64**: rebuild `repiu` and run it. Compare against Task 575 on the same ROM
   set (`pumpit2a`), recording the timer safe-point site count and the new
   stopping point.
2. **i386**: `repiu` link and `repiu_core_probe`, no regression.
3. **Win32**: `repiu_aot_probe`, no regression.
4. **Census**: numbers unchanged (`agrees=true`, same emittable and reachable
   blocks).

## Completion criteria

- The x64 run gets further than Task 575's and where it stops is recorded.
- No regression on i386, Win32, or the census.
- A work log and frontier section 3.22 are written.
