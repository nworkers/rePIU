# Task 415 작업 지시 — 세대 실패 벌칙을 주소로 좁히기

설계: [20260804-415](../design/20260804-415-generation-failure-address-scope.md) ·
결과: [작업 로그](../work-logs/20260804-415-generation-failure-address-scope.md)

## 변경 파일

| # | 파일 | 변경 |
|---|---|---|
| 1 | `src/platform/win32/aot/aot_generation_failure_policy.h` (신규) | counter 3개 선언. **ThreadContext 의존 없음**(호스트가 읽기 위해) |
| 2 | `src/platform/win32/aot/aot_runtime_dispatch.cpp` | 실패 주소 집합, 시도 전 조회·스킵, 격리 대신 주소 기록, 상한·스위치 |
| 3 | `src/platform/win32/aot/aot_runtime_dispatch.h` | 위 헤더로 옮겼다는 주석만 |
| 4 | `src/host/win32/main.cpp` | 정책 counter 로그 |

## 구현 규칙

* **재시도 폭주를 만들지 않습니다.** 실패한 주소는 다시 시도하지 않습니다.
* 상한(256) 초과 또는 `REPIU_AOT_QUARANTINE_ON_GENERATION_FAILURE=1`이면 예전 동작으로
  물러섭니다.
* `thread_context.h`·`execution_trampoline.h`·`aot_code_cache_win32.h`는 건드리지
  않습니다(전체 재빌드 40분).
* 집합은 guest thread 전용이므로 lock 없이 둡니다.

## 검증

1. `cl /Zs` 후 증분 Release 빌드.
2. A/B `page`(예전) 3회 대 `address`(신규) 5회, 60초, EEPROM 실행별 격리.
3. 기계 확인: `quarantines` 0, 정책 counter가 주소 1개·스킵 5~9회.
4. 회귀 확인: 프레임 중앙값 대비.

---

# Task 415 Work Order — narrow the generation-failure penalty to an address

Design: [20260804-415](../design/20260804-415-generation-failure-address-scope.md) ·
Outcome: [work log](../work-logs/20260804-415-generation-failure-address-scope.md)

## Files

A new `aot/aot_generation_failure_policy.h` declares the three counters with **no
ThreadContext dependency** so the host can read them; `aot_runtime_dispatch.cpp` holds the
failed-address set, the pre-attempt lookup and skip, the record-instead-of-quarantine
policy, the 256-address limit, and the switch; `aot_runtime_dispatch.h` keeps only a
pointer comment; `main.cpp` logs the counters.

## Implementation rules

No retry storm — a failed address is never attempted again. Past the limit, or with
`REPIU_AOT_QUARANTINE_ON_GENERATION_FAILURE=1`, the old behaviour returns. None of
`thread_context.h`, `execution_trampoline.h`, or `aot_code_cache_win32.h` may be touched,
since each costs a forty-minute rebuild. The set is guest-thread only and needs no lock.

## Verification

Syntax-check, incremental Release build, then A/B of three `page` runs against five
`address` runs at 60 seconds with the EEPROM isolated per run; confirm zero quarantines and
the policy counters, and compare frame medians for regression.
