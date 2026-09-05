# 작업 지시 20260904-586 — 가드 슬롯 패치 이음매 정리와 shadow selector 예약의 가시성

설계: [20260904-586](../design/20260904-586-guard-slot-patch-seam.md)

## 변경 대상

| 파일 | 변경 |
|---|---|
| `include/repiu/runtime/aot_shadow_selector_block.h` | 신규. 블록·예약 타입과 후보 사다리 |
| `src/runtime/aot_shadow_selector_block.cpp` | 신규. 예약·해제 구현과 실패 사유 |
| `include/repiu/runtime/aot_segment_patch.h` | shadow 블록 선언 제거, guarded read 패처 선언 추가 |
| `src/runtime/aot_segment_patch.cpp` | 플랫폼 의존 제거, `PatchAotGuardedSegmentReadSites` 추가 |
| `include/repiu/runtime/aot_code_cache.h` | `AotGuardedSegmentReadSite`에 `guard_prologue` 추가 |
| `src/runtime/aot_code_cache.cpp` | read 슬롯 emitter에서 `RecordGuardPrologue` 호출 |
| `src/engine/aot_code_cache.cpp` | `ResolveAotSegmentOverrides`·`ResolveAotGuardedSegmentReads`·`ReResolveWin32AotSegmentOverrides` 위임 |
| `src/engine/aot/aot_guard_compare_fault.h` | 신규. 가드 비교 구간 판정 |
| `src/engine/aot/aot_guard_compare_fault.cpp` | 신규. 구현 |
| `src/engine/execution/thread_context.h` | 새 헤더 포함, 트립와이어 카운터 |
| `src/engine/execution/execution_trampoline.cpp` | 예약 결과 로그, 트립와이어 호출 |
| `CMakeLists.txt` | 새 소스 두 개 등록 |
| `docs/analysis/linux-port-frontier.md` | 3.32절 |
| `docs/work-logs/20260904-586-guard-slot-patch-seam.md` | 작업 로그 |

## 구현 단계

1. `AotShadowSelectorBlock`·`AotShadowSelectorReservation`·후보 사다리를 새 헤더/소스로
   옮기고, 64비트에서 성공할 수 없는 `nullptr` 폴백을 제거하며 `message`를 채웁니다.
2. `aot_segment_patch`를 플랫폼 무의존으로 되돌립니다.
3. `AotGuardedSegmentReadSite`에 `guard_prologue`/`guard_prologue_size`를 추가하고
   `EmitGuardedSegmentReadSlot`에서 `RecordGuardPrologue`를 호출합니다.
4. `PatchAotGuardedSegmentReadSites`를 추가합니다.
5. `ResolveAotSegmentOverrides`와 `ResolveAotGuardedSegmentReads`,
   `ReResolveWin32AotSegmentOverrides`의 guarded read 루프를 런타임 패처 위임으로
   바꾸고, 저장소에서 슬롯 머리 상수를 제거합니다.
6. 가드 비교 구간 판정을 새 파일로 만들고 `DispatchGuestFault`에서 호출합니다.
7. `RunExecutionThread`가 shadow 블록 예약 결과를 한 줄 남깁니다.
8. Linux x64 빌드·실행, Win32 빌드, 프로브로 회귀를 확인합니다.

## 검증 절차

1. `cmake --build build/linux_x64_repiu -j 12` 성공.
2. `timeout 30s ./build/linux_x64_repiu/repiu pumpit1` 실행 후
   `[repiu-shadow-selector]` 줄에서 `base`가 후보 중 하나이고 `stride`가 4바이트인지 확인.
3. 같은 실행에서 `[repiu-guard-compare-fault]`가 나오지 않는지 확인(585 이후 도달 불가).
4. 585 기준선과 동일하게 `signal=0x4 eip=0x010F7F86`까지 도달하는지 확인 — 회귀 없음.
5. `./build/linux_x64_repiu/repiu_core_probe` 통과.
6. Win32 빌드(`build/win32_x86_debug`) 성공과 `repiu_core_probe` 통과.

---

# Work order 20260904-586 — Closing the guard-slot patch seam and making the shadow-selector reservation visible

Design: [20260904-586](../design/20260904-586-guard-slot-patch-seam.md)

## Files to change

| File | Change |
|---|---|
| `include/repiu/runtime/aot_shadow_selector_block.h` | New. Block and reservation types, candidate ladder |
| `src/runtime/aot_shadow_selector_block.cpp` | New. Reserve/release and failure reason |
| `include/repiu/runtime/aot_segment_patch.h` | Drop the shadow-block declarations, add the guarded-read patcher |
| `src/runtime/aot_segment_patch.cpp` | Drop the platform dependency, add `PatchAotGuardedSegmentReadSites` |
| `include/repiu/runtime/aot_code_cache.h` | Add `guard_prologue` to `AotGuardedSegmentReadSite` |
| `src/runtime/aot_code_cache.cpp` | Call `RecordGuardPrologue` in the read-slot emitter |
| `src/engine/aot_code_cache.cpp` | Delegate the three resolvers |
| `src/engine/aot/aot_guard_compare_fault.h` | New. Guard-compare window test |
| `src/engine/aot/aot_guard_compare_fault.cpp` | New. Implementation |
| `src/engine/execution/thread_context.h` | Include the new header, tripwire counter |
| `src/engine/execution/execution_trampoline.cpp` | Log the reservation, call the tripwire |
| `CMakeLists.txt` | Register the two new sources |
| `docs/analysis/linux-port-frontier.md` | Section 3.32 |
| `docs/work-logs/20260904-586-guard-slot-patch-seam.md` | Work log |

## Implementation steps

1. Move `AotShadowSelectorBlock`, `AotShadowSelectorReservation` and the candidate
   ladder into the new header/source, drop the `nullptr` fallback that cannot
   succeed on 64-bit, and fill in `message`.
2. Return `aot_segment_patch` to platform independence.
3. Add `guard_prologue`/`guard_prologue_size` to `AotGuardedSegmentReadSite` and
   call `RecordGuardPrologue` from `EmitGuardedSegmentReadSlot`.
4. Add `PatchAotGuardedSegmentReadSites`.
5. Delegate `ResolveAotSegmentOverrides`, `ResolveAotGuardedSegmentReads` and the
   guarded-read loop in `ReResolveWin32AotSegmentOverrides` to the runtime
   patchers, leaving no slot-head constant in the repository.
6. Add the guard-compare window test in its own files and call it from
   `DispatchGuestFault`.
7. Have `RunExecutionThread` print one line for the shadow-block reservation.
8. Verify with a Linux x64 build and run, a Win32 build, and the probes.

## Verification procedure

1. `cmake --build build/linux_x64_repiu -j 12` succeeds.
2. Run `timeout 30s ./build/linux_x64_repiu/repiu pumpit1` and confirm the
   `[repiu-shadow-selector]` line reports a candidate base with a 4-byte stride.
3. Confirm the same run prints no `[repiu-guard-compare-fault]` line — the window
   should be unreachable after 585.
4. Confirm the run still reaches `signal=0x4 eip=0x010F7F86`, matching the 585
   baseline, so nothing regressed.
5. `./build/linux_x64_repiu/repiu_core_probe` passes.
6. The Win32 build (`build/win32_x86_debug`) succeeds and its `repiu_core_probe`
   passes.
