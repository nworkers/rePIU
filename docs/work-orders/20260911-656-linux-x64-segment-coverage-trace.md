# 20260911-656 작업 지시: Linux x64 segment-override coverage slot trace

설계: [20260911-656 설계](../design/20260911-656-linux-x64-segment-coverage-trace.md)

## 한국어

1. 기존 dynamic AOT trace에 emitted address-map entry와 slot bytes 출력을 추가합니다.
2. `AotSegmentOverrideSite`의 실제 offset과 segment 정보를 같은 trace에 출력합니다.
3. 출력 범위는 `REPIU_AOT_FALLBACK_TRACE`와 `REPIU_AOT_DYNAMIC_CONTAINS`가
   활성화된 진단 실행으로 제한합니다.
4. 최신 Linux x64 Debug를 빌드하고 core probe와 실제 `pumpit2a` trace를 검증합니다.
5. 분석 문서와 작업 로그를 갱신하고 커밋합니다.

범위 밖:

* coverage validator의 판정 변경
* segment override emission 변경
* guest stack, RET resolver, fallback 정책 변경

## English

1. Extend the existing dynamic AOT trace with the emitted address-map entry and slot bytes.
2. Print the matching `AotSegmentOverrideSite` offsets and segment information in the same trace.
3. Limit the output to diagnostic runs with `REPIU_AOT_FALLBACK_TRACE` and
   `REPIU_AOT_DYNAMIC_CONTAINS` enabled.
4. Build Linux x64 Debug and verify the core probe and real `pumpit2a` trace.
5. Update the analysis document and work log, then commit.

Out of scope:

* Changing the coverage validator's decision
* Changing segment-override emission
* Changing guest stack, RET resolver, or fallback policy
