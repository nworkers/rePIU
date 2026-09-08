# Task 637 작업 로그: legacy fallback의 HLE 후 cache 재진입

설계: [20260908-637](../design/20260908-637-legacy-fallback-post-hle-reentry.md) ·
작업 지시: [20260908-637](../work-orders/20260908-637-legacy-fallback-post-hle-reentry.md) ·
분석: [linux-port-frontier 3.74](../analysis/linux-port-frontier.md)

## 한국어

### 원인

최초 미매핑 target `0x010F920C`가 `aot_legacy_fallback=true`로 전환한 뒤에도
`TryResumeAotAfterHandledHle`는 `aot_reentry_pending`만 자격으로 인정했습니다.
수정 전에는 `0x010F1E0F` HLE가 다음 EIP `0x010F1E13`을 계산하고도
`pending=0`으로 거절되어, 유효한 cache entry가 있는 `0x010F1E17` direct CALL까지
원본 코드를 long mode에서 실행했습니다.

### 구현

HLE 후 cache 재진입의 초기 gate가 `aot_reentry_pending ||
aot_legacy_fallback`을 허용하도록 수정했습니다. 이후 segment-write, arena,
quarantine, exact cache lookup과 span preflight는 바꾸지 않았습니다. 성공 시 기존
경로가 pending, legacy, TF와 single-step trace 상태를 함께 해제합니다.

opt-in `REPIU_AOT_HLE_REENTRY_TRACE`에는 `legacy` 상태를 추가했습니다.
`0xFFFFFFFF`는 모든 주소를 선택하며, 일반 32-event 제한 뒤에도 legacy recovery
후보는 출력하여 집중 추적할 주소를 찾을 수 있게 했습니다.

### 검증

Linux x64 Debug `repiu`와 `repiu_core_probe`를 빌드했습니다. core probe 결과는
다음과 같습니다.

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
```

실제 `pumpit2a` 집중 추적은 최초로 안전한 기존 cache entry에 도달한 HLE가
`0x010F1D79`임을 확인했습니다.

```text
[repiu-aot-fallback] #1 guest=0x010F920C mapped=0
[repiu-hle-reentry] stage=entry handled=0x010F1D79 current=0x010F1D7A pending=0 legacy=1
[repiu-hle-reentry] stage=cache-hit-span-safe cache_target=0x200026B6
[repiu-hle-reentry] stage=resumed pending=0 legacy=0 cache_target=0x200026B6
```

세 번 반복한 실행에서 `0x010F1E17` fault는 재발하지 않았습니다. 새 frontier는
동적 cache `0x200829C5` breakpoint 뒤 `0x21000000` 실행 SIGSEGV이며, guest 역매핑과
전이 원인은 다음 작업 범위입니다. 게임은 아직 정상 실행되지 않습니다.

## English

### Cause

After the first unmapped target `0x010F920C` set
`aot_legacy_fallback=true`, `TryResumeAotAfterHandledHle` still recognized only
`aot_reentry_pending` as eligibility. Before the fix, HLE at `0x010F1E0F`
computed next EIP `0x010F1E13` but rejected it as `pending=0`, leaving original
code to execute in long mode through the mapped direct call at `0x010F1E17`.

### Implementation

The initial post-HLE cache re-entry gate now admits
`aot_reentry_pending || aot_legacy_fallback`. All later segment-write, arena,
quarantine, exact-cache lookup, and span-preflight gates are unchanged. On
success, the existing path clears pending, legacy, TF, and single-step trace
state together.

The opt-in `REPIU_AOT_HLE_REENTRY_TRACE` now prints `legacy` state. Value
`0xFFFFFFFF` selects every address, and legacy recovery candidates remain
visible after the ordinary 32-event limit so a focused address can be found.

### Verification

Linux x64 Debug `repiu` and `repiu_core_probe` were built. The core probe passed
all 24 enabled groups, with the two expected assembly-only skips.

Focused real `pumpit2a` tracing confirms that HLE at `0x010F1D79` is the first
one to reach a safe existing cache entry. It advances to `0x010F1D7A`, passes
span preflight, resumes at `0x200026B6`, and clears legacy state.

Three repeated runs no longer reproduce the `0x010F1E17` fault. The new
frontier is an execution SIGSEGV at `0x21000000` after a breakpoint in dynamic
cache `0x200829C5`; reverse guest mapping and transfer cause are follow-up
work. The game still does not run normally.
