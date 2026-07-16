# AOT 페이지 retire/quarantine 실시간 계측 설계
# Design: Live Telemetry for AOT Page Retire/Quarantine Counters

## 1. 배경 (Background)

Task 216은 `pumpit1`의 `aot-dynamic` 동결 구간(`dispatch` 정지, `aot_boundary/reentry`만 계속
증가)에서 `aot_boundary_guest_eip`가 guest `0x030EE1DA`(`RET`) 한 주소에 30초 이상 고정됨을
확인했다. 정적 AOT 캐시 플랜에는 이 주소를 위한 반환 전용 디스패치 thunk가 이미 있는데도
`aot_return_dispatch_count`는 이 구간에서 전혀 증가하지 않아, 이 RET가 속한 코드 페이지의 캐시
엔트리가 self-modifying-code write-watch 등으로 반복 retire/quarantine 되고 있을 가능성을
가설로 남겼다(`docs/analysis/current-execution-frontier.md` Task 216 항목).

`aot_retired_entry_trap_count`/`aot_quarantine_count`/`aot_page_retire_attempt_count`/
`aot_page_retire_success_count`(`execution_trampoline.cpp:168-174`)는 이미 `ThreadContext`에
존재하지만 `Win32MinimalExecutionAttempt` 요약 결과에만 복사되고(`execution_trampoline.cpp:1249-
1263`) 실시간 공유 텔레메트리로는 미러링되지 않는다 — 프로세스가 멈춘 것처럼 보이는 구간에서는
이 값을 확인할 방법이 없다.

## 2. 목적 (Objective)

위 네 카운터를 `aot_boundary_count`/`aot_reentry_count`와 같은 방식(로컬 atomic 갱신 + 존재 시
`shared_live_telemetry` 미러링)으로 실시간 노출해, Task 216이 남긴 가설(코드 페이지 retire
스래싱)을 정지 구간 도중에 직접 확인한다.

## 3. 설계 (Design)

1. `Win32SharedLiveTelemetry`에 `aot_retired_entry_trap_count`, `aot_quarantine_count`,
   `aot_page_retire_attempt_count`, `aot_page_retire_success_count`를 추가하고
   `kWin32LiveTelemetryVersion`을 12 → 13으로 올린다.
2. 각 카운터의 기존 `fetch_add` 호출 지점(`execution_trampoline.cpp:9507`, `:9516`, `:9522`,
   `:9634`, `:10012`)에 `InterlockedIncrement`를 추가해 동일한 값을 `shared_live_telemetry`에도
   반영한다. 기존 `BumpAotBoundaryCount`/`BumpAotReentryCount`와 같은 관례를 따른다.
3. `PrintSnapshot`(`src/host/win32/supervisor_main.cpp`)의 `aot_boundary_guest=`/
   `legacy_fallback_count/addr=` 출력 뒤에 `retire_attempt/success/trap/quarantine=` 필드를
   추가한다.
4. 검증: `REPIU_EXECUTION_BACKEND=aot-dynamic pumpit1`을 40초 재구동해, 동결 구간에서 네 카운터가
   (a) 계속 증가하면 retire/quarantine 스래싱 가설을 확정, (b) 0에서 멈춰 있으면 스래싱 가설을
   기각하고 "정적 계획상 존재하는 반환 thunk가 애초에 로드/연결되지 않았다" 등 다른 가설로
   전환한다.

## 4. 영향 범위 (Impact Scope)

순수 진단 계측 추가로 게스트 실행 동작을 변경하지 않는다. `Win32SharedLiveTelemetry` 레이아웃이
바뀌므로 버전 상수를 함께 올린다.
