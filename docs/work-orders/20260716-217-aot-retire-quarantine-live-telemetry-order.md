# AOT 페이지 retire/quarantine 실시간 계측 작업 지시서
# Work Order: Live Telemetry for AOT Page Retire/Quarantine Counters

## 1. 목적 (Objective)

Task 216이 남긴 가설("guest `0x030EE1DA` RET가 속한 코드 페이지가 반복 retire/quarantine되어
전용 반환 thunk 대신 매번 느린 boundary 경로로 빠진다")을 실시간 계측으로 검증한다. 설계는
`docs/design/20260716-217-aot-retire-quarantine-live-telemetry.md` 참조.

## 2. 세부 작업 (Tasks)

1. `include/repiu/platform/win32/live_telemetry.h`에 `aot_retired_entry_trap_count`,
   `aot_quarantine_count`, `aot_page_retire_attempt_count`, `aot_page_retire_success_count`를
   추가하고 버전을 13으로 올린다.
2. `src/platform/win32/execution_trampoline.cpp`의 기존 네 카운터 증가 지점에 라이브 미러링을
   추가한다.
3. `src/host/win32/supervisor_main.cpp`의 `PrintSnapshot`에 새 필드 출력을 추가한다.
4. `repiu_loader_win32`/`repiu_supervisor_win32` Debug 재빌드.
5. `REPIU_EXECUTION_BACKEND=aot-dynamic pumpit1`을 40초 재구동해 동결 구간에서 새 필드 값을
   관찰하고 결과를 `docs/analysis/current-execution-frontier.md`에 반영한다.

## 3. 검증 범위 (Verification Scope)

텔레메트리 쓰기만 추가하므로 Debug 재빌드 성공과 40초 supervised 재구동 관찰이 검증 수단이다.
게스트 실행 분기나 레지스터 조작은 변경하지 않는다.
