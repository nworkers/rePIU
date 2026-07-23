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
- selector 0을 포함한 live segment policy를 planner/emitter에 전달하는 최종 구조
- HLE 직후 생성 CFG의 전체 boundary preflight 비용과 효용

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

With both guards enabled, a 30-second supervisor run completed with zero fatal
state and zero legacy fallback. A subsequent graceful 30-second loader run
recorded 5,670 attempts and 2,335 successful immediate HLE re-entries. The raw
single-step totals were 189,656 for `aot-dynamic` and 127,940 for `aot-dbt`, but
different initialization/hot-phase entry times make that raw 32.5% difference
non-comparable. Each DBT success removes exactly one TF instruction, giving a
conservative within-run proxy reduction of `2,335 / (127,940 + 2,335)`, or about
1.8%. Progress was effectively unchanged, so wall-clock improvement is not yet
confirmed. Both isolated EEPROM copies retained the original SHA-256.
