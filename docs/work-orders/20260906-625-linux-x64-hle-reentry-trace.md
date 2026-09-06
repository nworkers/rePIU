# Task 625 작업 지시서: Linux x64 HLE boundary 재진입 추적

## 한국어

### 작업

1. 관련 설계 문서와 Task 624 분석 결과를 기준으로 trace 범위를 고정한다.
2. `REPIU_AOT_HLE_REENTRY_TRACE` parser와 선택 주소 판정 helper를 추가한다.
3. `TryResumeAotAfterHandledHle`의 pending/current/cache/gate 상태를 기록한다.
4. post-HLE translation 시도·성공·실패와 cache target을 기록한다.
5. 환경 변수 미설정 시 기존 output과 실행 semantics를 유지한다.
6. core probe와 `pumpit2a` A/B 실행 결과를 분석 문서와 작업 로그에 남긴다.

### 제한

* guest register, EIP, EFLAGS, stack, cache map, fault policy를 변경하지 않는다.
* trace 목적의 branch 추가 외에 post-HLE 정책을 자동으로 켜지 않는다.
* trace 출력은 선택된 HLE boundary에만 한정한다.
* 기존 `0x011A6440` fault를 무시하거나 target을 보정하지 않는다.

### 완료 조건

* `core_probe_failures=0`.
* 선택 boundary의 pending/cache/gate 상태가 재현 로그로 확인된다.
* post-HLE translation A/B의 fault frontier 변화 여부가 판정된다.

## English

### Work

1. Fix the trace scope from the design and Task 624 analysis.
2. Add the `REPIU_AOT_HLE_REENTRY_TRACE` parser and selected-address match.
3. Record pending/current/cache/gate state in
   `TryResumeAotAfterHandledHle`.
4. Record post-HLE translation attempt/success/failure and cache target.
5. Preserve existing output and execution semantics when unset.
6. Record core-probe and `pumpit2a` A/B results in the analysis and work log.

### Limits

* Do not change guest registers, EIP, EFLAGS, stack, cache map, or fault policy.
* Do not enable post-HLE policy automatically as part of tracing.
* Limit trace output to the selected HLE boundary.
* Do not suppress or repair the existing `0x011A6440` fault.

### Done criteria

* `core_probe_failures=0`.
* The selected boundary's pending/cache/gate state is visible in reproduction logs.
* The A/B effect of post-HLE translation on the fault frontier is determined.

## 결과 / Result

* `0x011A643F -> 0x011A6440`에서 `pending=1`과 cache hit
  `0x200611A5`를 확인했습니다.
* immediate re-entry span은 `decode` 사유로 거부되었고, 이 실행에서는
  post-HLE translation gate에 도달하지 않았습니다.
* fault frontier는 `0x011A6440`, `EAX=0x37016BE9`로 유지되었습니다.

## Result (English)

* The `0x011A643F -> 0x011A6440` boundary had `pending=1` and a cache hit at
  `0x200611A5`.
* The immediate re-entry span was rejected with reason `decode`; this run did
  not reach the post-HLE translation gate.
* The fault frontier remained `0x011A6440` with `EAX=0x37016BE9`.
