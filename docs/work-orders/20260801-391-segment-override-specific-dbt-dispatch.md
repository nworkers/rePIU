# 20260801-391 작업 지시: Segment-Override 전용 DBT Dispatch / Work Order: Segment-Override-Specific DBT Dispatch

설계: [20260801-391-segment-override-specific-dbt-dispatch.md](../design/20260801-391-segment-override-specific-dbt-dispatch.md)

## 한국어

1. code-cache 옵션과 image 상태에 segment-override 전용 dispatch opt-in을 추가합니다.
2. 활성화 시 `kSegmentOverrideMem`만 기존 HLE dispatch slot으로 방출합니다.
3. coverage validator와 synthetic probe에 활성/비활성, 누락 fallback 검증을 추가합니다.
4. Win32 host에 `REPIU_AOT_DBT_SEGMENT_OVERRIDE_DISPATCH` 정책과 로그를 연결합니다.
5. Release loader와 두 PIU 구성의 전체 probe를 검증합니다.
6. 동일 3초 opt-out/opt-in smoke로 예외, HLE dispatch 성공/복귀, selector 경계를 비교합니다.
7. 분석, 아키텍처, 작업 로그를 갱신하고 커밋합니다.

## English

1. Add a segment-override-specific dispatch opt-in to code-cache options and image state.
2. When enabled, emit only `kSegmentOverrideMem` through the existing HLE dispatch slot.
3. Extend coverage validation and the synthetic probe for enabled, disabled, and missing-fallback cases.
4. Wire `REPIU_AOT_DBT_SEGMENT_OVERRIDE_DISPATCH` policy and logging into the Win32 host.
5. Verify the Release loader and full probes for both PIU layouts.
6. Compare exceptions, HLE dispatch success/fallback, and selector boundaries in matched three-second opt-out/opt-in smokes.
7. Update analysis, architecture, and the work log, then commit.
