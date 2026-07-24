# 20260724-288 작업 로그: 네이티브 직선 span 확장 / Work log: native linear-span extension

설계: [20260724-288-native-linear-span-extension.md](../design/20260724-288-native-linear-span-extension.md)

작업 지시: [20260724-288-native-linear-span-extension.md](../work-orders/20260724-288-native-linear-span-extension.md)

## 한국어

### Stage 1 — page-generation 스캔 캐시

`NativeFastPathState`에 entry EIP별 `NativeLinearSpan` 캐시와 hit/miss 카운터를
추가했습니다. 캐시 키는 guest page와 coherence 계층의 active generation을 함께
보존합니다. 다음 조건을 모두 만족할 때만 결과를 저장하거나 조회합니다.

- entry page가 AOT write-watch로 보호됨
- page가 retired/quarantined 상태가 아니고 active generation을 가짐
- span boundary가 entry와 같은 4 KiB page에 있음

조건을 만족하지 않거나 generation이 바뀌면 기존 Zydis 스캔을 다시 수행합니다. stale
entry는 조회 시 즉시 삭제합니다. 환경 변수 `REPIU_NATIVE_LINEAR_SPAN_CACHE=1|on|true`만
캐시를 켜며, 미지정·알 수 없는 값은 OFF입니다. 따라서 기존 `aot-dbt` span 기본 정책은
바뀌지 않습니다.

probe는 generation 1 최초 miss, 저장 뒤 hit, generation 2 교체 뒤 miss와 stale 삭제를
검증합니다. coherence probe도 initial generation 조회, retirement 후 조회 거부, 새
generation 발행, 재-retirement 후 조회 거부를 확인합니다. 최종 로그와 live telemetry에
span lifecycle 및 cache hit/miss를 노출했습니다.

### 측정 장치 보강

두 native-span A/B 스크립트에 `-CompareCache`를 추가했습니다. 이 모드는 span을 항상
켜고 cache만 `0/1`로 교차합니다. 낮은 Win32 주소 예약 실패 또는 relocated base별 AOT
배치 실패가 간헐적으로 발생했으므로, 실행 결과가 생성되지 않은 시작 실패만 bounded
retry하고 유효 실행만 CSV에 기록하도록 `-StartupRetries`도 추가했습니다. 각 유효 실행은
격리 EEPROM을 다시 복사합니다.

### 검증

- Win32 x86 Debug 전체 빌드: 성공
- `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE`: 성공
- 신규 `linear_span_cache_generation=true`
- 기존 `linear_span_all=true`, `coherence_all=true`
- PowerShell A/B 스크립트 parser 검사: 성공

유효 60초 supervisor A/B:
`build/benchmarks/native-linear-span-cache/aot-dbt/20260724-233956/results.csv`

| 항목 | OFF | ON | 판단 |
|---|---:|---:|---|
| progress | 51,545 | 51,223 | -0.62% |
| window open | 5,250 ms | 5,328 ms | ON 78 ms 지연 |
| single-step | 855,100 | 762,633 | 실행량 감소와 동반 |
| guest instruction proxy | 1,702,674 | 1,498,756 | 처리량 증가 없음 |
| span entry/boundary/cancel | 196,200/196,200/0 | 172,735/172,735/0 | 정확성 유지 |
| cache hit/miss | 0/0 | 0/622,322 | 재사용 없음 |

유효 60초 직접 loader A/B:
`build/benchmarks/native-linear-span/aot-dbt-direct-cache/20260724-234214/results.csv`

| 항목 | OFF | ON | 판단 |
|---|---:|---:|---|
| progress | 51,441 | 51,439 | 사실상 동일 |
| single-step | 886,018 | 843,473 | 실행량 감소와 동반 |
| span entry/boundary/cancel | 204,761/204,761/0 | 193,303/193,303/0 | 정확성 유지 |
| cache hit/miss | 0/0 | 0/701,975 | 재사용 없음 |

두 ON 실행 모두 fatal/legacy fallback 0, EEPROM SHA-256 일치, exception false였으며
direct loader는 정상 graceful timeout으로 종료했습니다.

### Stage 1 판정

**기본 활성화를 보류합니다.** 현재 반복되는 native-span fallback은 active generation
페이지보다 retired/quarantined 페이지에 집중됩니다. 안전 술어상 이 페이지는 캐시할 수
없어 두 독립 실행에서 hit가 0이었습니다. hit가 없는 상태에서는 decode 재사용으로
throughput을 높일 수 없으므로 240초 3쌍 측정은 조기 종료했습니다. 코드는 향후 active
generation fallback 분포가 달라질 때 재검증할 수 있도록 명시적 opt-in으로 유지합니다.

다음 단계는 Stage 2입니다. 먼저 실제 write-watch/read-only coverage를 확인하고, 완전히
보호되는 span 코드 page에서만 memory-write 경계를 통과시키는 fail-closed 후보를
구현합니다.

## English

### Stage 1 — page-generation scan cache

Stage 1 adds a per-entry `NativeLinearSpan` cache and hit/miss counters to
`NativeFastPathState`. A result is stored or reused only when the entry page is protected by
the AOT write watch, has a non-retired and non-quarantined active generation, and the span
boundary remains on the same 4 KiB page. The guest page and generation are part of the key;
a mismatch erases the stale entry and falls back to the existing Zydis scan. Untracked and
unsafe pages always rescan. Only `REPIU_NATIVE_LINEAR_SPAN_CACHE=1|on|true` enables the
candidate, so the existing `aot-dbt` span default is unchanged.

The probe covers an initial generation-1 miss, a hit after insertion, and a generation-2
miss with stale-entry removal. The coherence probe also checks active generation lookup,
retirement rejection, new-generation publication, and second-retirement rejection. Final
and live telemetry now expose span lifecycle and cache hit/miss counters.

Both A/B scripts gained `-CompareCache`, which holds spans on and alternates only cache
state. They also gained bounded `-StartupRetries` because transient low-address reservation
and relocated-base AOT-placement failures occurred before guest execution; only valid runs
enter the CSV, with the isolated EEPROM fixture recopied for every attempt.

The full Win32 x86 Debug build passed. `repiu_aot_probe` passed with
`linear_span_cache_generation=true`, `linear_span_all=true`, and `coherence_all=true`; both
PowerShell scripts passed parser validation.

The valid 60-second supervisor pair measured progress `51,545/51,223`, window-open
`5,250/5,328 ms`, span lifecycle `196,200/196,200/0` versus
`172,735/172,735/0`, and cache `hit/miss=0/622,322`. The valid direct-loader pair measured
progress `51,441/51,439`, span lifecycle `204,761/204,761/0` versus
`193,303/193,303/0`, and cache `0/701,975`. Both enabled runs had zero fatal, legacy
fallback, cancellation, or exception and matched the EEPROM fixture; the direct loader
ended through its normal graceful timeout.

**Decision: hold default promotion.** The current repeating native-span fallback is
concentrated on retired or quarantined pages rather than active-generation pages. The safe
predicate therefore produced zero hits in two independent runs. With no possible decode
reuse, the three-pair 240-second campaign was stopped early. The code remains explicit
opt-in for future distributions. Stage 2 will first measure write-guard coverage, then pass
memory-write boundaries only on fully protected span code pages.

## 한국어 — Stage 2: guarded memory-write 통과

### 구현과 안전 범위

`REPIU_NATIVE_LINEAR_SPAN_WRITES=1`에서만 explicit memory write를 span 안에서 통과하는
후보를 구현했습니다. 다음 조건을 모두 만족해야 합니다.

- entry와 이후 스캔 code page가 모두 AOT write-watch로 보호됨
- write가 span entry 자체가 아님
- memory operand의 base/index register가 같은 span 앞부분에서 변경되지 않음
- entry CONTEXT로 계산한 전체 target range가 guest runtime 안에 있음
- target page가 write-watch 대상이거나 최초 `VirtualQuery`에서 committed+writable임

target page 보호 결과는 `NativeFastPathState`에 캐시해 반복 scan에서 OS query를 하지
않습니다. watched code page를 실제로 수정하면 write 완료 전에 access violation이 발생해
span을 중단하고 기존 coherence 처리로 들어갑니다. 이 정상 중단은 unexpected cancel과
분리해 집계합니다. 미커버 code page, entry write, 변경된 주소 register, 부적합 target은
기존 write 경계로 남습니다.

초기 실험에서 두 가지 문제를 찾아 fail-closed 조건을 강화했습니다. `0x030F3FC8`
`mov [edx], ax`는 접근 불가 target에 대한 기존 fault/HLE 처리가 필요한데 처음에는
unexpected AV를 만들었습니다. entry/modified-register/target preflight로 이를 제거했습니다.
또한 매 scan `VirtualQuery`는 처리량을 약 5분의 1로 낮춰 page별 최초 query cache로
대체했습니다.

probe는 guarded write 통과, unguarded 기존 경계, 다음 미커버 page 경계, entry write
거부, 앞선 address-register 변경 거부, 부적합 target 거부를 각각 검증합니다.

### 최종 검증

- Win32 x86 Debug 전체 빌드: 성공
- `linear_span_guarded_write_crossed=true`
- `linear_span_unguarded_write_stopped=true`
- `linear_span_uncovered_page_stopped=true`
- `linear_span_entry_write_stopped=true`
- `linear_span_modified_address_write_stopped=true`
- `linear_span_rejected_target_write_stopped=true`
- `linear_span_all=true`, `coherence_all=true`

최종 60초 supervisor A/B:
`build/benchmarks/native-linear-span-writes/aot-dbt/20260725-003610/results.csv`

| 항목 | OFF | ON | 변화 |
|---|---:|---:|---:|
| progress | 48,225 | 17,236 | -64.26% |
| single-step | 707,914 | 678,533 | -4.15% |
| guest instruction proxy | 1,400,871 | 1,538,315 | +9.81% |
| 평균 span 길이 | 4.30 | 5.09 | +18.34% |
| write-cross | 0 | 48,633 | 활성 |
| entry/boundary/write-fault/unexpected cancel | 161225/161225/0/0 | 169057/169033/24/0 | 불변식 통과 |

의미 기반 240초 direct pilot:
`build/benchmarks/native-linear-span/aot-dbt-direct-writes/20260725-002726/results.csv`

| 항목 | OFF | ON | 변화 |
|---|---:|---:|---:|
| progress | 190,366 | 90,667 | -52.37% |
| single-step | 2,710,519 | 2,612,791 | -3.61% |
| texture download | 5 | 5 | 동일 |
| draw | 1,606 | 1,278 | -20.42% |
| buffer swap | 233 | 185 | -20.60% |
| write-cross | 0 | 132,288 | 활성 |

두 direct 실행은 exception false, timed out true, fatal/legacy fallback 0, EEPROM hash
일치였습니다. late phase에서 OFF/ON 모두 약 2,100회의 `EXCEPTION_SINGLE_STEP` span
cancel을 기록해 공통 zero-cancel 게이트는 충족하지 못했습니다.

### Stage 2 판정

**기본 활성화를 보류합니다.** 평균 span 길이와 local instruction proxy는 늘었지만 실제
draw/swap 처리량이 약 20% 감소했습니다. 3쌍 240초 측정은 명확한 의미 milestone 회귀로
조기 종료했습니다. 구현은 `REPIU_NATIVE_LINEAR_SPAN_WRITES=1` opt-in으로만 유지합니다.
다음 단계는 memory semantics를 건드리지 않는 forward direct `jmp` 체인입니다.

## English — Stage 2: guarded memory-write crossing

Stage 2 adds explicit opt-in crossing under `REPIU_NATIVE_LINEAR_SPAN_WRITES=1`. Every
traversed code page must be AOT write-watched. Entry writes, writes whose base/index register
changed earlier in the span, and targets outside guest runtime or on read-only/uncommitted
non-watched pages remain at the existing boundary. Target-page protection is queried once
and cached in `NativeFastPathState`. A real write to a watched page faults before completion,
stops the span, and enters the existing coherence path; this expected write-fault stop is
counted separately from unexpected cancellation.

The investigation tightened two fail-closed conditions. The write at `0x030F3FC8` required
the old fault/HLE path and initially produced an unexpected AV; entry/register/target
preflight removes it. Calling `VirtualQuery` on every scan reduced throughput by about 5x,
so the final path uses a first-query page cache. Synthetic probes cover guarded crossing,
unguarded and uncovered-page stops, entry-write rejection, modified-address-register
rejection, and invalid-target rejection. The full Win32 build, `linear_span_all`, and
`coherence_all` pass.

The final 60-second supervisor pair increased mean span length from 4.30 to 5.09 and the
instruction proxy by 9.81%, crossed 48,633 writes, and satisfied
`entry = boundary + write_fault_cancel` with zero unexpected cancellation. However, progress
fell 64.26%. The decisive 240-second direct pilot kept texture at 5 but reduced draw from
1,606 to 1,278 (-20.42%) and swap from 233 to 185 (-20.60%); progress fell 52.37% while
single-step improved only 3.61%. Both runs had no exception, fatal, legacy fallback, or EEPROM
change. Both also showed about 2,100 late `EXCEPTION_SINGLE_STEP` span cancellations.

**Decision: hold default promotion.** The three-pair campaign was stopped early because
semantic throughput clearly regressed. The implementation remains explicit opt-in, and the
next candidate is forward direct-`jmp` chaining, which does not change memory semantics.

## 한국어 — Stage 3: forward direct-jump 체인

### 구현과 안전 범위

`REPIU_NATIVE_LINEAR_SPAN_JUMPS=1`에서만 스캐너가 전방 near direct `jmp rel`을 span에
포함하고 target에서 스캔을 계속하도록 구현했습니다. target은 guest runtime 범위 안이며
현재 AOT placement의 HLE boundary가 아니고 quarantined page에도 속하지 않아야 합니다.
역방향 jump는 loop 방지를 위해 경계로 남기고, indirect/far jump와 거부 target도 기존
single-step 경계를 유지합니다. 기본값은 OFF입니다.

`NativeFastPathState`, live telemetry, 최종 실행 보고와 두 A/B 스크립트에 forward-chain과
backward-stop 계측을 추가했습니다. 합성 probe는 전방 체인, 기능 OFF 시 기존 정지,
역방향 정지, HLE와 동일하게 callback이 거부한 target 정지를 검증합니다.

### 검증과 측정

- Win32 x86 Debug 전체 빌드: 성공
- `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE`: 성공
- `linear_span_forward_jump_chained=true`
- `linear_span_forward_jump_disabled=true`
- `linear_span_backward_jump_stopped=true`
- `linear_span_rejected_jump_stopped=true`
- `linear_span_all=true`, `coherence_all=true`
- 두 PowerShell A/B 스크립트 parser 검사: 성공

10초 smoke:
`build/benchmarks/native-linear-span-jumps/aot-dbt/20260725-005236/results.csv`

| 항목 | OFF | ON |
|---|---:|---:|
| progress | 12,563 | 12,561 |
| single-step | 98,991 | 98,922 |
| forward chain | 0 | 0 |
| backward stop | 0 | 7 |

유효 60초 supervisor A/B:
`build/benchmarks/native-linear-span-jumps/aot-dbt/20260725-005324/results.csv`

| 항목 | OFF | ON | 판단 |
|---|---:|---:|---|
| progress | 50,562 | 51,002 | +0.87%, timing 변동 범위 |
| single-step | 748,334 | 756,241 | +1.06% |
| guest instruction proxy | 1,473,214 | 1,487,331 | +0.96% |
| span entry/boundary/cancel | 169737/169737/0 | 171393/171393/0 | 정확성 유지 |
| forward chain | 0 | 0 | 최적화 기회 없음 |
| backward stop | 0 | 703 | 계측 활성 |

두 60초 실행 모두 fatal/legacy fallback 0이고 EEPROM SHA-256이 fixture와 일치했습니다.

### Stage 3 및 Task 288 판정

**기본 활성화를 보류합니다.** 짧은 smoke와 60초 실행 모두 실제 forward chain이 0이어서
현재 hot fallback에서는 single-step을 줄일 수 없습니다. 성능 효과를 만들 수 없는 240초
3쌍과 direct-loader 측정은 조기 종료했습니다. 같은 이유로 복잡도와 debug-register 압력을
늘리는 conditional-branch Dr1 후보도 진행하지 않습니다. 구현은 향후 fallback 분포가
달라질 때 재측정할 수 있도록 명시적 opt-in으로 유지합니다.

Task 288의 세 후보 중 기본 승격된 것은 없습니다. Stage 1은 안전 cache hit가 0,
Stage 2는 draw/swap 약 20% 회귀, Stage 3은 forward-chain 기회가 0이었습니다. 따라서 현재
기본 native span 정책은 Task 287 상태를 그대로 유지하며, 다음 작업은 더 큰 `other`
경계군을 겨냥한 Task 289 selector-aware exception-free dispatch입니다.

## English — Stage 3: forward direct-jump chaining

Stage 3 adds explicit opt-in chaining under `REPIU_NATIVE_LINEAR_SPAN_JUMPS=1`. The scanner
includes an in-range forward near direct `jmp rel` and resumes at its target only when the
target is neither an AOT HLE boundary nor a quarantined page. Backward, indirect, far, and
rejected targets retain the existing single-step boundary. Forward-chain and backward-stop
counters are exposed through live/final telemetry and both A/B scripts. Synthetic probes
cover an accepted forward chain, disabled behavior, a backward stop, and a rejected target.

The full Win32 x86 Debug build and all AOT/coherence probes pass. A 10-second smoke recorded
zero chains and seven backward stops. The valid 60-second supervisor OFF/ON pair measured
progress `50,562/51,002`, single-step `748,334/756,241`, and instruction proxy
`1,473,214/1,487,331`. ON recorded zero forward chains and 703 backward stops, with exact
`entry/boundary=171393/171393`, zero cancellation/fatal/legacy fallback, and a matching
EEPROM hash.

**Decision: hold default promotion.** Neither run exposed a forward-chain opportunity, so
the candidate cannot reduce single-step in the current hot fallback. The 240-second
three-pair and direct-loader campaigns were stopped early, and the higher-complexity
conditional-branch Dr1 candidate was not pursued. Task 288 is complete with all three
extensions default off: Stage 1 had zero safe cache hits, Stage 2 regressed draw/swap by
about 20%, and Stage 3 had zero forward chains. The Task 287 base native-span policy remains
unchanged; Task 289 selector-aware exception-free dispatch is the next frontier.
