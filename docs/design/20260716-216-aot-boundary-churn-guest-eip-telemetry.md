# AOT boundary/reentry churn 구간의 실시간 게스트 EIP 계측 설계
# Design: Live Guest-EIP Telemetry for the AOT Boundary/Reentry Churn Window

## 1. 배경 (Background)

Task 215는 90초 `aot-dynamic` 재검증에서 `dispatch_entry/exit`가 `elapsed_ms≈19031`에 `56857/56857`로
동결된 뒤 90초 종료까지 전혀 늘지 않는 현상을 관측하고, 이 구간의 `last_eip`/`last_guest_eip`가
`0x030F6574`, `exception=0x80000003`(`EXCEPTION_BREAKPOINT`)로 고정되어 있다는 이유로 이를
"`0x030F6574` cross-segment thunk assertion storm의 재발"로 결론지었다.

그러나 `last_eip`/`last_guest_eip`는 `ExceptionDispatchScope`로 감싸인 **완전 dispatch**에서만
갱신되는 필드다(`GuestStackVectoredExceptionHandler`의 `Lightweight VEH transfer paths run without
an ExceptionDispatchScope` 주석, `execution_trampoline.cpp:9110`). Task 205가 이미 한 번 지적한 것과
정확히 같은 함정이다 — "`Relocated exception byte window`가 SEH 예외의 AOT 매핑 guest 주소를
쓰지 않고 stale `last_guest_eip`를 사용해 종료 지점을 오판정했다."

동일 로그를 `aot_boundary_count`/`aot_reentry_count`(경량 VEH 경로, dispatch와 무관하게 갱신됨)와
`diagnostic_progress_count`/`single_step_trace_count` 기준으로 다시 읽으면 다른 그림이 나온다.

| elapsed_ms | dispatch | progress | single_step | aot(boundary+reentry) |
|---|---|---|---|---|
| 19031 | 56857 (동결 시작) | 7457 (동결 시작) | 47737 (동결 시작) | 50584/50620 |
| 55078 | 56857 | 7457 | 47737 | 83282/83317 |
| 85094 | 56857 | 7457 | 47737 | 116742/116777 |

66초 동안 `dispatch`·`progress`·`single_step`는 완전히 정지했지만 `aot_boundary/reentry`는 초당
약 1,050~1,100씩 계속 증가한다. 이는 `HandleAotReentry`(`execution_trampoline.cpp:9986`)의
"인라인 캐시 미스 → 게스트 원본 명령 1개를 TF로 단일 스텝 실행 → 결과 주소를 캐시에서 재탐색 →
캐시로 복귀" 경로(레지스터 손상 없이 `BumpAotBoundaryCount`/`BumpAotReentryCount`만 갱신, `progress`/
`single_step`은 건드리지 않음)가 초당 1,000회 이상 반복되고 있다는 뜻이다. `HandleOriginalFatalBreakpoint`가
처리하는 `fatal_breakpoint_count`도 이 구간 내내 0으로 남아, `0x030F3438`류의 "인식된" fatal
breakpoint 관용구가 아니라는 점도 이미 확인됐다(Task 215 로그 참조).

즉 이 구간은 (a) 실기 DOS4GW cross-segment thunk assertion storm이 아니라, (b) 2026-07-14
600초 관측이 미확정으로 남긴 "게임 내부 tick/플래그 폴링 무한 대기" 가설, 또는 (c) 동일 인라인
캐시 미스가 반복적으로 실패/재발하는 캐시 스래싱일 가능성이 더 높다. 다만 현재 텔레메트리로는
**정확히 어떤 게스트 주소가 이 캐시 미스를 반복적으로 유발하는지 알 수 없다** — `aot_last_indirect_source/
target`은 `HandleAotFarCallTransfer`류의 두 경로에서만 갱신되고, 이번 churn을 유발하는
`HandleAotReentry`의 단일 스텝 경로는 이를 갱신하지 않기 때문이다.

## 2. 목적 (Objective)

`HandleAotReentry`의 캐시 미스 경계 지점에서 사용되는 게스트 주소(`guest_address`/`current`)를
실시간 공유 텔레메트리에 노출해, dispatch가 동결된 상태에서도 supervisor가 "지금 어떤 게스트
주소가 이 churn을 유발하는지"를 관측할 수 있게 한다. 부수적으로 `aot_legacy_fallback_count`/
`aot_last_fallback_address`(현재는 실행 종료 후 요약값으로만 노출됨)도 같은 방식으로 라이브
미러링해, "완전한 legacy fallback(1명령씩 무기한 단일 스텝)"으로 전이했는지도 실시간으로
구분할 수 있게 한다.

## 3. 설계 (Design)

1. `Win32SharedLiveTelemetry`(`include/repiu/platform/win32/live_telemetry.h`)에 필드 3개를 추가한다:
   `aot_boundary_guest_eip`(마지막으로 캐시 미스를 겪은 게스트 주소), `aot_legacy_fallback_count`,
   `aot_last_fallback_address`. 구조체 변경이므로 `kWin32LiveTelemetryVersion`을 11 → 12로 올린다.
2. `HandleAotReentry`(`execution_trampoline.cpp:9986`)의 다음 두 지점에 라이브 미러링을 추가한다:
   * 캐시 미스 경계 진입 시(`win32_context->Eip = guest_address; ... BumpAotBoundaryCount(context);`
     직전/직후, `execution_trampoline.cpp:10027` 부근) — `aot_boundary_guest_eip`를 `guest_address`로 갱신.
   * legacy fallback 진입 시(`execution_trampoline.cpp:10077-10083` 부근, 기존
     `aot_legacy_fallback_count.fetch_add`/`aot_last_fallback_address.store` 옆) — 같은 값을
     `shared_live_telemetry`에도 `InterlockedIncrement`/`InterlockedExchange`로 미러링.
   기존 `BumpAotBoundaryCount`/`BumpAotReentryCount`(`execution_trampoline.cpp:9113-9131`) 패턴을
   그대로 따른다(로컬 atomic 갱신 + `shared_live_telemetry` 존재 시 미러링).
3. `PrintSnapshot`(`src/host/win32/supervisor_main.cpp:106`)의 `aot_boundary/reentry=` 출력 뒤에
   `aot_boundary_guest=0x.. legacy_fallback_count/addr=../0x..`를 추가로 출력한다.
4. 검증: `REPIU_EXECUTION_BACKEND=aot-dynamic`으로 `pumpit1`을 40~60초 재구동해, 동결 구간에서
   `aot_boundary_guest_eip`가 (a) 고정된 소수의 주소를 반복 방문하는지(캐시 스래싱/타이트 루프
   가설을 뒷받침), (b) 계속 증가하는 다양한 주소를 방문하는지(느린 정상 진행 가설을 뒷받침),
   (c) `aot_legacy_fallback_count`가 0에서 증가하는지(완전 legacy fallback 전이 여부)를 관찰한다.

## 4. 영향 범위 (Impact Scope)

순수 진단 계측 추가로 게스트 실행 동작을 변경하지 않는다(분기/레지스터 조작 없음, 텔레메트리
쓰기만 추가). `Win32SharedLiveTelemetry` 레이아웃이 바뀌므로 버전 상수를 함께 올려 이전 빌드의
공유 메모리 매핑과 섞이지 않게 한다.
