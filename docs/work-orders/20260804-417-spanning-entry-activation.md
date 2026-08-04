# Task 417 작업 지시 — 걸친 요청 항목 활성화

설계: [20260804-417](../design/20260804-417-spanning-entry-activation.md) ·
결과: [작업 로그](../work-logs/20260804-417-spanning-entry-activation.md)

## 변경 파일

| # | 파일 | 변경 |
|---|---|---|
| 1 | `src/platform/win32/aot_code_cache_win32.cpp` | `AotStrictSpanningEntryEnabled`, `EntrySpansQuarantinedPage`, append 루프의 활성 판정 완화 |
| 2 | `src/platform/win32/aot/aot_generation_failure_policy.h`, `aot_runtime_dispatch.cpp` | `AotSpanningEntryActivationCount` |
| 3 | `src/host/win32/main.cpp` | 정책 로그에 `spanning-activations` 추가 |

## 구현 규칙

* 완화는 **요청 항목 하나에만** 적용합니다(`entry.guest_address == guest_entry`).
* **quarantined 페이지를 걸치면 여전히 거부**합니다.
* 길이가 0이거나 주소 공간을 넘으면 거부합니다(보수적으로 실패 쪽).
* `REPIU_AOT_STRICT_SPANNING_ENTRY=1`로 예전 규칙을 되살릴 수 있어야 합니다.
* 널리 포함되는 헤더(`thread_context.h`·`execution_trampoline.h`·`aot_code_cache_win32.h`)는
  건드리지 않습니다.

## 검증

1. `cl /Zs` 후 증분 Release 빌드.
2. A/B strict 5회 대 relaxed 8회, 60초, EEPROM 실행별 격리.
3. 기계 확인: relaxed에서 `generation failure addresses` 0, `spanning-activations` ≥ 1.
4. 회귀: pumpit1 60초 1회(오늘 기준선 2,865 / 2,735 프레임).

---

# Task 417 Work Order — activate the straddling requested entry

Design: [20260804-417](../design/20260804-417-spanning-entry-activation.md) ·
Outcome: [work log](../work-logs/20260804-417-spanning-entry-activation.md)

## Files

`aot_code_cache_win32.cpp` gains the switch helper, the quarantine span test, and the
relaxed activation in the append loop; the policy header and `aot_runtime_dispatch.cpp` gain
the counter; `main.cpp` prints it.

## Implementation rules

The relaxation applies to the **requested entry only**, a **quarantined** span still
refuses, a zero length or an address-space overflow refuses, and
`REPIU_AOT_STRICT_SPANNING_ENTRY=1` restores the old rule. None of the widely included
headers may be touched.

## Verification

Syntax-check, incremental Release build, A/B of five strict against eight relaxed runs at
60 seconds with the EEPROM isolated per run, confirmation that relaxed reports zero failed
addresses and at least one spanning activation, and one pumpit1 run against today's 2,865
and 2,735 frame baseline.
