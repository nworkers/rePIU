# Task 639 설계: long-mode fallthrough 경계 복구

## 한국어

### 배경과 목표

Task 638은 첫 미처리 SIGTRAP `0x200829C5`가 동적 AOT append 끝의 미해결
`kBlockFallthrough` 5바이트 slot임을 확인했습니다. long-mode emitter는 cache 밖 target의
`E9 + rel32`를 첫 바이트 `INT3`로 중립화하지만, 이 slot은 address map에 속하지 않아
기존 AOT reentry가 guest 주소를 복원하지 못합니다.

목표는 fixup metadata에서 이 sentinel의 guest target을 복원하고 기존 fail-closed TF
원본 실행 경로로 전달하는 것입니다. 게임 코드를 재구현하거나 특정 주소를 하드코딩하지
않습니다.

### 설계

플랫폼 공용 runtime AOT code-cache 계층에 `FindAotBlockFallthroughTarget`을 둡니다. 다음 조건을
모두 만족하는 fixup만 인정합니다.

* kind가 `kBlockFallthrough`
* fixup이 미해결 상태
* `cache_address + 1 == cache_patch_offset`
* 주소와 출력 포인터가 유효함

`cache_patch_offset`은 원래 `E9` 다음 rel32의 시작이므로 그보다 한 바이트 앞이
중립화된 `INT3`입니다. 찾으면 `guest_target`을 반환합니다. `HandleAotReentry`는 기존
DBT direct-edge fallback 및 빠른 address-map 조회가 실패한 경우에만 이 helper를
사용하고, 이후에는 기존 boundary 처리와 동일하게 guest target에서 TF 실행을
시작합니다. provenance index도 이 sentinel을 `kOtherPlannerFixup`으로 기록합니다.

```mermaid
flowchart LR
    I["unresolved fallthrough INT3"] --> F["find matching kBlockFallthrough fixup"]
    F --> T["restore guest_target"]
    T --> B["existing AOT boundary path"]
    B --> O["single-step original guest code"]
    O --> R["safe cache reentry when available"]
```

잘못된 주소, resolved fixup, 다른 fixup kind는 거절합니다. 따라서 일반 cache breakpoint의
의미는 바뀌지 않습니다.

기존 opt-in AOT fault trace에는 이 조회 결과를 함께 기록하여 실제 sentinel이 선택한
guest target을 검증할 수 있게 합니다.

### 검증

core probe에 synthetic placement를 추가하여 정확한 sentinel만 target을 반환하고
인접 주소, resolved fixup, 다른 kind는 거절함을 확인합니다. Linux x64 `repiu`와 core
probe를 빌드한 뒤 실제 `pumpit2a`에서 기존 `0x200829C5`/`0x21000000` 연쇄가 사라지는지
확인하고 새 frontier를 기록합니다.

## English

### Background and goal

Task 638 identified the first unhandled SIGTRAP at `0x200829C5` as the
unresolved five-byte `kBlockFallthrough` slot at the end of a dynamic AOT
append. The long-mode emitter neutralizes an out-of-cache `E9 + rel32` by
changing its first byte to `INT3`, but this slot has no address-map entry, so
existing AOT reentry cannot recover a guest address.

Recover the sentinel's guest target from fixup metadata and feed it into the
existing fail-closed TF original-execution path. No game logic is reimplemented
and no game address is hard-coded.

### Design

Add platform-neutral `FindAotBlockFallthroughTarget` to the runtime AOT code-cache
layer. It accepts only an unresolved `kBlockFallthrough` whose
`cache_address + 1` equals `cache_patch_offset`. The patch offset names the
rel32 after the original `E9`, so the preceding byte is the neutralized
`INT3`. On a match it returns `guest_target`.

`HandleAotReentry` consults this helper only after the existing DBT direct-edge
and fast address-map lookups fail, then uses the unchanged boundary path to
single-step from the guest target. The provenance index records this sentinel as
`kOtherPlannerFixup`. Wrong addresses, resolved fixups, and other fixup kinds
remain rejected.

The existing opt-in AOT fault trace also reports this lookup result so the
guest target selected by a real sentinel can be verified.

### Verification

Add a synthetic core-probe placement proving that only the exact unresolved
sentinel resolves. Build Linux x64 `repiu` and the core probe, then run real
`pumpit2a` to determine whether the former `0x200829C5`/`0x21000000` chain is
removed and record the new frontier.
