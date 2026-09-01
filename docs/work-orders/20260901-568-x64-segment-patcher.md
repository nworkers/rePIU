# 작업 지시 20260901-568 — x64 segment override를 실제 patcher에 연결

설계: [20260901-568-x64-segment-patcher.md](../design/20260901-568-x64-segment-patcher.md)

## 범위

1. `AotSegmentOverrideSite`에 `guard_prologue`/`guard_prologue_size`를 더하고,
   두 emitter가 **이미지에서 떠서** 채운다. patcher의 하드코딩 상수를 지운다.
2. 손으로 site를 만드는 `selector_guard` probe도 emitter처럼 채운다.
3. 바이트를 쓰는 루프를 `src/runtime/aot_segment_patch.cpp`로 옮긴다. 메모리를
   열고 닫는 일만 engine에 남는다. `AotSegmentTable` 등 타입도 runtime으로
   옮기고 engine에는 별칭을 둔다.
4. x64 probe가 실제 patcher를 부르도록 바꾼다. HLE로 보낸 뒤 native로 되돌리는
   왕복까지 실행한다.
5. census가 emitter의 판정을 **묻도록** `LongModeSegmentOverrideEmittable`을
   내보낸다.
6. 측정 뒤 `enable_long_mode_segment_override` 기본값을 정한다.

## 검증

- Linux x64 / i386 / Win32 회귀.
- census `agrees=true`, 도달 가능 block 재측정.

---

# Work order 20260901-568 — connect the x64 segment override to the real patcher

Design: [20260901-568-x64-segment-patcher.md](../design/20260901-568-x64-segment-patcher.md)

## Scope

1. Add `guard_prologue`/`guard_prologue_size` to `AotSegmentOverrideSite`,
   filled by both emitters **from the image itself**. Delete the patcher's
   hardcoded constant.
2. The hand-built site in the `selector_guard` probe fills it too, as an
   emitter would.
3. Move the byte-writing loop to `src/runtime/aot_segment_patch.cpp`, leaving
   only the memory work in the engine. Move `AotSegmentTable` and friends to
   runtime, aliased from the engine header.
4. Change the x64 probe to call the real patcher, and to run the HLE-and-back
   round trip.
5. Export `LongModeSegmentOverrideEmittable` so the census **asks** the
   emitter's rule.
6. Decide the `enable_long_mode_segment_override` default after measuring.

## Verification

- Linux x64, i386, and Win32 regressions.
- Census `agrees=true`, and reachable blocks re-measured.
