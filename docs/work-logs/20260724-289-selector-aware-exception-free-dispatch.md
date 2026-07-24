# 20260724-289 작업 로그: selector 인지 exception-free dispatch / Work log: selector-aware exception-free dispatch

설계: [20260724-289-selector-aware-exception-free-dispatch.md](../design/20260724-289-selector-aware-exception-free-dispatch.md)

작업 지시: [20260724-289-selector-aware-exception-free-dispatch.md](../work-orders/20260724-289-selector-aware-exception-free-dispatch.md)

## 한국어

### Stage 1 — live selector descriptor 모델과 guard 보강

구현 착수 전 코드 대조에서 Task 264 Phase 3a가 이미 shadow selector 주소/값과
descriptor base를 segment-override guard에 공급한다는 사실을 확인했습니다. 새 ABI를
추가하지 않고 기존 `Win32AotSegmentResolution`을 selector/base/limit/flags 및 명시적
native/HLE/unresolved 정책으로 확장했습니다.

- selector 0: DOS low-memory HLE boundary
- descriptor 전체 linear range가 64 KiB DOS low-memory 안: HLE boundary
- descriptor 없음, overflow 또는 shadow 주소 없음: unresolved boundary
- 정상 nonzero flat descriptor와 기존 검증된 GS non-flat base-add: guarded native

재해석 self-gate는 selector만 비교하던 방식에서 전체 descriptor fingerprint 비교로
바뀌었습니다. segment load뿐 아니라 DPMI base/limit/flags/allocate/free, DOS interrupt
vector의 ES, DOS4GW identification의 GS, LINEXE bridge의 ES 변경도 재해석을 요청합니다.
동일 fingerprint이면 기존처럼 RW/RX 전환을 생략합니다.

합성 probe는 flat/native, GS non-flat/native, selector 0/HLE, low-memory/HLE,
unresolved, guard mismatch의 `INT3` fallback, native base patch와 HLE 재패치를 검증합니다.
live/final telemetry에는 native/HLE/unresolved site와 실제 HLE-exit/mismatch를 추가했습니다.

### 검증

- Win32 x86 Debug 전체 증분 빌드: 성공
- `repiu_aot_probe`: `selector_guard_all=true`, `linear_span_all=true`,
  `coherence_all=true`
- 60초 기본 `aot-dbt` supervisor smoke: progress 48,817, single-step 734,006
- selector guard: native/HLE/unresolved site `193288/120668/0`, HLE exit/mismatch
  `7554/0`
- fatal 0, legacy fallback 0, EEPROM SHA-256 fixture 일치
- 증거: `build/task289-stage1-smoke/supervisor.log`

Stage 1은 성능 승격 기능이 아니라 post-HLE cache-miss 번역을 안전하게 시험하기 위한
정확성 기반입니다. 기존 segment-register store는 계속 HLE가 shadow 값을 반환하므로
Task 264 Phase 2 revert 의미를 바꾸지 않습니다. 다음 단계는 생성 CFG의 HLE/guard
coverage를 구조적으로 검증하고, 통과한 cache miss만 opt-in으로 즉시 번역하는 것입니다.

## English

### Stage 1 — live selector descriptor model and guard hardening

Code reconnaissance confirmed that Task 264 Phase 3a already feeds shadow-selector
addresses/values and descriptor bases into segment-override guards. Stage 1 reuses that ABI
and extends `Win32AotSegmentResolution` with selector/base/limit/flags plus explicit
native/HLE/unresolved policy. Selector zero and descriptors wholly inside 64 KiB DOS low
memory remain HLE boundaries; missing, overflowing, or addressless resolutions remain
unresolved boundaries; valid nonzero flat descriptors and the proven GS non-flat base-add
path remain guarded-native.

The re-resolution self-gate now compares the complete descriptor fingerprint. Segment
loads, DPMI descriptor lifecycle changes, DOS ES/GS changes, and LINEXE ES restoration all
request re-resolution, while unchanged fingerprints still avoid RW/RX cache transitions.
Synthetic probes cover policy classification, mismatch-to-`INT3`, native base patching, and
HLE re-patching. Live/final telemetry exposes native/HLE/unresolved sites and actual
HLE-exit/mismatch boundaries.

The Win32 x86 Debug build and all probes passed (`selector_guard_all`, `linear_span_all`, and
`coherence_all`). A 60-second default `aot-dbt` supervisor smoke reached progress 48,817 and
single-step 734,006, with guard accounting `193288/120668/0` native/HLE/unresolved sites and
`7554/0` HLE exits/mismatches. Fatal and legacy fallback stayed zero and the EEPROM hash
matched. Stage 1 is a correctness prerequisite, not a performance promotion. Segment-register
stores retain their shadow-value HLE semantics. Stage 2 will structurally preflight complete
generated CFG coverage and opt in only safe post-HLE cache-miss translation.

## 한국어 — Stage 2: whole-CFG 검증과 post-HLE miss 번역

`ValidateAotCodeCacheHleCoverage`는 생성 plan의 모든 HLE/segment-override record를 cache
image와 대조합니다. 실제 `INT3` 또는 `pushfd; cmp shadow; je; popfd; int3` 구조가 아니면
동적 image를 publish하지 않습니다. 합성 probe는 정상 CFG 통과와 fallback `INT3`가
빠진 CFG 거부를 확인합니다. `REPIU_AOT_DBT_POST_HLE_TRANSLATE=1`에서만 segment-write와
quarantine 장벽 뒤의 cache miss를 번역하며, 기존 cache hit는 기존 직선 preflight를
그대로 사용합니다.

10초 A/B 두 번과 60초 A/B에서 번역 attempt/success는 모두 0/0이었습니다. 최종 60초
OFF/ON은 progress 49,446/49,343, single-step 734,954/733,160, guest proxy
1,443,256/1,440,067이었습니다. 양쪽 모두 cancel/fatal/legacy fallback 0, EEPROM 일치입니다.
현재 안전 후보는 cache hit로 이미 처리되어 miss 모집단이 없으므로 기본 OFF로 보류합니다.

## English — Stage 2: whole-CFG validation and post-HLE miss translation

`ValidateAotCodeCacheHleCoverage` rejects a generated image unless every HLE or
segment-override record is an actual `INT3` or a complete mismatch-to-`INT3` selector guard.
Synthetic valid and missing-guard probes pass. Only
`REPIU_AOT_DBT_POST_HLE_TRANSLATE=1` translates a cache miss behind the existing
segment-write and quarantine barriers; cache hits retain the existing straight-line
preflight. Two 10-second pairs and one 60-second pair all recorded `0/0` translation
attempts/successes. The final OFF/ON pair measured progress 49,446/49,343, single-step
734,954/733,160, and proxy 1,443,256/1,440,067, with zero cancellation/fatal/legacy fallback
and matching EEPROM. The feature remains default off because there is no current miss
population.

## 한국어 — Stage 3: cache breakpoint provenance census와 보류 결정

초기 설계의 `other = HLE + 미번역 fallthrough + arbitrary miss` 가정을 코드에서 다시
검증했습니다. completed cache image는 direct/conditional/block-fallthrough target이 image
안에서 해소되지 않으면 build가 실패하며, guest cache miss 자체는 cache `INT3` 주소를
갖지 않습니다. 따라서 범용 host dispatch 전에 cache breakpoint의 구조적 출처를 분리하는
Stage 3a로 작업을 보정했습니다.

`AotCacheBreakpointProvenance`와 placement의 O(1) offset index를 추가했습니다. 정적 배치와
동적 append가 planner HLE, selector guard, inline-cache fallback, jump-table fallback,
other planner fixup을 등록하고, 실행 시 retired/inactive entry와 명시적 probe sentinel을
우선 판정합니다. live telemetry v22와 종료 summary는 8개 원인을 노출합니다. 합성
`boundary_provenance_probe`와 전체 기존 probe가 통과했습니다.

60초 기본 `aot-dbt` smoke의 최종 supervisor 값은 다음과 같습니다.

- boundary/re-entry `71,486/72,468`, guest reason ret/indir/other
  `6,463/28,449/36,574`
- provenance HLE/segment/inline/jump-table/retired/probe/fixup/unknown
  `22,248/7,064/34,912/0/7,298/0/0/0`
- inline `34,912 = 6,463 + 28,449`, retired `7,298 = retired trap 7,298`
- cache breakpoint 합계 71,522와 일반 boundary 차이 36은 retired entry가 새 generation으로
  즉시 복귀한 조기 처리 횟수
- fatal 0, legacy fallback 0, EEPROM SHA-256 원본 일치
- 증거: `build/task289-stage3a-smoke-indexed/supervisor.log`, `loader-live.log`

`unknown=0`이므로 arbitrary miss/미번역 fallthrough용 Stage 3b host tail은 추가하지 않고
보류했습니다. 현재 큰 비용은 이미 전용 dispatcher 대상인 inline miss와 정확성이 필요한
planner HLE/segment 경계이며, 다음 개선은 이 명령의 충실한 번역 범위를 넓히는 쪽입니다.

## English — Stage 3: cache-breakpoint provenance census and hold decision

Code inspection corrected the original assumption that `other` contains completed-image
untranslated fallthroughs and arbitrary misses. Image construction fails if direct,
conditional, or block-fallthrough edges do not resolve internally, and a guest cache miss has
no cache `INT3` address by itself. Stage 3 was therefore gated by a structural provenance
census.

`AotCacheBreakpointProvenance` and an O(1) placement offset index now classify planner HLE,
selector guard, inline-cache fallback, jump-table fallback, retired/inactive entry, explicit
probe sentinel, other planner fixup, and unknown. Static placement and dynamic append both
index immutable sites; runtime retirement/probe state takes precedence. Live telemetry v22,
the final summary, the new synthetic probe, and all existing probes passed.

The 60-second default `aot-dbt` smoke recorded boundary/re-entry `71,486/72,468`, guest reasons
return/indirect/other `6,463/28,449/36,574`, and provenance
`22,248/7,064/34,912/0/7,298/0/0/0`. Inline exactly equals return+indirect and retired exactly
equals the retired-entry trap counter. The 36 extra cache breakpoints are retired entries that
re-entered a new generation before ordinary boundary accounting. Fatal and legacy fallback
were zero and the isolated EEPROM hash matched. Because unknown is zero, Stage 3b adds no
generic host tail and remains held; faithful translation of planner-HLE/segment instructions is
the next relevant lever.
