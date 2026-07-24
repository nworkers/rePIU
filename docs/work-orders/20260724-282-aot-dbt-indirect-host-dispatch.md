# AOT-DBT indirect call/jump miss host dispatch 작업 지시 / AOT-DBT indirect call/jump miss host dispatch work order

## 한국어

### 목표

Task 280 로드맵 4단계를 A안으로 구현합니다. `aot-dbt`에서 prefix 없는 legacy-32
`FF /2`, `FF /4` inline-cache miss를 host-stack thunk로 처리하고, 저장된 guest
`CONTEXT`로 기존 `HandleAotIndirectTransfer`를 재사용합니다.

### 작업 범위

1. Task 281 attempt 회계를 보정합니다. `ThreadContext`는 C++ 진입 수를 `entry`로
   유지하고, 보고되는 `attempt`는 `success + fallback`으로 도출합니다.
2. RET fallback 원인 enum을 두 경로 공용 `AotDbtDispatchFallbackReason`으로 일반화하고
   slot 3을 `kUnreadableSource`로 바꿉니다. 카운터 배열은 경로별로 분리합니다.
3. 공용 image에 `AotDbtIndirectDispatchSite`와
   `enable_dbt_indirect_miss_dispatch` 옵션을 추가합니다.
4. `EmitIndirectInlineCacheSlot`이 옵션에 따라 3슬롯 miss tail, fallback continuation,
   call/jump별 success continuation을 방출하게 합니다. 옵션이 꺼지면 기존
   `popfd; INT3` 바이트를 유지합니다.
5. Win32 placement와 dynamic append가 miss 절대 주소와 thunk `rel32`를 해결하고 append
   offset을 재배치하게 합니다.
6. `aot_dbt_indirect_dispatch.{h,cpp}`에 naked thunk와 resolver adapter를 추가합니다.
   site 존재와 guest 명령 종류 일치를 검증한 뒤에만 handler를 호출합니다.
7. indirect 시도/성공/fallback과 원인 벡터를 `ThreadContext`, 공개 실행 결과, 종료
   로그에 추가합니다.
8. `aot-dbt` backend에서만 새 옵션을 켭니다.
9. synthetic probe를 추가하고 Win32 x86 Debug 빌드와 격리 EEPROM 실구동을 수행합니다.
10. 설계, analysis, architecture, 작업 로그를 갱신합니다.

### 제외 범위

- HLE boundary, 미번역 fallthrough, 임의 cache miss의 exception-free 전환
- quarantine, HLE, non-guest, translation 실패의 직접 처리 확대
- `legacy`, `aot`, `aot-dynamic` image layout과 실행 의미 변경
- inline-cache 슬롯 수나 교체 정책 변경

### 완료 조건

- `attempt = success + fallback = reason sum`이 RET과 indirect 양쪽에서 성립합니다.
- probe가 call/jump layout, memory operand 형태, 비활성 시 기존 layout, placement와
  dynamic append offset, 원인 slot을 검증합니다.
- Win32 x86 Debug 빌드와 기존 probe가 모두 통과합니다.
- 격리 EEPROM 실구동에서 fatal, exception, legacy fallback이 0이고 EEPROM hash가
  불변이며 `indir` boundary가 감소합니다.
- 대조 `aot-dynamic`에서 새 카운터가 0입니다.

## English

### Goal

Implement Task 280 Stage 4 with option A: route prefix-free legacy-32 `FF /2` and
`FF /4` inline-cache misses through the host-stack thunk under `aot-dbt`, reusing
`HandleAotIndirectTransfer` with the saved guest `CONTEXT`.

### Scope

1. Correct the Task 281 attempt accounting: keep the raw C++ entry count and derive
   the reported attempt as `success + fallback`.
2. Generalize the fallback-cause enum to `AotDbtDispatchFallbackReason` with slot 3 as
   `kUnreadableSource`, keeping separate per-path counter arrays.
3. Add `AotDbtIndirectDispatchSite` and the `enable_dbt_indirect_miss_dispatch` option
   to the platform-neutral image.
4. Emit the three-slot miss tail, fallback continuation, and per-kind success
   continuation when the option is on; keep the existing bytes when it is off.
5. Resolve and relocate the miss immediate and thunk `rel32` in Win32 placement and
   dynamic append.
6. Add the naked thunk and resolver adapter in `aot_dbt_indirect_dispatch.{h,cpp}`,
   validating the site and the guest instruction kind before calling the handler.
7. Add indirect attempt/success/fallback counters and the reason vector to the thread
   context, public execution result, and final log.
8. Enable the option only for the `aot-dbt` backend.
9. Add synthetic probes, run the Win32 x86 Debug build and isolated-EEPROM live runs.
10. Update the design, analysis, architecture, and work-log documents.

### Out of scope

- Exception-free dispatch for HLE boundaries, untranslated fallthrough, or arbitrary
  cache misses
- Widening direct handling of quarantine, HLE, non-guest, or translation failures
- Changing `legacy`, `aot`, or `aot-dynamic` layouts or execution semantics
- Changing inline-cache slot count or replacement policy

### Completion criteria

- `attempt = success + fallback = reason sum` holds for both paths.
- Probes verify layouts, operand forms, the disabled layout, placement and dynamic
  append offsets, and the cause slots.
- The Win32 x86 Debug build and all existing probes pass.
- A live isolated-EEPROM run has zero fatal/exception/legacy fallback, unchanged EEPROM
  hashes, and a reduced `indir` boundary count.
- The `aot-dynamic` control keeps the new counters at zero.
