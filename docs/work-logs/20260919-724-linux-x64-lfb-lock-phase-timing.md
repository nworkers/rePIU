# Task 724 작업 로그 — Linux x64 LFB lock 단계별 timing

설계: [20260919-724](../design/20260919-724-linux-x64-lfb-lock-phase-timing.md) ·
작업 지시: [20260919-724](../work-orders/20260919-724-linux-x64-lfb-lock-phase-timing.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260919-723](20260919-723-linux-x64-glide-host-work-ordinal.md)

## 수행 결과

`GlideLfbTimingProfile`을 추가했습니다. `REPIU_GLIDE_LFB_TIME_PROFILE=1|on|true`일 때만
`grLfbLock` staging seed의 readback, RGBA8→565 encode, total cycles 및 성공/실패를 기록합니다.
profile은 gate handler 내부의 관찰 전용이며 guest memory, OpenGL 호출 순서, lock 반환값을
바꾸지 않습니다. 종료 summary와 Windows x86 `repiu_aot_probe --glide-lfb-timing`이 profile을 포함합니다.

## Linux x64 관찰

30초 `pumpit2a` bounded run: 304 locks, readback 성공 304/실패 0, encode 성공 304/실패 0,
clamp 0. 총 10,454,401,712 cycles 중 readback 6,496,580,582(62.1%), encode
3,957,821,130(37.9%)였습니다. 최대 한 lock은 47,920,248 cycles였습니다.

## 검증

- Linux x64 Debug `repiu`, `repiu_core_probe` 빌드 성공; core probe 30/30 성공
- Linux x64 profile run: timeout immediate-exit, fault 없음, LFB timing summary 완결
- Win32 x86 Debug 전체 빌드 성공; core probe 28/28 성공
- Win32 x86 `repiu_aot_probe --glide-lfb-timing` 성공: policy, aggregation, clamp, inert 모두 true

## 다음

부분 write lock의 기존 픽셀 보존 조건을 확인한 뒤에만 full framebuffer readback 회피를
검토합니다. 지금 단계에서는 성능 최적화를 적용하지 않습니다.

---

## English

Design: [20260919-724](../design/20260919-724-linux-x64-lfb-lock-phase-timing.md) ·
Work order: [20260919-724](../work-orders/20260919-724-linux-x64-lfb-lock-phase-timing.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260919-723](20260919-723-linux-x64-glide-host-work-ordinal.md)

### Result

Added `GlideLfbTimingProfile`. Only `REPIU_GLIDE_LFB_TIME_PROFILE=1|on|true` records
readback, RGBA8-to-565 encoding, total cycles, and success/failure for the `grLfbLock`
staging seed. It is gate-handler observation only and changes neither guest memory,
OpenGL call order, nor lock return values. Shutdown summary and Windows x86
`repiu_aot_probe --glide-lfb-timing` include it.

### Linux x64 observation

30-second bounded `pumpit2a`: 304 locks, 304/0 readback success/failure, 304/0 encode
success/failure, zero clamps. Of 10,454,401,712 cycles, readback used 6,496,580,582
(62.1%) and encoding 3,957,821,130 (37.9%). Maximum one-lock total was 47,920,248 cycles.

### Verification

- Linux x64 Debug `repiu` and `repiu_core_probe` built; core probe passed 30/30.
- Linux x64 profile run: timeout immediate-exit, no fault, complete LFB timing summary.
- Full Win32 x86 Debug build succeeded; core probe passed 28/28.
- Windows x86 `repiu_aot_probe --glide-lfb-timing` passed: policy, aggregation, clamp, and inert were all true.

### Next

Consider avoiding full framebuffer readback only after confirming the original-pixel
preservation conditions of partial write locks. No performance optimization is applied
at this stage.
