# AOT quarantine 원인 실시간 계측 작업 지시서
# Work Order: Live Telemetry for the AOT Quarantine Cause

## 1. 목적 (Objective)

`0x030EE1DA`가 속한 페이지의 quarantine 원인(페이지 번호, 쓰기 소스/목적지)을 실시간으로
확인해 오탐 여부를 판정한다. 설계는
`docs/design/20260716-218-aot-quarantine-cause-live-telemetry.md` 참조.

## 2. 세부 작업 (Tasks)

1. `include/repiu/platform/win32/live_telemetry.h`에 `aot_last_retired_page`,
   `aot_last_code_write_source`, `aot_last_code_write_destination`을 추가하고 버전을 14로
   올린다.
2. `src/platform/win32/execution_trampoline.cpp`의 기존 저장 지점에 라이브 미러링을 추가한다.
3. `src/host/win32/supervisor_main.cpp`의 `PrintSnapshot`에 새 필드 출력을 추가한다.
4. 재빌드 후 40초 재구동으로 quarantine 발생 구간(8~16초)의 값을 관찰한다.
5. 결과에 따라 `HandleAotReentry`가 quarantine된 페이지에서도 이미 캐시 plan에 존재하는 반환
   thunk를 우선 재시도하도록 하는 로컬 실험(커밋하지 않음)을 수행해 progress 재개 여부를
   인과적으로 확인한다.
6. 결과를 `docs/analysis/current-execution-frontier.md`와 작업 로그에 반영한다.

## 3. 검증 범위 (Verification Scope)

1~4단계는 텔레메트리 쓰기만 추가하는 순수 계측이다. 5단계 실험은 실행 의미론을 바꾸므로
결과 확인 후 커밋하지 않고 되돌린다(Task 211의 로컬 실험 선례와 동일한 방식).
