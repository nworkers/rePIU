# Task 727 작업 로그 — Linux x64 LFB native-store source census

설계: [20260920-727](../design/20260920-727-linux-x64-lfb-store-site-census.md)
작업 지시: [20260920-727](../work-orders/20260920-727-linux-x64-lfb-store-site-census.md)

## 결과

`REPIU_LINUX_X64_LFB_STORE_SOURCE_CENSUS=1|on|true`는 aggregate 설정 없이 observer와
write-LFB range gate까지 독립적으로 활성화합니다. guest EIP 표본은 `ThreadContext` 밖의 고정
64-entry 상태에 보관합니다. positive LFB overlap 4,096건마다 같은 EIP는 누적하고, 새 EIP가
가득 찬 표에 도달하면 overflow count/byte aggregate로 기록합니다. 종료 snapshot은 count 내림차순,
동률 EIP 오름차순으로 상위 8개를 보고합니다. hot path에서 동적 할당이나 정렬은 수행하지 않습니다.

## 검증

- Linux x64 Debug `repiu`, `repiu_core_probe` 빌드 성공; core probe 25/25 통과.
- Win32 x86 Debug 전체 빌드 성공. `repiu_aot_probe --glide-lfb-native-store-census`는 policy,
  aggregation, ranking tie-break, malformed range, source overflow를 모두 통과했습니다.
- Win32 x86 core probe는 기존 `mode16_push_writes=false` 그룹으로 28개 중 1개가 실패했습니다.
- source-only Linux x64 `pumpit2a` bounded 실행은 Glide 초기화와 write LFB lock 진입을 보였지만,
  외부 제한 종료가 final report를 남기지 않아 top-EIP 결과는 기록하지 않았습니다.

---

# English

## Task 727 work log — Linux x64 LFB native-store source census

Design: [20260920-727](../design/20260920-727-linux-x64-lfb-store-site-census.md)
Work order: [20260920-727](../work-orders/20260920-727-linux-x64-lfb-store-site-census.md)

## Result

`REPIU_LINUX_X64_LFB_STORE_SOURCE_CENSUS=1|on|true` independently enables the observer and
write-LFB range gate without aggregate census. Guest-EIP samples live in fixed 64-entry state
outside `ThreadContext`. Every 4,096th positive LFB overlap accumulates its EIP; a new EIP after
the table fills contributes to overflow count/byte aggregates. Shutdown reports the top eight by
descending count and then ascending EIP. The hot path does no dynamic allocation or sorting.

## Verification

- Linux x64 Debug `repiu` and `repiu_core_probe` built successfully; core probe passed 25/25.
- The full Win32 x86 Debug build succeeded. `repiu_aot_probe --glide-lfb-native-store-census`
  passed policy, aggregation, ranking tie-break, malformed-range, and source-overflow checks.
- The Win32 x86 core probe retained one existing failure of 28: `mode16_push_writes=false`.
- A source-only bounded Linux x64 `pumpit2a` run reached Glide initialization and write-LFB
  lock entry, but external deadline termination emitted no final report, so no top-EIP result is
  recorded.
