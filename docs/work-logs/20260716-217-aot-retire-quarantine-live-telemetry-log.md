# AOT 페이지 retire/quarantine 실시간 계측 작업 로그
# Work Log: Live Telemetry for AOT Page Retire/Quarantine Counters

## 1. 개요 (Overview)

Task 216이 남긴 가설("guest `0x030EE1DA`가 속한 코드 페이지가 반복 retire/quarantine되어 매번
느린 boundary 경로로 빠진다")을 검증하기 위해, `aot_page_retire_attempt_count`/
`aot_page_retire_success_count`/`aot_retired_entry_trap_count`/`aot_quarantine_count`를 실시간
공유 텔레메트리에 미러링했다(설계: `docs/design/20260716-217-aot-retire-quarantine-live-
telemetry.md`).

## 2. 구현 (Implementation)

1. `include/repiu/platform/win32/live_telemetry.h`: 네 필드 추가, 버전 12 → 13.
2. `src/platform/win32/execution_trampoline.cpp`: `BumpAotBoundaryCount`/`BumpAotReentryCount`와
   같은 관례로 `BumpAotPageRetireAttemptCount`/`BumpAotPageRetireSuccessCount`/
   `BumpAotRetiredEntryTrapCount`/`BumpAotQuarantineCount` 헬퍼를 추가하고, 기존 5개
   `fetch_add` 호출 지점(`RequestAotGuestPageRetirement` 성공/시도, 두 곳의 quarantine 판정,
   `HandleAotReentry`의 retired-entry trap)을 이 헬퍼 호출로 교체했다.
3. `src/host/win32/supervisor_main.cpp`의 `PrintSnapshot`에
   `retire_attempt/success/trap/quarantine=` 출력을 추가했다.
4. `repiu_loader_win32`/`repiu_supervisor_win32` Debug 재빌드 성공.

## 3. 검증 결과 (Verification Results)

`REPIU_EXECUTION_BACKEND=aot-dynamic`으로 `pumpit1`을 40초 재구동했다(`task217-verify-40s.log`).

| elapsed_ms | dispatch | aot_boundary_guest_eip | retire_attempt/success/trap/quarantine |
|---|---|---|---|
| 10297 | 45993/45993 | `0x045D0300` | 0/1/1/1 (quarantine 시작) |
| 15453 | 46832/46832 | `0x045D0478` | 0/24/24/30 |
| 16469 | 47326/47326 | `0x030B1A73` | 0/24/24/36 (이후 불변) |
| 21640 (동결 시작) | 56857/56857 | `0x030EE1DA` | 0/24/24/36 |
| 40047 (종료) | 56857/56857 | `0x030EE1DA` | 0/24/24/36 |

Task 216의 스래싱 가설과 달리, retire/quarantine 네 카운터는 동결이 시작되기 약 5초 전
(`elapsed_ms≈16469`)에 이미 `0/24/24/36`으로 정지했고 동결 구간(21.6초~40초) 내내 전혀 움직이지
않았다. 즉 이 구간에서 "반복적으로" 페이지가 무효화/재해석되고 있는 것이 아니다.

`ResolveAotTransferTarget`(`execution_trampoline.cpp:9588`)을 재확인한 결과, quarantine된
페이지는 `IsWin32AotGuestPageQuarantined` 체크가 캐시 조회보다 먼저 와서 재시도 없이 즉시
`false`를 반환하도록 되어 있다(`:9602-9606`) — 한 번 quarantine되면 새 이벤트 없이 구조적으로
영구히 캐시에서 배제된다는 뜻이다.

## 4. 결론 (Conclusion)

Task 216의 "반복 retire/quarantine 스래싱" 가설은 기각한다. 대신: `0x030EE1DA`가 속한 페이지는
부팅~LINEXE 초기화 구간(8~16초)에 발생한 36건의 same-page quarantine 이벤트 중 하나로 **한 번**
격리된 뒤 계속 그 상태로 남아 있고, 그 이후 이 함수가 호출될 때마다(추정상 반복 호출 루프) 매번
`HandleAotReentry`의 느린 boundary 경로만 타는 것으로 보인다. `docs/analysis/current-execution-
frontier.md`에 Task 217 항목을 추가해 Task 216의 가설을 정정했다.

## 5. 다음 단계 (Next Steps)

1. `aot_last_retired_page`를 같은 방식으로 라이브 미러링해 `0x030EE1DA`의 실제 페이지 번호와
   quarantine 발생 시점을 확정한다.
2. 이 함수의 호출자를 역추적해, 반복 호출이 게임의 정상 진행 중 일부인지(단순 성능 저하) 아니면
   2026-07-14 항목이 남긴 "게임 내부 tick/플래그 폴링 무한 대기" 가설과 같은 것인지 구분한다.
3. quarantine 원인이 오탐인지 DOS4GW 자체의 정상 thunk 자기 패치인지에 따라, (a) 오탐 조건 수정
   또는 (b) `HandleAotReentry`가 quarantine된 페이지에서도 자주 재진입되는 명령(RET 등)에는
   전용 thunk를 우선 재시도하도록 순서를 조정하는 것 중 하나를 다음 구현으로 선택한다.

## 6. 참고 (References)

* 로그: `task217-verify-40s.log`(세션 스크래치패드, UTF-16LE → `task217-verify-40s.utf8.txt`)
* 관련 문서: `docs/design/20260716-217-aot-retire-quarantine-live-telemetry.md`,
  `docs/analysis/current-execution-frontier.md`(Task 216, Task 217 항목)
