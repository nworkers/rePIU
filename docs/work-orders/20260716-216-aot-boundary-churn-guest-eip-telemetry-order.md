# AOT boundary/reentry churn 구간 실시간 게스트 EIP 계측 작업 지시서
# Work Order: Live Guest-EIP Telemetry for the AOT Boundary/Reentry Churn Window

## 1. 목적 (Objective)

Task 215가 `0x030F6574` cross-segment thunk assertion storm으로 결론지은 관측이 stale
`last_guest_eip`에 근거한 오판정일 가능성을 검증하고, `dispatch`가 동결된 구간에서 실제로 어떤
게스트 주소가 `aot_boundary/reentry` churn을 유발하는지 실시간으로 확인한다. 설계는
`docs/design/20260716-216-aot-boundary-churn-guest-eip-telemetry.md` 참조.

## 2. 세부 작업 (Tasks)

1. `include/repiu/platform/win32/live_telemetry.h`에 `aot_boundary_guest_eip`,
   `aot_legacy_fallback_count`, `aot_last_fallback_address` 필드를 추가하고
   `kWin32LiveTelemetryVersion`을 12로 올린다.
2. `src/platform/win32/execution_trampoline.cpp`의 `HandleAotReentry` 캐시 미스 경계 지점과
   legacy fallback 진입 지점에 위 필드들의 라이브 미러링을 추가한다.
3. `src/host/win32/supervisor_main.cpp`의 `PrintSnapshot`에 새 필드 출력을 추가한다.
4. `repiu_loader_win32`/`repiu_supervisor_win32` Debug 타깃을 재빌드한다.
5. `REPIU_EXECUTION_BACKEND=aot-dynamic pumpit1`을 재구동해 동결 구간에서 새 필드 값을 관찰하고
   결과를 `docs/analysis/current-execution-frontier.md`에 반영한다(Task 215의 조기 결론을 필요 시
   정정).

## 3. 검증 범위 (Verification Scope)

코드 변경은 텔레메트리 쓰기만 추가하므로 기존 자동 빌드 검증(Debug 재빌드 성공)과 40~60초
supervised 재구동 관찰이 검증 수단이다. 게스트 실행 분기나 레지스터 조작은 변경하지 않으므로
회귀 위험은 낮다.
