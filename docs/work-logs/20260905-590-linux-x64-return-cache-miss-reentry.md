# 작업 로그 20260905-590 — Linux x64 return cache-miss 재진입

설계: [20260905-590](../design/20260905-590-linux-x64-return-cache-miss-reentry.md)  
작업 지시: [20260905-590](../work-orders/20260905-590-linux-x64-return-cache-miss-reentry.md)

## 결과

`LinuxX64EngineResolver`가 direct `FindAotCacheAddress` 대신 공용
`ResolveAotTransferTarget`을 사용하도록 변경했습니다. 따라서 valid return continuation은
기존 cache entry 또는 dynamic append 결과로 재진입할 수 있고, 실패 시에는 기존 zero/INT3
fail-closed 계약을 유지합니다.

`0x010F4AD1`은 dynamic translation을 실제로 요청했지만 다음 validator 거절로 append되지
않았습니다.

```text
dynamic AOT CFG lacks complete HLE/selector-guard coverage
```

trace는 `translation-failed`와 append result message를 출력하도록 보강했습니다. 이로써
cache miss, policy 거절, dynamic translation 실패를 구분합니다.

## 검증

* WSL Linux x64 `repiu_exe` 및 `repiu` 빌드 성공. 기존 initialized `extern`
  `g_repiu_active_thread_context` GCC warning 하나만 관측했습니다.
* watched `pumpit2a`에서 `0x010F4A96` segment PUSH HLE을 통과하고
  `source=0x010F4AD1 result=translation-failed` 및 위 CFG coverage message를 확인했습니다.
* return thunk은 새 cache entry나 raw guest address로 jump하지 않고 기존 INT3 fail-closed로
  멈췄습니다.

## 후속 작업

`0x010F4AD1` dynamic CFG가 누락한 HLE/selector guard boundary를 plan builder와 validator에서
분석하고, 안전하게 coverage를 완성하는 translation 범위를 구현합니다.

---

# Work log 20260905-590 — Linux x64 return cache-miss reentry

Design: [20260905-590](../design/20260905-590-linux-x64-return-cache-miss-reentry.md)  
Work order: [20260905-590](../work-orders/20260905-590-linux-x64-return-cache-miss-reentry.md)

## Result

`LinuxX64EngineResolver` now uses shared `ResolveAotTransferTarget` rather than
a direct cache-map lookup. A valid return continuation can therefore use an
existing entry or a dynamic append; failure retains the zero-to-INT3 fail-closed
contract.

Dynamic translation was requested for `0x010F4AD1`, but its append was rejected:

```text
dynamic AOT CFG lacks complete HLE/selector-guard coverage
```

The trace now distinguishes cache miss, policy refusal, and dynamic-translation
failure, including the append-result message.

## Verification

* WSL Linux x64 `repiu_exe` and `repiu` built successfully, with only the
  existing initialized-`extern g_repiu_active_thread_context` GCC warning.
* Watched `pumpit2a` passes the segment-PUSH HLE and reports
  `source=0x010F4AD1 result=translation-failed` with the coverage message.
* The return thunk jumped neither to raw guest code nor an invented cache entry;
  it retained its fail-closed INT3.

## Follow-up

Analyze the HLE/selector-guard boundary omitted from the `0x010F4AD1` dynamic
CFG and implement a safe range that completes coverage.
