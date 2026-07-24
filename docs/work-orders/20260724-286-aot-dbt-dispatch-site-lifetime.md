# AOT-DBT dispatch-site 수명 안전 작업 지시서 / AOT-DBT dispatch-site lifetime safety work order

## 한국어

### 목표

Task 285에서 확정한 dispatch-site use-after-reallocation을 indirect와 RET host
adapter에서 제거하고, calls-only 실제 크래시가 사라지는지 검증합니다.

### 작업 범위

1. 두 `FindDispatchSite`를 pointer 반환에서 caller-owned value snapshot으로 바꿉니다.
2. resolver 전후 모든 site field 접근을 local snapshot으로 통일합니다.
3. AOT host adapter의 유사 placement-vector pointer 수명 패턴을 검색합니다.
4. 기존 synthetic probe를 통과시킵니다.
5. Task 285 sequence 56 probe를 재실행합니다.
6. probe-off calls-only와 indirect-off control을 240초 격리 EEPROM으로 실행합니다.
7. 분석·아키텍처·작업 로그를 갱신합니다.

### 비범위

- CALL/RET emitter, thunk ABI 또는 stack 의미 변경
- vector container 교체나 전역 reserve 의존
- CALL host dispatch 기본 활성화
- 성능 최적화

### 완료 조건

- resolver 호출 뒤 placement site 포인터/참조 사용이 없습니다.
- sequence 56이 정상 pre/post/return 상태를 기록합니다.
- calls-only 240초에서 기존 Glide AV가 사라지고 실제 도달한 Glide 호출을 기록합니다.
- 전체 probe와 RET 회귀가 통과하고 EEPROM hash가 control과 같습니다.

## English

### Goal and scope

Remove the dispatch-site use-after-reallocation confirmed by Task 285 from both indirect and
RET host adapters, then verify that the calls-only live crash disappears. Convert both site
lookups from pointer returns to caller-owned value snapshots, use only the snapshot across
resolver calls, audit similar host-adapter pointer lifetimes, run all probes, repeat the
sequence 56 step test, and run isolated 240-second probe-off calls-only and indirect-off
control tests.

Emitter/thunk ABI, stack semantics, vector-container replacement, reserve-based assumptions,
default CALL enablement, and performance optimization are out of scope. Completion requires
no placement-site pointer/reference surviving a resolver, a fully matching sequence 56
round trip, no former Glide AV through 240 seconds with the reached Glide calls recorded,
passing RET and full probe regressions, and matching EEPROM hashes.
