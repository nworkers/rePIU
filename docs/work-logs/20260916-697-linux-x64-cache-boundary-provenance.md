# Task 697 작업 로그 — Linux x64 transient cache 경계 분류

## 결과

`REPIU_AOT_CACHE_MAP_TRACE=<cache-address>` 읽기 전용 진단을 추가했습니다. 초기 및
정상 회수된 최종 AOT placement에서 지정 주소의 범위를 확인하고, 범위 안이면 현재와
직전 byte, reverse map, breakpoint provenance를 함께 출력합니다. 실행 정책, guest
semantics, cache 내용은 변경하지 않습니다.

Task 696의 `0x200695A3`은 재실행의 초기/최종 placement에 없었고, guest
`0x010F777C`도 30초 동안 동적 map이 65,632개 entry로 증가한 뒤까지 미매핑이었습니다.
따라서 이전 host cache 주소는 특정 동적 append 세대의 transient 주소이며 후속 실행의
정적 식별자가 아닙니다. 당시 provenance는 같은 경로가 재현되지 않아 소급 확정하지
않았습니다.

## 검증

* Linux x64 `repiu` 변경 object 컴파일 및 재링크 성공
* 기존 `execution_trampoline.cpp` extern 초기화 경고 외 컴파일 오류 없음
* Linux x64 `repiu_core_probe`: 27/27 통과
* 1초 및 30초 bounded `pumpit2a`: timeout cleanup `recovered=1`, `stopped=1`,
  `failure=0`
* `git diff --check`: 오류 없음

정상 게임 화면·입력 진행 또는 정상 종료는 확인되지 않았습니다.

## English

### Result

Added the read-only `REPIU_AOT_CACHE_MAP_TRACE=<cache-address>` diagnostic. It
checks the requested address against the initial and cleanly recovered final
AOT placements and, when in range, prints current/previous bytes, reverse maps,
and breakpoint provenance. It does not alter execution policy, guest semantics,
or cache contents.

Task 696's `0x200695A3` was absent from both reruns' initial/final placements,
and guest `0x010F777C` remained unmapped after the 30-second run grew the dynamic
map to 65,632 entries. The old host cache address was therefore transient to a
specific dynamic-append generation, not a static identifier for later runs.
Its original provenance was not assigned retrospectively because the same path
did not recur.

### Verification

* Linux x64 `repiu` changed object compiled and executable relinked
* No compile error beyond the existing `execution_trampoline.cpp` extern
  initialization warning
* Linux x64 `repiu_core_probe`: 27/27 passed
* One-second and 30-second bounded `pumpit2a` runs: timeout cleanup reported
  `recovered=1`, `stopped=1`, and `failure=0`
* `git diff --check`: clean

Normal game screen/input progress or normal termination was not confirmed.
