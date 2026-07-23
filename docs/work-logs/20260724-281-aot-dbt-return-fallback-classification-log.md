# 20260724-281 작업 로그: AOT-DBT RET fallback 원인 분류

## 한국어

### 구현

- `include/repiu/platform/win32/execution_trampoline.h`에 고정 10칸
  `AotDbtReturnFallbackReason` enum과 `kAotDbtReturnFallbackReasonCount`를 두고,
  공개 실행 결과 `Win32MinimalExecutionAttempt`에 같은 순서의 count 배열을 추가했습니다.
- `ThreadContext`에 reason별 atomic counter 배열을 추가했습니다.
- `RecordAotDbtReturnFallback` 하나만 total과 reason 하나를 함께 증가시키며, 범위를
  벗어난 값은 `kUnknown`으로 접습니다.
- `ResolveAotDbtReturnMissFrame`이 attempt를 site 검증보다 먼저 증가시켜 invalid site도
  회계에 포함하도록 옮겼습니다.
- `HandleAotReturnTransfer`에 기본값 `nullptr`인 선택적 출력 인자를 추가해 기존 호출자의
  동작을 바꾸지 않고 실패 원인을 전달합니다.
- 종료 telemetry 복사와 host 로그에 고정 순서 reason vector와 reason 합계를 출력합니다.
- `src/tools/aot_probe/dbt_return_fallback_probe.*`가 모든 reason slot을 한 번씩 기록해
  slot 값과 `total = reason sum` 불변식을 검증합니다.

### 검증

- VS2022 Win32 x86 Debug 전체 빌드 성공
- `repiu_aot_probe` 전체 통과 (backend policy, legacy/DBT return layout, indirect
  inline-cache chain/round-robin/retirement, native linear span, SMC coherence, 신규
  DBT return fallback 회계)
- 격리 EEPROM `aot-dbt` 15초:
  `build/task281-headless-20260724-014514/attempt-01-stderr.log`
  - return attempt/success/fallback `6,298/884/5,413`
  - reason vector `0/0/0/0/0/0/5413/0/0/0`, reason total `5,413`
  - boundary ret/indir/direct/cond/other `5,413/33/0/0/15,218`
  - AOT entry/boundary/reentry/fallback `1/20,664/21,584/0`, progress `15,630`
  - exception caught false, fatal 0, legacy fallback 0
- 격리 EEPROM `aot-dbt` 120초 hot phase:
  `build/task281-headless-hot-20260724-014853/stderr.log`
  - return attempt/success/fallback `11,828/3,794/8,034`
  - reason vector `0/0/0/0/0/0/8034/0/0/0`, reason total `8,034`
  - boundary ret/indir/direct/cond/other `8,034/34,851/0/0/70,957`
  - AOT entry/boundary/reentry/fallback `1/113,842/117,742/0`, progress `128,767`
  - exception caught false, fatal 0, legacy fallback 0
- 두 실행의 EEPROM SHA-256은 원본과 동일
  (`A1FC1D12...46652570`)

### 결과

관측된 모든 RET fallback이 `quarantined target` 한 칸에 100% 집중했습니다. site, state,
opcode, stack, zero, HLE, non-guest, translation은 두 실행 모두 0이므로 Task 277
host-stack ABI와 return target 해석은 관측 범위에서 결함이 없습니다. quarantine은 SMC
페이지에 대한 정확성 장치이므로 RET 경로에서 완화하지 않습니다. hot phase의 `indir`
boundary가 34,851회로 RET fallback의 약 4.3배이므로 다음 대상은 Task 280 로드맵 4단계
indirect call/jump host dispatcher이며, operand capture는 저장된 guest `CONTEXT`로 기존
`HandleAotIndirectTransfer`를 재사용하는 A안으로 진행합니다.

### 남은 작업

15초 표본에서 `attempt = success + fallback + 1`이 관측됐습니다. attempt를 C++ resolver
진입 직후에 증가시키므로 graceful timeout이 resolver 실행 중에 걸리면 그 1건이 어느
결과에도 도달하지 못합니다. 120초 표본은 정확히 일치했습니다. 분류 결과와 실행 의미에는
영향이 없지만, 보고되는 attempt를 `success + fallback`으로 도출하도록 보정하는 작업이
남아 있으며 Stage 4 착수 전에 처리합니다. 이번 브랜치에서는 완료하지 못했습니다.

## English

### Implementation

A fixed ten-slot `AotDbtReturnFallbackReason` enum and matching count array were added to
the public Win32 execution result, with per-reason atomic counters in `ThreadContext`. A
single helper, `RecordAotDbtReturnFallback`, increments both the fallback total and exactly
one reason, folding out-of-range values into `kUnknown`. The DBT return adapter now counts
the attempt on C++ resolver entry, before site validation, so invalid sites are accounted.
`HandleAotReturnTransfer` reports the cause through an optional defaulted output parameter,
leaving existing callers unchanged. Final telemetry and the host log print the fixed-order
reason vector and its sum, and a new synthetic probe records every slot once to verify both
accounting invariants.

### Verification and result

The VS2022 Win32 x86 Debug build and every AOT probe, including the new fallback-accounting
probe, passed. Two isolated-EEPROM `aot-dbt` runs recorded 6,298/884/5,413 and
11,828/3,794/8,034 attempts/successes/fallbacks with reason vectors
`0/0/0/0/0/0/5413/0/0/0` and `0/0/0/0/0/0/8034/0/0/0` — every observed fallback is a
quarantined return target. Both runs had zero exceptions, fatal state, and legacy fallback,
and unchanged EEPROM hashes.

Because quarantine protects self-modifying guest pages, the RET target policy stays as is;
no safely recoverable RET fallback remains. Hot-phase indirect boundaries (34,851) exceed
RET fallbacks (8,034) by about 4.3x, so Stage 4 of the Task 280 roadmap proceeds with
option A: reuse `HandleAotIndirectTransfer` from the thunk's saved guest `CONTEXT`.

### Remaining work

The 15-second sample showed attempt = success + fallback + 1 because the graceful timeout
landed inside the resolver after the attempt increment; the 120-second sample matched
exactly. The reported attempt should be derived as `success + fallback` so the invariant
holds for timeout samples too. That correction was not completed on this branch and is a
prerequisite for Stage 4.
