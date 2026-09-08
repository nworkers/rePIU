# Task 639 작업 로그: long-mode fallthrough 경계 복구

설계: [20260908-639](../design/20260908-639-long-mode-fallthrough-boundary-recovery.md)  
작업 지시: [20260908-639](../work-orders/20260908-639-long-mode-fallthrough-boundary-recovery.md)

## 한국어

runtime AOT 계층에 미해결 `kBlockFallthrough`의 중립화된 `INT3`와 guest target을
연결하는 `FindAotBlockFallthroughTarget`을 추가했습니다. AOT reentry는 기존 빠른
address-map 조회가 실패한 경우에만 이 metadata를 확인하고, 일치한 target을 기존 TF
원본 실행 경로로 넘깁니다.
provenance index에는 sentinel을 `kOtherPlannerFixup`으로 등록했습니다. opt-in fault
trace도 복원된 target을 표시합니다.

synthetic probe는 exact unresolved sentinel만 성공하고 인접 주소, resolved fixup, 다른
fixup kind는 거절함을 확인했습니다. 실제 trace는 `0x200829C5 -> 0x011C8E10`을
확인했고 네 번의 실행 모두 기존 `0x21000000` 2차 fault 없이 새 frontier까지
진행했습니다.

### 검증

* Linux x64 `repiu` 빌드: 통과
* Linux x64 core probe: `24/24`, `long_mode_fallthrough_lookup=true`
* 실제 `pumpit2a` 4회: 기존 `0x200829C5` 미처리 및 `0x21000000` fault 0회
* 새 frontier 4/4: guest `INT3 0x0138C781`, cache AV `0x20328014`, guest map `0x0138C783`

게임은 아직 정상 실행되지 않습니다. 다음 분석 대상은 왜 상위 제어 흐름이
`0x011C8E10`으로 향했는지와 그 영역이 코드로 유효한지입니다.

## English

Added runtime AOT `FindAotBlockFallthroughTarget`, connecting a neutralized
unresolved `kBlockFallthrough` `INT3` to its guest target. AOT reentry checks
this metadata only after the existing fast address-map lookup fails and feeds a
match into the existing TF original-execution path. The provenance index records the sentinel as
`kOtherPlannerFixup`, and the opt-in fault trace reports the recovered target.

The synthetic probe confirms that only the exact unresolved sentinel succeeds;
adjacent addresses, resolved fixups, and other fixup kinds are rejected. The
real trace confirms `0x200829C5 -> 0x011C8E10`, and all four runs reach the new
frontier without the former secondary `0x21000000` fault.

### Verification

* Linux x64 `repiu` build: passed
* Linux x64 core probe: `24/24`, `long_mode_fallthrough_lookup=true`
* Four real `pumpit2a` runs: zero unhandled `0x200829C5` or `0x21000000` faults
* New frontier in 4/4: guest `INT3 0x0138C781`, cache AV `0x20328014`, guest map `0x0138C783`

The game still does not run normally. The next analysis target is why upstream
control flow selected `0x011C8E10` and whether that region is valid code.
