# 20260723-276 작업 로그: AOT-DBT 기반과 HLE 후 즉시 복귀

## 한국어

### 구현

- 플랫폼 공용 `runtime::ExecutionBackend`과 parser/name/capability helper를
  추가했습니다. `legacy`, `aot`, `aot-dynamic`, `aot-dbt`를 한 정책으로 전달합니다.
- Win32 host의 중복 문자열 판정을 제거하고 잘못된 backend 값을 실행 전에 거부합니다.
- AOT 실행 API와 `ThreadContext`가 dynamic bool 대신 backend 정책을 보존합니다.
- `aot-dbt`는 HLE handler가 EIP를 전진시킨 뒤 안전한 기존 cache entry가 있으면 TF를
  지우고 즉시 복귀합니다.
- DBT 전용 preflight와 복귀 정책은 `aot_dbt_dispatch.*`로 분리하고, 기존
  `aot_runtime_dispatch`에는 공용 target resolution과 AOT 경계 처리를 유지했습니다.
- 즉시 복귀는 segment-register write 뒤에는 금지됩니다. 또한 Zydis로 첫 control
  transfer까지 최대 64개 명령을 검사해 등록된 HLE boundary, decode/read 실패와 상한
  도달을 fail-closed 거부합니다.
- 시도/성공 계측과 실제 backend 이름 출력을 추가했습니다.

### 안전성 보완 과정

최초 구현은 HLE 직후 cache miss도 동적 번역했고, 두 번째 구현은 기존 cache hit만
허용했습니다. 두 경로 모두 약 4초에 guest `0x030FC777`의 `ES:[0]` 접근이 생성
cache의 직접 `[0]` 접근으로 바뀌어 `0xC0000005`가 발생했습니다. 직선 HLE preflight만
추가한 세 번째 시도도 같은 지점에서 재현됐습니다.

마지막 single-step은 직전 `mov es, ax`였습니다. 따라서 segment selector 변경은
다음 HLE 경계까지 기존 TF bridge를 유지해야 한다는 정책을 추가했습니다. 최종
30초 supervisor와 graceful loader 실행은 예외 없이 완료됐습니다. 이 실패 과정과
근거는 `docs/analysis/aot-dbt-post-hle-reentry.md`에 누적했습니다.

### 검증

- Win32 x86 Debug 전체 빌드 성공
- AOT synthetic probe 전체 통과
  - `execution_backend_policy=true`
  - inline-cache layout/chain/round-robin/retirement 전체 통과
  - native linear span probe 전체 통과
  - SMC coherence probe 전체 통과
- 최종 30초 supervisor 실행: fatal 0, legacy fallback 0, window open 1
- 최종 30초 graceful `aot-dbt`:
  - single-step `127,940`
  - AOT boundary/re-entry `12,711/12,722`
  - DBT HLE 즉시 복귀 시도/성공 `5,670/2,335`
  - fatal 0, legacy fallback 0, progress `10,685`, window open 1
- 비교 `aot-dynamic` graceful 실행:
  - single-step `189,656`
  - AOT boundary/re-entry `15,267/15,278`
  - fatal 0, progress `10,709`, window open 1
- 모든 실행용 EEPROM 사본은 원본 SHA-256
  `A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`와 일치

두 실행의 초기화/hot phase 진입 시점이 달라 원시 single-step 차이 `-32.5%`는 성능
수치로 사용하지 않습니다. 성공 2,335회가 각각 TF 명령 하나를 제거하므로 같은 실행
내 국소 proxy 절감은 약 **1.8%**입니다. progress는 사실상 같아 사용자 체감
wall-clock 개선은 아직 확인되지 않았습니다.

### 결과

`aot-dynamic`은 기존 정책 그대로 유지되고, `aot-dbt`는 이후 exception-free
return/indirect/cache-miss dispatch를 독립적으로 확장할 기반을 확보했습니다. 첫
increment는 안전하게 동작하지만 개선폭은 작으므로, 다음 단계는 HLE 직후 miss를
무리하게 넓히기보다 return/indirect sentinel의 host dispatcher 전환을 우선 검토합니다.

## English

### Implementation and safety

Added the platform-neutral `runtime::ExecutionBackend` policy and propagated
`legacy`, `aot`, `aot-dynamic`, and `aot-dbt` through the Win32 host, AOT API, and
thread context. Only `aot-dbt` may immediately re-enter an existing cache entry
after an HLE handler advances EIP. Zydis preflights up to 64 instructions through
the first control transfer, and segment-register writes always retain the TF
bridge. Cache misses, registered HLE boundaries, quarantine, decode/read failure,
and cap exhaustion fail closed.

DBT-specific preflight and re-entry policy live in `aot_dbt_dispatch.*`; the
existing runtime-dispatch module retains shared target resolution and AOT
boundary handling.

Early versions reproduced an access violation around four seconds at guest
`0x030FC777`: selector-zero `ES:[0]` low-memory semantics had become a generated
direct `[0]` access after `mov es, ax`. Restricting to cache hits and then adding
straight-line preflight alone did not fix it. Treating the segment-selector write
itself as a mandatory bridge barrier produced stable 30-second supervisor and
graceful-loader runs.

### Verification and result

The full Win32 x86 Debug build passed, as did backend-policy, inline-cache,
native-linear-span, and all SMC coherence probes. The final graceful `aot-dbt`
run recorded 127,940 single steps, 12,711/12,722 boundaries/re-entries,
5,670/2,335 immediate attempts/successes, progress 10,685, one window open, zero
fatal state, and zero legacy fallback. The comparison `aot-dynamic` run recorded
189,656 single steps, 15,267/15,278 boundaries/re-entries, progress 10,709, and
zero fatal state.

Initialization and hot-phase entry differed, so the raw 32.5% single-step
difference is not claimed as a performance gain. Each of the 2,335 successful
re-entries removes exactly one TF instruction, a conservative within-run proxy
reduction of about 1.8%. Progress was effectively unchanged; user-visible
wall-clock improvement remains unconfirmed. Every isolated EEPROM copy retained
the original SHA-256.
