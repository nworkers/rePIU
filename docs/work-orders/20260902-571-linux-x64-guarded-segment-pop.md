# 작업 지시 20260902-571 — Linux x64 guarded segment pop

설계: [20260902-571](../design/20260902-571-linux-x64-guarded-segment-pop.md)

## 목표

Task 570 뒤 census가 보고하는 유일한 frontier kind인 `kGuardedSegmentPop`을
long-mode code cache에서 실행 가능하게 만든다. 첫 정지 지점은 `0x10fc2d5`의
`1f`(`POP DS`)다.

## 범위

포함한다.

- long-mode `kGuardedSegmentPop` slot emitter와 그 admission predicate
- `AotGuardedSegmentPopSite`의 `guard_prologue`·`has_counter_operands`
- `PatchAotGuardedSegmentPopSites` runtime patcher
- engine의 세 patch 경로를 runtime patcher 호출로 통일 (설계 결정 3)
- census tally와 `RecordIsEmitted`의 새 kind 처리
- Linux x64 실행 probe `ProbeGuardedSegmentPop`

포함하지 않는다.

- selector가 실제로 바뀌는 경우의 native 처리. 그것은 descriptor base 갱신을
  포함하며 기존 HLE에 남긴다.
- `PUSH Sreg`(`kGuardedSegmentRead` 계열)와 `LDS`/`LES` 같은 far pointer load.
- i386 slot의 바이트 변경. i386은 회귀 확인 대상이지 수정 대상이 아니다.

## 단계

1. `include/repiu/runtime/aot_code_cache.h`
   - `AotGuardedSegmentPopSite`에 `guard_prologue`,
     `guard_prologue_size`, `has_counter_operands`를 더한다.
   - `long_mode_guarded_segment_pop_count`를 image에 더한다.
   - `LongModeGuardedSegmentPopEmittable`을 선언한다.
2. `src/runtime/aot_code_cache.cpp`
   - `LongModeGuardedSegmentPopEmittableImpl`을 load predicate 옆에 둔다.
   - `EmitLongModeGuardedSegmentPop`을 설계 결정 2의 바이트 표대로 방출하고
     `RecordGuardPrologue`로 prologue를 기록한다. fixup 목표는
     `instruction.fallthrough_target`이다.
   - 기존 i386 `EmitGuardedSegmentPopSlot`에 `has_counter_operands = true`와
     `RecordGuardPrologue`를 더한다.
   - long-mode 방출 분기에 새 emitter를 넣는다.
   - 방출 검증(layout 확인) 경로가 새 kind를 알도록 한다.
3. `include/repiu/runtime/aot_segment_patch.h`, `src/runtime/aot_segment_patch.cpp`
   - `AotGuardedSegmentPopPatchStats`와 `PatchAotGuardedSegmentPopSites`를
     load patcher와 같은 계약으로 추가한다.
4. `src/engine/aot_code_cache.cpp`
   - `ResolveAotGuardedSegmentPops`, `ResolveAotGuardedSegmentLoads`,
     그리고 재해결 경로의 인라인 pop patch를 runtime patcher 호출로 바꾼다.
5. `src/tools/instruction_census/main.cpp`
   - tally와 `RecordIsEmitted`에 새 kind를 더하고 `agrees=` 합계에 넣는다.
6. `src/tools/aot_probe/linux_x64_guest_register_probe.cpp`
   - `ProbeGuardedSegmentPop`을 더하고 `RunLinuxX64GuestRegisterProbe`에 넣는다.

## 완료 조건

- Linux x64 census가 `agrees=true`이고 first stop이 `0x10fc2d5`에서 이동한다.
- Linux x64 core probe가 `guest_segment_pop=true`,
  `linux_x64_guest_register_all=true`로 통과한다.
- Linux i386과 Win32 x86 probe에 회귀가 없다.
- 설계의 검증 항목 여섯 개가 모두 증거와 함께 작업 기록에 남는다.

---

# Work order 20260902-571 — Linux x64 guarded segment pop

Design: [20260902-571](../design/20260902-571-linux-x64-guarded-segment-pop.md)

## Objective

Make `kGuardedSegmentPop` -- the only frontier kind the census reports after
Task 570 -- executable in the long-mode code cache. The first stop is `1f`
(`POP DS`) at `0x10fc2d5`.

## Scope

In scope:

- the long-mode `kGuardedSegmentPop` slot emitter and its admission predicate;
- `guard_prologue` and `has_counter_operands` on `AotGuardedSegmentPopSite`;
- the `PatchAotGuardedSegmentPopSites` runtime patcher;
- unifying the engine's three patch paths onto the runtime patchers
  (design decision 3);
- the new kind in the census tally and in `RecordIsEmitted`; and
- the Linux x64 execution probe `ProbeGuardedSegmentPop`.

Out of scope:

- handling natively a selector that actually changes; that involves updating the
  descriptor base and stays with the existing HLE;
- `PUSH Sreg` (the `kGuardedSegmentRead` family) and far-pointer loads such as
  `LDS`/`LES`; and
- changing the i386 slot's bytes. i386 is a regression subject here, not an
  edit target.

## Steps

1. `include/repiu/runtime/aot_code_cache.h`
   - add `guard_prologue`, `guard_prologue_size`, and `has_counter_operands`
     to `AotGuardedSegmentPopSite`;
   - add `long_mode_guarded_segment_pop_count` to the image; and
   - declare `LongModeGuardedSegmentPopEmittable`.
2. `src/runtime/aot_code_cache.cpp`
   - put `LongModeGuardedSegmentPopEmittableImpl` beside the load predicate;
   - emit `EmitLongModeGuardedSegmentPop` per design decision 2's byte table and
     record its prologue with `RecordGuardPrologue`; the fixup target is
     `instruction.fallthrough_target`;
   - give the existing i386 `EmitGuardedSegmentPopSlot`
     `has_counter_operands = true` and a `RecordGuardPrologue` call;
   - add the new emitter to the long-mode emission branch; and
   - teach the emission-verification (layout) path about the new kind.
3. `include/repiu/runtime/aot_segment_patch.h`,
   `src/runtime/aot_segment_patch.cpp`
   - add `AotGuardedSegmentPopPatchStats` and
     `PatchAotGuardedSegmentPopSites` on the load patcher's contract.
4. `src/engine/aot_code_cache.cpp`
   - replace `ResolveAotGuardedSegmentPops`, `ResolveAotGuardedSegmentLoads`,
     and the inline pop patch on the re-resolution path with runtime patcher
     calls.
5. `src/tools/instruction_census/main.cpp`
   - add the new kind to the tally and to `RecordIsEmitted`, and include it in
     the `agrees=` total.
6. `src/tools/aot_probe/linux_x64_guest_register_probe.cpp`
   - add `ProbeGuardedSegmentPop` and include it in
     `RunLinuxX64GuestRegisterProbe`.

## Done when

- the Linux x64 census reports `agrees=true` and the first stop has moved off
  `0x10fc2d5`;
- the Linux x64 core probe passes with `guest_segment_pop=true` and
  `linux_x64_guest_register_all=true`;
- the Linux i386 and Win32 x86 probes show no regression; and
- all six of the design's verification items are recorded in the work log with
  their evidence.
