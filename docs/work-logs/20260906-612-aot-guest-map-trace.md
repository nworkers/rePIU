# Task 612 작업 로그 — Linux x64 AOT guest 주소 맵 세대 추적

## 한국어

### 작업 결과

`REPIU_AOT_GUEST_MAP_TRACE` 진단을 추가하여 Linux x64 AOT 배치의 초기 상태와
guest/translation worker 종료 후 최종 상태를 비교할 수 있게 했습니다. 지정한
relocated-base 상대 offset마다 exact/covering map entry, cache offset,
emitted bytes, direct-edge fixup, 동적 세대의 inactive 상태를 읽기 전용으로
출력합니다. 환경 변수가 없으면 기존 실행 경로와 출력은 변하지 않습니다.

### 확인된 증거

* 초기 map entry 수는 `51866`, 최종 map entry 수는 `55194`였습니다.
* `0x010F1D74` allocator 본체, `0x010F4FE8`, `0x010F5134`, `0x010F849D`
  helper 주소는 최종 상태에서 각각 3개 세대가 확인되었습니다.
* 실제 allocator 호출은 `0x010F1E17`이며, 각 세대에서
  `direct-call -> 0x010F4FE8` fixup이 `resolved=1`로 확인되었습니다.
  초기 cache patch는 `0x26F0`, 동적 세대는 `0x50788`, `0x51AA9`였습니다.
* 호출 source의 emitted bytes는
  `458D7FFC41C7071C1E0F01E95E000000`이고, helper 본체는
  `458D7FFC41891F`로 확인되었습니다. 호출 직후 `TEST EAX,EAX`인
  `0x010F1E1C`도 모든 세대에 매핑되었습니다.
* allocator prologue watch는 동적 cache 주소에서 3회 관찰되었습니다.
  따라서 초기 AOT entry에만 설치한 sentinel이 hit하지 않은 것은 호출 경로
  전체가 실행되지 않았다는 증거가 아닙니다.
* 기본 `pumpit2a` 실행은 exit code `0`, 기존
  `Not enough memory to allocate file structures` 메시지, DOS
  `AX=4C01` 종료를 유지했으며 SIGSEGV/SIGILL은 발생하지 않았습니다.
* `repiu_core_probe`는 `24/24`를 통과했습니다.

### 결론과 다음 과제

`allocator/helper가 AOT map에 없거나 direct-call fixup이 끊겼다`는 가설은
기각되었습니다. 현재 미확정인 것은 동적 코드 세대에서 direct call이 실제로
helper까지 도달하는지, 그리고 helper가 반환하는 값이 무엇인지입니다.
다음 작업에서는 host stack을 guest instruction으로 직접 실행하지 않도록
안전한 post-call 지점 또는 dynamic-generation별 slot-level trace를 사용해
이 반환값을 관찰해야 합니다. 이번 작업에서는 guest EIP, free-list, selector
limit, stack, memory contract를 변경하지 않았습니다.

## English

### Result

Added the opt-in `REPIU_AOT_GUEST_MAP_TRACE` diagnostic. It compares the
initial AOT placement with the final placement after the guest and translation
workers stop. For each relocated-base-relative offset it reports exact or
covering map entries, cache offsets, emitted bytes, direct-edge fixups, and the
inactive state of dynamically generated generations. With the variable unset,
the existing execution path and output remain unchanged.

### Evidence

* The initial map contained `51866` entries and the final map contained
  `55194` entries.
* The allocator body at `0x010F1D74` and helpers at `0x010F4FE8`,
  `0x010F5134`, and `0x010F849D` each had three generations in the final map.
* The actual allocator call source is `0x010F1E17`. In every generation, its
  `direct-call -> 0x010F4FE8` fixup reported `resolved=1`; the initial cache
  patch was `0x26F0`, followed by dynamic patches `0x50788` and `0x51AA9`.
* The call source emitted
  `458D7FFC41C7071C1E0F01E95E000000`, while the helper emitted
  `458D7FFC41891F`. The post-call `TEST EAX,EAX` at `0x010F1E1C` was also
  mapped in every generation.
* The allocator prologue watch fired three times at dynamic cache addresses.
  A sentinel installed only in the initial AOT entry therefore cannot prove
  that the call path was not executed.
* The default `pumpit2a` run retained exit code `0`, the existing
  `Not enough memory to allocate file structures` message, and DOS
  `AX=4C01` termination, with no SIGSEGV or SIGILL.
* `repiu_core_probe` passed `24/24` checks.

### Conclusion and next task

The hypothesis that the allocator/helper was absent from the AOT map or that
the direct-call fixup was unresolved is rejected. It remains unresolved whether
the dynamic code generations actually reach the helper and what value the
helper returns. The next task should observe that return value through a safe
post-call point or a per-generation slot-level trace, without executing guest
stack instructions directly on the host stack. This task did not change guest
EIP, the free list, selector limits, the stack, or the memory contract.
