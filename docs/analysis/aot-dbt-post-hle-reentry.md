# AOT-DBT HLE 후 즉시 복귀 분석 / AOT-DBT immediate post-HLE re-entry

## 한국어

### 확인됨

Task 276의 최초 구현은 AOT HLE handler가 EIP를 전진시킨 직후 cache miss에서도
`RequestAotDynamicTranslation`을 호출했습니다. `pumpit1` 30초 비교에서
`aot-dynamic`은 정상 진행했지만 최초 `aot-dbt`는 즉시 복귀 `42/42`회 뒤 약 4초에
`0xC0000005`로 종료했습니다.

예외 cache 주소 `0x0D79F7D9`는 guest `0x030FC777`로 역매핑됐습니다. 원본 명령은
`26 80 38 00`(`cmp byte ptr es:[eax], 0`)이고 EAX는 0이었습니다. 직전 guest 코드는
`8E C0`(`mov es, ax`)였으며, 생성 cache 명령은 selector base 0을 단순 선형 주소로
fold한 `[eax+0]` 접근이었습니다. 이 실행점의 selector 0은 DOS low-memory HLE 의미가
필요하므로 주소 0 직접 접근은 올바르지 않습니다.

기존 경로는 HLE 뒤 일반 명령 하나를 TF로 실행한 다음 정확히 segment-override 주소에서
re-entry를 시도합니다. 그 주소는 HLE boundary로 거부되어 기존 low-memory handler가
처리합니다. 반면 HLE 직후 앞 주소에서 새 arbitrary-entry CFG를 생성하면 뒤쪽
segment-override가 같은 생성 block에 포함되어 정확한 주소의 HLE 판정을 우회할 수
있습니다.

### 결론

기존 cache entry hit만 허용한 두 번째 시도도 같은 지점에서 재현됐습니다. 해당 entry는
HLE 직후 일반 명령에서 시작했지만 그 직선 block 내부에 `0x030FC777` HLE boundary가
포함되어 있었습니다. 따라서 entry 존재 여부는 안전성의 충분조건이 아닙니다.

직선 구간 HLE preflight만 추가한 세 번째 시도도 재현됐고 마지막 single-step EIP는
여전히 직전 `mov es, ax`였습니다. 이 경우 핵심 상태 전이는 직선 구간 자체보다
**segment selector 변경 직후**라는 점입니다. 물리 host segment와 guest shadow/folded
base 의미가 다시 안정되는 다음 HLE 경계까지 TF bridge를 유지해야 합니다.

첫 `aot-dbt` increment는 cache hit에 더해 현재 주소부터 첫 control transfer까지
최대 64개 명령을 사전 디코드하고, 등록된 HLE boundary가 하나라도 있으면 거부합니다.
방금 처리한 명령이 segment register를 쓰는 경우도 즉시 복귀를 금지합니다. cache
miss와 사전 검사 실패는 기존 TF bridge를 유지합니다. HLE 직후 cache miss까지 직접
번역하려면 동적 planner가 live selector 0 저메모리 의미를 반영하거나, 생성 CFG 전체의
HLE boundary를 보장하는 별도 검증이 먼저 필요합니다.

### 미확정

- 기존 cache hit만으로 줄어드는 single-step의 실제 비율
- 기존 cache hit만으로 줄어드는 single-step의 장기 wall-clock 효과

### Task 289 Stage 1에서 확정된 selector 정책

Task 264의 기존 shadow 주소/값과 segment-site patch ABI를 재사용하되 resolution에
descriptor base/limit/flags와 명시적 native/HLE/unresolved 정책을 추가했습니다. selector
0과 전 구간이 DOS low-memory에 속하는 descriptor는 code-cache 첫 바이트를 `INT3`로
유지합니다. 정상 nonzero flat descriptor와 Task 264에서 검증한 GS non-flat base-add
descriptor만 기존 self-correcting guard를 활성화합니다. 재해석 self-gate도 selector만이
아니라 전체 descriptor fingerprint를 비교합니다.

따라서 “live selector 정책을 전달하는 최종 구조”는 확정됐습니다. 남은 미확정 항목은
이 정책을 전제로 한 whole-CFG preflight와 post-HLE cache-miss 번역의 비용·효용입니다.

Task 289 Stage 2는 모든 planner HLE record가 실제 `INT3`이거나 mismatch가 `INT3`으로
가는 완전한 selector guard인지 생성 image 전체를 검사합니다. 누락 합성 probe는
거부됩니다. opt-in `REPIU_AOT_DBT_POST_HLE_TRANSLATE=1`은 segment-write와 quarantine
장벽을 유지한 채 이 검사를 통과한 cache miss만 번역합니다. 그러나 10초와 60초 A/B
모두 번역 시도/성공이 `0/0`이었습니다. 현재 안전한 post-HLE 후보는 기존 cache hit로
이미 처리되므로 기능은 기본 OFF로 보류합니다.

### 최종 Task 276 관측

segment-register write barrier와 직선 구간 preflight를 함께 적용한 뒤 30초 supervisor
실행은 끝까지 진행했고 fatal/legacy fallback은 0이었습니다. 이어 loader 자체의
30초 graceful timeout으로 최종 계측을 회수했습니다.

| 지표 | `aot-dynamic` | `aot-dbt` |
|---|---:|---:|
| single-step | 189,656 | 127,940 |
| progress | 10,709 | 10,685 |
| AOT boundary/re-entry | 15,267/15,278 | 12,711/12,722 |
| DBT HLE 즉시 복귀 시도/성공 | 0/0 | 5,670/2,335 |
| fatal | 0 | 0 |
| legacy fallback | 1 | 0 |
| window open | 1 | 1 |

두 실행은 초기화와 hot phase 진입 시점이 달랐으므로 원시 single-step 차이 `-32.5%`를
성능 개선으로 해석하지 않습니다. DBT 실행 자체에서 성공 2,335회는 각각 기존 TF
명령 하나를 제거합니다. 같은 실행이 즉시 복귀 없이 `127,940 + 2,335` single-step을
수행했을 것으로 보는 국소 proxy 절감은 약 **1.8%**입니다. progress는 사실상 같아
wall-clock 개선은 아직 확인되지 않았습니다. 두 EEPROM 사본은 원본 SHA-256과
일치했습니다.

## English

The first Task 276 implementation dynamically translated a cache miss immediately
after an HLE handler advanced EIP. `aot-dynamic` completed the controlled run, but
the first `aot-dbt` run terminated around four seconds with `0xC0000005` after
42/42 immediate re-entries.

The cache fault mapped to guest `0x030FC777`, whose instruction was
`26 80 38 00` (`cmp byte ptr es:[eax], 0`) with EAX zero. It followed
`8E C0` (`mov es, ax`). Generated code folded selector base zero into a direct
`[eax+0]` access, but selector zero at this point requires DOS low-memory HLE
semantics. The established one-step bridge reaches the exact segment-override
address, where HLE-boundary rejection preserves the low-memory handler. Starting
a new arbitrary-entry CFG at the preceding address can absorb the override into
the generated block and bypass that exact-address check.

Restricting the second attempt to an existing cache-entry hit reproduced the same
fault: the entry started at an ordinary instruction but its straight-line block
contained the `0x030FC777` HLE boundary. Entry existence is therefore not a
sufficient safety condition.

A third attempt with straight-line HLE preflight also reproduced the fault, with
the last single-step EIP still at the preceding `mov es, ax`. The decisive state
transition is therefore the segment-selector write itself: the TF bridge must
remain until the following HLE boundary stabilizes host-segment and guest
shadow/folded-base semantics.

The first `aot-dbt` increment now also decodes up to 64 instructions through the
first control transfer and rejects any registered HLE boundary. It also prohibits
immediate re-entry after an instruction that writes a segment register. A cache
miss or preflight failure retains the TF bridge. Extending immediate translation
to misses requires either live selector-zero policy in the planner or
whole-generated-CFG HLE-boundary guarantees.

Task 289 Stage 1 resolves the selector-policy prerequisite by reusing Task 264's shadow
address/value and segment-site patch ABI while adding explicit descriptor
base/limit/flags and native/HLE/unresolved policy. Selector zero and descriptors wholly in
DOS low memory retain an `INT3` cache boundary. Valid nonzero flat descriptors and the
Task-264-proven GS non-flat base-add descriptor keep the self-correcting guard. The
re-resolution gate fingerprints the complete descriptor rather than only the selector.
The remaining open item is the cost and value of whole-CFG preflight plus post-HLE miss
translation under this policy.

Task 289 Stage 2 validates the complete generated image: every planner HLE record must be an
actual `INT3` or a complete selector guard whose mismatch reaches `INT3`. A synthetic missing
guard is rejected. `REPIU_AOT_DBT_POST_HLE_TRANSLATE=1` preserves segment-write and
quarantine barriers and translates only a cache miss that passes this preflight. Both
10-second and 60-second A/B pairs nevertheless recorded `0/0` translation attempts/successes;
the current safe post-HLE population is already served by cache hits. The feature remains
default off.

With both guards enabled, a 30-second supervisor run completed with zero fatal
state and zero legacy fallback. A subsequent graceful 30-second loader run
recorded 5,670 attempts and 2,335 successful immediate HLE re-entries. The raw
single-step totals were 189,656 for `aot-dynamic` and 127,940 for `aot-dbt`, but
different initialization/hot-phase entry times make that raw 32.5% difference
non-comparable. Each DBT success removes exactly one TF instruction, giving a
conservative within-run proxy reduction of `2,335 / (127,940 + 2,335)`, or about
1.8%. Progress was effectively unchanged, so wall-clock improvement is not yet
confirmed. Both isolated EEPROM copies retained the original SHA-256.

## 한국어 — Task 289 Stage 3 provenance 결론

`other` guest-opcode 분류는 cache breakpoint의 생성 원인을 뜻하지 않습니다. placement
metadata를 기준으로 60초 실측한 결과 HLE/segment/inline/jump-table/retired/probe/fixup/
unknown은 `22,248/7,064/34,912/0/7,298/0/0/0`이었습니다. completed image에는 미해소
direct/fallthrough fixup이 없고 unknown도 0이므로, 별도의 arbitrary miss/fallthrough
host-dispatch 모집단은 확인되지 않았습니다. Stage 3b는 정확성 우선 원칙에 따라 보류합니다.

## English — Task 289 Stage 3 provenance conclusion

The guest-opcode `other` classification is not the origin of a cache breakpoint. A 60-second
placement-metadata census recorded HLE/segment/inline/jump-table/retired/probe/fixup/unknown
as `22,248/7,064/34,912/0/7,298/0/0/0`. A completed image has no unresolved direct/fallthrough
fixup and unknown is zero, so no separate arbitrary-miss/fallthrough host-dispatch population
was observed. Stage 3b remains held under the accuracy-first policy.

## 한국어 — Task 308 정상 호출 HLE 결론

**확인됨:** cache-local target이 있는 안전 HLE는 `INT3/VEH` 없이 기존 공용 handler를
정상 호출하고 즉시 cache로 복귀할 수 있습니다. Win32 x86 thunk는 GPR/EFLAGS,
x87/MMX/SSE와 host stack/TIB 경계를 보존합니다. 60초 실행에서 직접 성공은
25,134회였고 target miss/state mismatch/unknown은 모두 0이었습니다.

**확인됨:** target miss는 HLE side effect가 이미 committed된 뒤이므로 원본 source
`INT3`로 fallback할 수 없습니다. 처리된 다음 guest EIP에서 TF bridge를 재개합니다.

**확인됨:** software interrupt는 이 ABI의 안전 모집단이 아닙니다. 직접
`INT 21h AH=25h`가 INT 8 selector를 OFF의 `002B`와 다른 `0023`으로 저장했고 후속
AV를 만들었습니다. 모든 `INT/IRET`와 segment/ESP write는 VEH 경계에 남깁니다.

**결론:** 안전 subset은 single-step 7.88%, AOT boundary 37.77%를 줄였지만 progress는
1.64%만 늘었습니다. HLE 예외 횟수는 관측 가능한 비용이지만 5배 또는 60배 격차의
주원인은 아닙니다.

## English — Task 308 normal-call HLE conclusion

**Confirmed:** A safe HLE site with an active cache target can invoke the shared handler
normally and return directly to cache without `INT3/VEH`. The Win32 x86 thunk preserves
GPR/EFLAGS, x87/MMX/SSE state, and host stack/TIB bounds. The 60-second run recorded 25,134
direct successes and zero target-miss, state-mismatch, or unknown failures.

A target miss after a committed HLE cannot re-execute the source `INT3`; it resumes the
handled next guest EIP through the established TF bridge.

Software interrupts are not part of this safe ABI. Direct `INT 21h AH=25h` changed the INT 8
selector from the established `002B` to `0023` and led to a later AV. All `INT/IRET` forms
and segment/ESP writes remain VEH-mediated.

The safe subset reduced single-step by 7.88% and AOT boundaries by 37.77%, but improved
progress by only 1.64%. HLE exception count is measurable overhead, not the principal cause
of the 5x or 60x gap.
