# AOT 반환 전이 실시간 계측 작업 지시서
# Work Order: Live Telemetry for AOT Return Transfers

## 1. 목적 (Objective)

동결 지점 RET `0x030EE1DA`의 실제 반환 대상이 quarantine된 thunk 페이지(`0x030FE000`)인지
실시간으로 확인해, "번역 함수 ↔ quarantine 페이지 왕복" 가설을 판정한다. 설계는
`docs/design/20260716-219-aot-return-transfer-live-telemetry.md` 참조.

## 2. 세부 작업 (Tasks)

1. `include/repiu/platform/win32/live_telemetry.h`에 `aot_last_return_source`,
   `aot_last_return_target`, `aot_last_expected_return`, `aot_last_return_matches_call`,
   `aot_return_dispatch_count`를 추가하고 버전을 15로 올린다.
2. `src/platform/win32/execution_trampoline.cpp`의 `HandleAotReturnTransfer`에 라이브 미러링을
   추가한다.
3. `src/host/win32/supervisor_main.cpp`의 `PrintSnapshot`에 새 필드 출력을 추가한다.
4. 재빌드 후 25초 재구동으로 동결 구간의 반환 대상 주소를 관찰한다.
5. 결과를 `docs/analysis/current-execution-frontier.md`와 작업 로그에 반영한다.

## 3. 검증 범위 (Verification Scope)

텔레메트리 쓰기만 추가하는 순수 계측이다. Debug 재빌드 성공과 25초 supervised 재구동 관찰이
검증 수단이다.
