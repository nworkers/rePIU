# Task 698 — Linux x64 guest-filtered AOT fault trace

## 목적

Task 697은 동적 cache의 절대 host 주소가 append 순서와 generation에 종속되어 후속
실행에서 재사용할 수 없음을 확인했습니다. 기존 `REPIU_AOT_FAULT_TRACE_ADDRESS`는
이미 알고 있는 cache 주소만 필터링하므로 transient 경계를 다시 포착하기 어렵습니다.
이번 작업은 같은 fault 순간의 reverse map을 이용해 guest 주소로 출력을 선택합니다.

## 설계

* `REPIU_AOT_FAULT_TRACE_GUEST_ADDRESS=<guest-address>`를 추가합니다.
* 기존 `REPIU_AOT_FAULT_TRACE=1`이 켜진 경우에만 동작합니다.
* exact, previous, block-fallthrough reverse map 중 하나가 지정 guest 주소와 일치하면
  기존 fault provenance 한 줄을 출력합니다.
* cache-address filter와 함께 지정하면 두 조건을 모두 만족해야 합니다.
* 출력 제한 16건과 기존 filter 미지정 동작은 유지합니다. guest 실행, dispatch,
  translation, cache 내용은 변경하지 않습니다.

## 검증

synthetic probe 추가보다 실제 fault trace의 기존 형식을 보존하는 데 초점을 둡니다.
Linux x64 본체 빌드, core probe 27개 그룹, 존재하지 않는 guest filter가 초기 fault
출력을 억제하는 bounded 실행으로 검증합니다.

## English

### Purpose

Task 697 confirmed that an absolute host address in the dynamic cache depends on
append order and generation and cannot be reused in a later run. The existing
`REPIU_AOT_FAULT_TRACE_ADDRESS` can only select an already-known cache address,
making a transient boundary difficult to recapture. This task selects output by
the reverse-mapped guest address at the same fault instant.

### Design

* Add `REPIU_AOT_FAULT_TRACE_GUEST_ADDRESS=<guest-address>`.
* It is active only with `REPIU_AOT_FAULT_TRACE=1`.
* Print the existing provenance line when the exact, previous, or block
  fallthrough reverse map matches the requested guest address.
* When combined with the cache-address filter, both filters must match.
* Preserve the 16-line limit and behavior with no guest filter. Do not alter
  guest execution, dispatch, translation, or cache contents.

### Verification

Focus on preserving the existing live fault-trace format rather than adding a
synthetic probe. Verify the Linux x64 executable build, all 27 core-probe
groups, and a bounded run where a nonexistent guest filter suppresses initial
fault output.
