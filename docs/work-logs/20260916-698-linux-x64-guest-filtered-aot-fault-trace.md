# Task 698 작업 로그 — Linux x64 guest-filtered AOT fault trace

## 결과

`REPIU_AOT_FAULT_TRACE_GUEST_ADDRESS`를 기존 AOT fault provenance trace에
추가했습니다. exact, previous, block-fallthrough guest 주소 가운데 하나가 filter와
일치할 때만 출력하며, cache-address filter가 함께 있으면 두 조건을 모두 적용합니다.
일치하지 않는 fault는 16건 출력 제한을 소비하지 않습니다. 실행·dispatch·translation
정책은 변경하지 않았습니다.

## 검증

* `execution_trampoline.cpp` 단일 object 컴파일 및 Linux x64 `repiu` 재링크 성공
* 기존 extern 초기화 경고 외 컴파일 오류 없음
* Linux x64 `repiu_core_probe`: 27/27 통과
* 실제 `pumpit2a`, guest `0x010F1728`: cache `0x20000005` provenance 한 줄 선택
* 실제 `pumpit2a`, guest `0xDEADBEEF`: AOT fault provenance 출력 0건

두 번째 실행은 timeout cleanup 회수에 실패했으며 정상 게임 종료는 확인되지
않았습니다. 목표 `0x010F777C` 경로도 이번 실행에서는 재현되지 않았습니다.

## English

### Result

Added `REPIU_AOT_FAULT_TRACE_GUEST_ADDRESS` to the existing AOT fault-provenance
trace. Output is selected when the exact, previous, or block-fallthrough guest
address matches. If the cache-address filter is also present, both conditions
apply. Nonmatching faults do not consume the 16-line output limit. Execution,
dispatch, and translation policies are unchanged.

### Verification

* Compiled the single `execution_trampoline.cpp` object and relinked Linux x64
  `repiu`
* No compile error beyond the existing extern-initialization warning
* Linux x64 `repiu_core_probe`: 27/27 passed
* Real `pumpit2a`, guest `0x010F1728`: selected one provenance line at cache
  `0x20000005`
* Real `pumpit2a`, guest `0xDEADBEEF`: zero AOT fault-provenance lines

The second run failed timeout cleanup recovery and is not evidence of normal
game termination. The target `0x010F777C` path also did not recur in these runs.
