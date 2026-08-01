# 20260801-391 Segment-Override 전용 DBT Dispatch 작업 로그 / Work Log

설계: [20260801-391-segment-override-specific-dbt-dispatch.md](../design/20260801-391-segment-override-specific-dbt-dispatch.md)

작업 지시: [20260801-391-segment-override-specific-dbt-dispatch.md](../work-orders/20260801-391-segment-override-specific-dbt-dispatch.md)

## 한국어

### 구현

- code-cache option/image/placement/dynamic append에 segment-override 전용 dispatch 정책을 연결했습니다.
- opt-in에서는 `kSegmentOverrideMem`만 기존 fail-closed `AotDbtHleDispatchThunk` slot으로 방출하며 다른 HLE 종류는 활성화하지 않습니다.
- `REPIU_AOT_DBT_SEGMENT_OVERRIDE_DISPATCH=1|on|true`를 추가했고 기본값은 OFF입니다.
- coverage validator와 synthetic probe가 활성 dispatch layout, fallback fixup, 비활성 기존 selector-guard slot, 누락 fallback 거부를 확인합니다.

### 검증

- Release Win32 loader와 `repiu_aot_probe` 빌드가 성공했습니다. 기존 C4819/LNK4217 경고만 남았습니다.
- 두 PIU 실행 파일 구성의 전체 probe가 종료 코드 0이고 `segment_override_dispatch_specific=true`, `selector_guard_all=true`, `coherence_all=true`입니다.
- 동일 3초 smoke의 frame 수는 off/on `588/656`(+11.56%)입니다.
- frame 정규화 전체 예외는 `62.318 -> 41.963`(-32.66%), access violation은 `19.493 -> 3.323`(-82.95%), VEH cycles는 `4.766M -> 4.088M`(-14.22%), guest-run cycles는 `18.969M -> 16.989M`(-10.44%)입니다.
- 활성 실행의 HLE host dispatch는 entry/attempt/success/fallback `39,896/39,896/31,404/8,492`입니다. 기준 성공 5,797건을 제외하면 약 25,607개의 segment-override dispatch가 직접 성공했습니다.
- fallback reason은 VEH-required 1, unhandled 8,491이며 invalid-site/target/state/unknown은 모두 0입니다. 양쪽 모두 의도한 timeout으로 끝났고 final exception과 Glide direct miss/terminal은 없습니다.

### 다음 검증

실제 Music Select 장시간 수동 캡처에서 처리량, frame 정규화 예외, unhandled fallback 비율, 화면·입력 동작을 확인하기 전까지 기본값은 OFF입니다. 장시간 결과가 유지되면 기본 승격 또는 native/HLE hybrid 여부를 결정합니다.

## English

### Implementation

- Wired a segment-override-specific dispatch policy through code-cache options, image, placement, and dynamic append.
- Under opt-in, only `kSegmentOverrideMem` emits through the existing fail-closed `AotDbtHleDispatchThunk`; no other HLE kind is enabled.
- Added `REPIU_AOT_DBT_SEGMENT_OVERRIDE_DISPATCH=1|on|true`; default remains OFF.
- Coverage validation and the synthetic probe check enabled dispatch layout, fallback fixup, disabled selector-guard layout, and rejection of a missing fallback.

### Verification

- Release Win32 loader and `repiu_aot_probe` builds passed with only pre-existing C4819/LNK4217 warnings.
- Full probes for both PIU layouts exited zero with `segment_override_dispatch_specific=true`, `selector_guard_all=true`, and `coherence_all=true`.
- Matched three-second smoke frame counts were `588/656` (+11.56%) for off/on.
- Per frame, total exceptions fell `62.318 -> 41.963` (-32.66%), access violations `19.493 -> 3.323` (-82.95%), VEH cycles `4.766M -> 4.088M` (-14.22%), and guest-run cycles `18.969M -> 16.989M` (-10.44%).
- Enabled HLE host dispatch entry/attempt/success/fallback was `39,896/39,896/31,404/8,492`. Subtracting the baseline 5,797 successes gives approximately 25,607 directly handled segment-override dispatches.
- Fallback reasons were one VEH-required and 8,491 unhandled; invalid-site, target, state, and unknown were all zero. Both runs reached the intended timeout without a final exception or Glide direct miss/terminal.

### Next verification

Default remains OFF until a long manual Music Select capture confirms throughput, frame-normalized exceptions, unhandled fallback ratio, and visual/input behavior. If the result holds, decide between default promotion and a native/HLE hybrid.

## 장시간 후속 결과

- Task 390 기준 로그와 broad-dispatch 로그의 `_GRBUFFERSWAP@4`는 `3,914 -> 2,116`으로 감소했습니다.
- frame당 전체 예외는 59.82%, guest-run cycles는 62.04%, VEH cycles는 76.38% 증가했습니다.
- dispatch fallback `21,060`건 중 `21,059`건이 unhandled였습니다.
- 따라서 broad dispatch는 기본 승격 후보에서 제외하고, Task 392의 native/HLE hybrid로 대체했습니다.

## Long-run follow-up

- `_GRBUFFERSWAP@4` fell from `3,914` in the Task 390 baseline to `2,116` with broad dispatch.
- Per-frame total exceptions rose 59.82%, guest-run cycles 62.04%, and VEH cycles 76.38%.
- Of 21,060 dispatch fallbacks, 21,059 were unhandled.
- Broad dispatch is therefore rejected for default promotion and replaced by the Task 392 native/HLE hybrid.