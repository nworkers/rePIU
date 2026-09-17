# Task 695 작업 로그 — Linux x64 guest-entry 범위 trace

## 결과

기존 exact guest-entry trace에 선택적 inclusive 끝 주소
`REPIU_LINUX_X64_GUEST_ENTRY_TRACE_END`를 추가했습니다. cache/guest entry 또는 exit가
범위 안에 들면 기존 상태 line을 최대 32건 출력하며, 끝 주소가 없거나 시작보다 작으면
기존 exact 동작을 유지합니다.

이 trace로 `0x010F0D96` RSP 손상의 시작점을 확정했습니다. cache breakpoint
`0x20001805`가 guest `0x010EFEB8` fatal marker로 역변환된 뒤
`HandleOriginalFatalBreakpoint`가 TF 없이 raw `0x010EFEB9`로 복귀합니다. 이어지는
`PUSH EDX; CALL 0x010F0D68`이 cache lowering을 우회하는 것이 다음 수정 대상입니다.

조사 중 signal RF 제거와 standalone `SUB ESP` HLE를 각각 시험했지만 live fault가
동일하게 재현되어 모두 되돌렸습니다.

## 검증

* WSL Ubuntu 24.04 Linux x64 Debug `repiu_core_probe`: 27/27 통과
* Linux x64 Debug `repiu`: 빌드 성공
* `0x010F0D00`–`0x010F0DA0` 범위: raw access-fault 연쇄 확인
* `0x010EFE00`–`0x010EFF00` 범위: `fatal-breakpoint`의 raw continuation 확인
* live 실행은 진단 작업이므로 기존 `0x010F0D96` fault가 계속 발생하며, 수정은 다음
  작업으로 넘깁니다.

## English

### Result

Added optional inclusive end address
`REPIU_LINUX_X64_GUEST_ENTRY_TRACE_END` to the existing exact guest-entry
trace. It prints up to 32 existing state lines when a cache/guest entry or exit
falls in range, while an absent or smaller end preserves exact behavior.

The trace identified the start of the `0x010F0D96` RSP corruption. Cache
breakpoint `0x20001805` reverse-maps to the guest fatal marker at `0x010EFEB8`,
then `HandleOriginalFatalBreakpoint` resumes at raw `0x010EFEB9` without TF.
The following `PUSH EDX; CALL 0x010F0D68` bypasses cache lowering and is the
next fix target.

Signal RF clearing and standalone `SUB ESP` HLE were each tested during the
investigation, did not change the live fault, and were reverted.

### Verification

* WSL Ubuntu 24.04 Linux x64 Debug `repiu_core_probe`: 27/27 passed
* Linux x64 Debug `repiu`: build passed
* `0x010F0D00`–`0x010F0DA0`: confirmed the raw access-fault chain
* `0x010EFE00`–`0x010EFF00`: confirmed the fatal-breakpoint raw continuation
* The live run intentionally retains the existing `0x010F0D96` fault because
  this task is diagnostic; the fix is the next task.
