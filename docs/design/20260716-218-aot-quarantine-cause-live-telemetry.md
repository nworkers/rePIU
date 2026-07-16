# AOT quarantine 원인(페이지/쓰기 출처) 실시간 계측 설계
# Design: Live Telemetry for the AOT Quarantine Cause (Page/Write Provenance)

## 1. 배경 (Background)

Task 217은 guest `0x030EE1DA`(RET)가 속한 페이지가 부팅~LINEXE 초기화 구간(8~16초)에 36건의
same-page quarantine 이벤트 중 하나로 한 번 격리된 뒤 계속 그 상태로 남아, 이후 이 함수가
호출될 때마다 매번 느린 `HandleAotReentry` boundary 경로만 탄다는 것을 확정했다. 다음으로
확인해야 할 것은 quarantine이 **오탐인지 DOS4GW 자체의 정상 self-modify(cross-segment thunk
자기 패치)인지**이며, 이를 위해서는 (a) quarantine된 페이지 번호(`aot_last_retired_page`)와
(b) 그 페이지에 쓰기를 가한 코드 주소(`aot_last_code_write_source`/`aot_last_code_write_
destination`, `HandleAotGuestCodeWriteCompletion` 계열에서 이미 계산됨, `execution_trampoline.
cpp:9562, 9579-9582`)가 필요하다. 세 필드 모두 `ThreadContext`에 이미 존재하지만 실행 종료 후
요약값으로만 노출되어, 실행 중에는 확인할 수 없다.

## 2. 목적 (Objective)

세 필드를 `aot_boundary_guest_eip`(Task 216)와 같은 방식으로 실시간 미러링해, quarantine이
발생한 정확한 페이지와 그 원인이 된 쓰기 명령의 guest 주소(소스/목적지)를 재구동 중에 직접
확인한다. 이 정보로 quarantine 소스가 `docs/analysis/20260715-209-aot-dynamic-import-stub-
storm.md`가 역어셈블한 DOS4GW cross-segment thunk 자기 패치(`0x010F342C` 부근,
`mov byte ptr [edi], 0xE9`)와 같은 주소대인지 대조한다.

## 3. 설계 (Design)

1. `Win32SharedLiveTelemetry`에 `aot_last_retired_page`, `aot_last_code_write_source`,
   `aot_last_code_write_destination`을 추가하고 `kWin32LiveTelemetryVersion`을 13 → 14로
   올린다.
2. `execution_trampoline.cpp`의 기존 세 `store` 호출 지점(`:9562`, `:9579`, `:9581`)에
   `InterlockedExchange` 미러링을 추가한다.
3. `PrintSnapshot`에 `retire_page=0x.. write_src/dst=0x../0x..`를 추가한다.
4. 검증: 40초 재구동으로 quarantine이 발생하는 8~16초 구간에서 세 필드 값을 관찰한다.

## 4. 이후 계획 (Follow-up)

값을 확인한 뒤, 이것이 오탐이면 오탐 조건을 수정하고, DOS4GW 자체의 정상 self-modify이면
quarantine된 페이지에서도 이미 알려진 반환 thunk가 있는 명령(RET 등)은 boundary 경로보다
먼저 재시도하도록 `HandleAotReentry`를 조정하는 실험을 로컬(커밋하지 않음)로 수행해 progress가
재개되는지 인과적으로 확인한다.
