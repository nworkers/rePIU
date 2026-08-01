# 20260801-390 Segment Load 기본 승격 작업 로그 / Work Log

설계: [20260801-390-segment-load-default-promotion.md](../design/20260801-390-segment-load-default-promotion.md)

작업 지시: [20260801-390-segment-load-default-promotion.md](../work-orders/20260801-390-segment-load-default-promotion.md)

## 한국어

### 측정 결론

- 기준/활성 capture의 `_GRBUFFERSWAP@4`는 `3,957/3,914`로 1.1% 이내이며 둘 다 SDL 종료 요청으로 끝났습니다.
- frame 정규화 결과 전체 예외 -24.85%, breakpoint -44.74%, AOT boundary -50.80%, effective `8E` -93.53%였습니다.
- guarded success/fallback은 `24,102/1,617`(93.71%/6.29%)이며 terminal failure와 Glide direct miss/terminal은 없습니다.

### 구현 및 검증

- `aot-dbt`에서 환경 변수 미지정 시 guarded segment-load를 기본 활성화했습니다.
- `REPIU_AOT_GUARDED_SEGMENT_LOAD=0|off|false`와 알 수 없는 값은 opt-out이며 다른 backend는 비활성화됩니다.
- Release Win32 loader와 `repiu_aot_probe` 빌드가 성공했습니다. 기존 C4819 경고만 남았습니다.
- 두 PIU 실행 파일 구성의 전체 probe는 종료 코드 0이며 모든 guarded segment-load 항목, `selector_guard_all=true`, `coherence_all=true`를 확인했습니다.
- 1초 기본 실행은 `enabled/sites: true/65`, 명시적 `0` 실행은 `false/0`을 기록했습니다. 둘 다 의도한 timeout으로 끝났고 Glide direct miss/terminal은 0입니다.

## English

### Measurement conclusion

- Baseline/enabled `_GRBUFFERSWAP@4` counts were `3,957/3,914`, within 1.1%, and both captures ended by SDL exit request.
- Frame-normalized total exceptions fell 24.85%, breakpoints 44.74%, AOT boundaries 50.80%, and effective `8E` boundaries 93.53%.
- Guarded success/fallback was `24,102/1,617` (93.71%/6.29%), with no terminal failure or Glide direct miss/terminal.

### Implementation and verification

- Guarded segment-load is now enabled by default for `aot-dbt` when the environment variable is unset.
- `REPIU_AOT_GUARDED_SEGMENT_LOAD=0|off|false` and unknown values opt out; other backends remain disabled.
- The Release Win32 loader and `repiu_aot_probe` builds passed with only pre-existing C4819 warnings.
- Full probes for both PIU executable layouts exited zero and reported all guarded segment-load checks, `selector_guard_all=true`, and `coherence_all=true`.
- A one-second default run logged `enabled/sites: true/65`; explicit `0` logged `false/0`. Both reached the intended timeout with zero Glide direct misses or terminal failures.
