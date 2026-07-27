# 네이티브 실행 single-step 병목 / Native Execution Single-Step Bottleneck

## 확인됨 / Confirmed

32-bit DOS/4GW read ABI 복원 후 PIU는 `Not PTX file`로 종료하지 않는다. 120초 관찰에서 heartbeat는 `27,231,182`, dispatch count는 `13,615,591`, progress는 `1,320,177`까지 계속 증가했다. 예외나 guest fatal 출력은 없었으며 관찰 제한이 실행을 종료했다.

약 23초 이후 표본 EIP 대부분은 relocated `0x030EE1xx`에 집중된다. object 2 base `0x03010000`를 빼면 `+0xDE1xx`이며, 원본 file offset `0x1005xx`의 bit 단위 unpack/decode loop와 대응한다. 주변 코드는 shift, mask, table lookup을 반복한다. 이는 정지한 wait loop가 아니라 PTX/resource 처리 계산이 진행되는 경로다.

현재 trampoline은 guest 진입 시 Trap Flag를 설정하고, 모든 single-step 예외 처리 후 다시 Trap Flag를 설정한다. 120초 동안 약 1,361만 명령만 처리된 이유는 원본 계산량 자체보다 명령마다 VEH dispatcher를 통과하는 구조적 비용이다.

```mermaid
flowchart LR
    G[guest instruction] --> TF[#DB single-step]
    TF --> VEH[Vectored Exception Handler]
    VEH --> CHECK[DOS/DPMI/segment/HLE 검사]
    CHECK --> SET[Trap Flag 재설정]
    SET --> G
```

## 현재 부족한 부분 / Current Gap

새로운 guest ABI나 파일 데이터가 부족하다는 증거는 아직 없다. 현재 확인된 부족한 부분은 **안전한 native fast path**다. 일반 산술·분기·메모리 명령을 여러 개 연속으로 네이티브 실행하면서도 `INT`, privileged instruction, software segment semantics, LINEXE/Glide gate를 정확히 가로채는 실행 경계가 필요하다.

## English

After restoring the 32-bit DOS/4GW read ABI, PIU no longer terminates through `Not PTX file`. During a 120-second observation, heartbeat reached `27,231,182`, dispatch count `13,615,591`, and progress `1,320,177`, with no guest fatal output or exception. The observation limit ended the run.

Most sampled EIPs after about 23 seconds fall in relocated `0x030EE1xx`, object 2 `+0xDE1xx`, corresponding to a bit-oriented unpack/decode loop near original file offset `0x1005xx`. The path is computing rather than waiting. The trampoline currently sets Trap Flag on entry and after every handled single-step exception, routing every guest instruction through the VEH dispatcher. The confirmed gap is therefore a safe native fast path, not yet another missing file or guest ABI.

## 첫 native fast path 검증 / First Native Fast-Path Verification

**확인됨:** object 2 `+0xDE170` 함수에 relocation-aware signature 검증과 return-address hardware breakpoint를 적용했다. 모듈 분리 후 30초 실행에서 fast path는 `9,242`회 진입하고 `9,242`회 정상 반환했으며 취소는 0회였다. 기존 PTX fatal이나 새로운 예외는 발생하지 않았다.

병목 표본은 `+0xDE1xx`에서 인접한 `+0xDE2xx` 및 이후 helper로 이동했다. progress는 같은 30초 규모에서 약 `705,486`이므로 첫 함수 하나만으로 전체 처리량이 크게 개선되지는 않았다. 다음 단계에는 개별 signature 항목을 계속 추가할지, 안전성을 정적으로 판정하는 공용 region verifier로 확장할지 결정해야 한다.

**Confirmed:** A relocation-aware signature and return-address hardware breakpoint were applied to object 2 `+0xDE170`. After extracting the implementation into its own module, a 30-second run recorded `9,242` entries, `9,242` normal returns, and zero cancellations, without the former PTX fatal or a new exception. Samples moved into adjacent helpers at `+0xDE2xx` and beyond; accelerating one function alone does not materially remove the total bottleneck.

## 공용 decoder 프로토타입 / Generic Decoder Prototype

**확인됨:** direct `CALL` candidate와 보수적 CFG 순회를 구현한 자체 decoder 프로토타입은 207개 함수 진입을 처리했지만, 핵심 unpack call graph에서 `29 CF`를 `29`와 `CF`로 잘못 분리했다. operand byte `CF`를 `IRET`로 오인했으므로 instruction boundary 안전성을 보장할 수 없다. 프로토타입은 fail-closed 상수로 비활성화했으며 production fast path로 사용하지 않는다.

정확한 x86 instruction boundary와 operand/control-flow metadata를 위해 MIT License의 Zydis를 pinned dependency로 도입하기로 결정했다. 자체 decoder는 Zydis adapter와 rePIU 고유 안전 정책으로 교체할 예정이다.

**Confirmed:** The in-house direct-call/CFG decoder prototype handled 207 function entries but split `29 CF` into opcode `29` followed by operand byte `CF` in the critical unpack call graph. Misclassifying that operand as `IRET` means instruction-boundary safety is not established. The prototype is disabled by a fail-closed constant and is not used as a production fast path. It will be replaced by a pinned MIT-licensed Zydis decoder plus rePIU-specific safety policy.

## Zydis 기반 검증 결과 / Zydis-Based Verification Result

**확인됨:** Zydis v4.1.1 legacy-32 decoder로 자체 instruction-length decoder를 교체했다. 첫 30초 실행에서 fast path는 `12,134`회 진입, `12,117`회 정상 반환, `17`회 안전 취소를 기록했다. 취소된 함수는 이후 cache에서 영구 거부하도록 보완했으며, 60초 재검증에서는 `19,437/19,431/6` entry/return/cancel을 기록했다. guest fatal이나 `Not PTX file`은 발생하지 않았다.

기존 실행은 30~120초 동안 주로 object 2 `+0xDE1xx` bit-unpack loop에 머물렀다. Zydis 적용 후 30초 시점에는 `+0x76Dxx~+0x774xx`, 36초 이후 `+0x479xx`, 57초 이후 다시 여러 resource 처리 구간으로 진행했다. 따라서 원본 unpack call graph가 실제로 native 실행되어 기존 단일 병목을 통과한 것이 확인된다. 새로 확인된 필수 HLE 누락은 아직 없다.

**Confirmed:** Replaced the in-house length decoder with pinned Zydis v4.1.1 legacy-32 decoding. The first 30-second run recorded `12,134/12,117/17` entries/returns/cancellations. After permanently rejecting a function on intermediate exception, a 60-second run recorded `19,437/19,431/6` without guest fatal output or `Not PTX file`. Execution no longer remains in object 2 `+0xDE1xx`; samples advance through `+0x76Dxx~+0x774xx`, `+0x479xx`, and subsequent resource-processing regions, confirming that the original unpack call graph passes the former bottleneck.

## 네이티브 span 거절 decode 비용 / Native-span rejection decode cost

**확인됨:** 약 881초 `aot-dbt` 실행은 native linear-span
`entry/boundary/reject=1,982,870/1,967,120/2,747,330`을 기록했습니다. 기본 scanner의
거절은 동일 entry에서도 매번 Zydis decode를 반복했습니다. Task 304의 byte-validated
음성 캐시는 세 번의 60초 ON 실행에서 거절의 99.68~99.69%를 재사용했습니다.

후반 progress 변화 중앙값은 `+0.02%`로 exception 횟수 자체를 줄이지 않는 decode
최적화의 전체 처리량 효과는 작았습니다. 그러나 texture milestone은 세 쌍 모두
빨라졌고 중앙값은 `1,031ms`(약 4.9%)였습니다. 따라서 이 비용은 초기 resource decode
구간에는 유의하지만, 남은 장기 병목은 여전히 single-step/retired breakpoint 같은 예외
횟수입니다.

**Confirmed:** An approximately 881-second `aot-dbt` run recorded native linear-span
`entry/boundary/reject=1,982,870/1,967,120/2,747,330`; repeated entries were decoded again by
Zydis on every rejection. Task 304's byte-validated negative cache reused 99.68-99.69% of
rejections in three 60-second ON runs. Median late progress changed only `+0.02%`, confirming
that decode caching does not remove the exception-count bottleneck, but every texture
milestone improved with a 1,031ms median (about 4.9%). The remaining long-run frontier is
therefore exception frequency, including single-step and retired breakpoint traps.

## Retired trap 직후 span 실험 / Immediate span experiment after retired traps

**확인됨:** Task 305는 active/new generation으로 해결되지 않은 retired cache trap에서 기존
native-span scanner를 즉시 시도했습니다. 첫 구현은 pending/trace 상태를 잘못 해제하여 첫
실제 성공 직후 `RET(C3)` 경계 처리를 건너뛰었고 약 19.5초에 종료됐습니다. 이 상태는
경계까지 반드시 보존해야 한다는 실행 계약을 확인했습니다.

수정 후 세 번의 30초 교차 A/B에서 span 성공률은 95.28~95.46%, single-step 감소는
`2.94% / 2.86% / 2.65%`였습니다. 반면 progress 변화는 `+0.35% / -0.03% / +0.45%`,
중앙값 `+0.35%`였고 texture 변화 중앙값은 `-17ms`였습니다. fatal은 모두 0이고 EEPROM
hash는 일치했습니다. 따라서 retired trap 일부를 single-step 없이 처리할 수 있다는 것은
확인됐지만 현재 scanner/Dr0 비용을 포함한 순 처리량 이득은 작으며, 기능은 opt-in입니다.

**Confirmed:** Task 305 immediately tried the existing native-span scanner when a retired
cache trap could not resolve to an active or new generation. The first implementation
incorrectly cleared pending/trace state, skipped `RET(C3)` handling at the first real boundary,
and ended around 19.5 seconds. This confirmed that the state must survive until the boundary.

After the fix, three 30-second alternating pairs observed a 95.28-95.46% span success rate and
single-step reductions of `2.94% / 2.86% / 2.65%`. Progress changed by
`+0.35% / -0.03% / +0.45%` (median `+0.35%`), and median texture change was `-17ms`.
All runs kept zero fatal events and matching EEPROM hashes. Retired traps can therefore skip
some single-step work, but the net throughput benefit after scanner/Dr0 cost is small, so the
feature remains opt-in.

## Retired trap hotset 측정 / Retired-trap hotset measurement

**확인됨:** Task 306의 60초 opt-in profile은 retired trap `7,401`회, guest 주소 61개,
cache 주소 146개를 기록했으며 histogram overflow와 metadata miss는 모두 0이었습니다.
guest 상위 16개 coverage는 98.24%였습니다. `0x030F4A94` 2,850회(38.51%)와
`0x030F507C` 1,891회(25.55%)만 합쳐도 64.06%입니다.

전체의 7,293회(98.54%)는 emitted length가 5바이트 미만인 entry였고 resolver 결과도
모두 quarantine이었습니다. relink 가능한 108회는 generation publish 107회와 failure
1회였습니다. 따라서 현재 `E9 rel32` 재연결 범위를 늘리는 것으로는 대부분의 예외를
줄일 수 없습니다. 다음 성능 후보는 1~4바이트 retired entry를 side table 또는 공용
dispatch gate를 통해 `INT3` 예외 없이 최신 generation/guest fallback으로 보내는 경로입니다.

실행은 내부 60초 timeout까지 도달했고 AOT legacy fallback은 0, guest terminal fatal
count는 0, EEPROM hash는 fixture와 일치했습니다. Glide 미구현 함수는 기존 정책대로
`[repiu-fatal] ... action=continue` 진단으로 남지만 guest 실행을 종료시키지는 않았습니다.

**Confirmed:** Task 306's opt-in 60-second profile recorded 7,401 retired traps across 61
guest addresses and 146 cache addresses, with zero histogram overflow or metadata misses.
The guest top 16 covered 98.24%. `0x030F4A94` contributed 2,850 events (38.51%) and
`0x030F507C` contributed 1,891 (25.55%), for 64.06% combined.

7,293 events (98.54%) came from entries shorter than five emitted bytes and all resolved as
quarantine. The 108 relinkable events split into 107 generation publications and one failure.
Extending the current `E9 rel32` relink therefore cannot remove most exceptions. The next
performance candidate is a side table or shared dispatch gate that redirects one-to-four-byte
retired entries to the latest generation or guest fallback without raising `INT3`.

The run reached its internal 60-second timeout with zero AOT legacy fallback, zero terminal
guest fatal count, and an EEPROM hash matching the fixture. Existing unimplemented Glide
calls remained explicitly labeled `[repiu-fatal] ... action=continue` diagnostics without
terminating guest execution.

## Exception-free HLE 경계의 실제 상한 / Measured limit of exception-free HLE boundaries

Task 308은 planner-HLE `INT3/VEH`를 정상 host-call로 바꾸어 “예외 횟수 자체가 전체
병목”이라는 가설을 직접 검증했습니다. 안전 slice의 60초 OFF/ON 결과는 다음과 같습니다.

| 지표 | OFF | ON | 변화 |
|---|---:|---:|---:|
| progress | 44,977 | 45,716 | +1.64% |
| single-step | 276,680 | 254,889 | -7.88% |
| AOT boundary | 66,245 | 41,224 | -37.77% |
| 직접 HLE 성공 | 0 | 25,134 | +25,134 |

예외 경계를 25,021회 줄이고 single-step도 21,791회 줄였지만 progress는 739만
증가했습니다. 따라서 현재 실행에서 “일반 HLE 예외 제거”의 wall-clock 상한은 5배
기준과 질적으로 다릅니다. 다음 계측은 count가 아니라 CPU 시간을 short retired
hotset, native-span scanner/Dr0, VEH 내부, host HLE와 guest loop별로 나누어야 합니다.

Task 308 directly tested whether exception count itself dominates whole-run time by replacing
planner-HLE `INT3/VEH` exits with normal host calls for a safe subset.

| Metric | OFF | ON | Change |
|---|---:|---:|---:|
| progress | 44,977 | 45,716 | +1.64% |
| single-step | 276,680 | 254,889 | -7.88% |
| AOT boundary | 66,245 | 41,224 | -37.77% |
| direct HLE success | 0 | 25,134 | +25,134 |

Removing 25,021 exception boundaries and 21,791 single steps yielded only 739 additional
progress units. The next profile must attribute CPU time—not counts—to short retired hotsets,
native-span scanning/Dr0, VEH work, host HLE, and individual guest loops.

## Single-step EIP별 handler latency / Handler latency by single-step EIP

Task 309는 `REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1|on|true`에서
`HandleSingleStepTrace`의 guest EIP별 count와 TSC latency tick을 기록했습니다.
60초 `aot-dbt`, superblock OFF 실행은 single-step 272,543개를 1,132개 EIP로
분류했으며 histogram overflow는 0이었습니다. count 상위 32 coverage는 49.11%,
cycle 상위 32 coverage는 67.21%였습니다.

| outcome | event 비율 | handler tick 비율 | 평균 tick/event |
|---|---:|---:|---:|
| HLE | 33.60% | 84.82% | 186,160 |
| timer | 0.07% | 0.10% | 106,576 |
| native 진입 | 29.15% | 10.37% | 26,239 |
| 일반 TF 재설정 | 37.18% | 4.71% | 9,338 |

cycle 상위 두 주소는 `0x030F940E: mov edx, ds` 11.10%와
`0x030F536A: mov eax, ds` 9.32%였습니다. 그 다음 상위 집단은
`0x0303BDAA`, `0x0303C795`, `0x0303C758`, `0x0303C779`,
`0x0303BDC3`, `0x0303BDF0`의 `IN/OUT` HLE였습니다. 상위 8개 합계는
43.09%로, 하나의 guest 계산 loop가 handler latency 80% 이상을 소유한다는
가설은 기각됐습니다.

**확인됨:** count hotspot과 latency hotspot은 다릅니다. 남은 handler 내부
latency는 일반 TF 재설정보다 segment/port-I/O HLE에 집중됩니다.

**미확정:** 이 TSC 범위는 kernel #DB 진입 전과 VEH 복귀 후를 포함하지 않고,
preemption이 sample을 부풀릴 수 있습니다. 따라서 전체 single-step 비용 또는 순수
CPU cycle로 해석할 수 없습니다. `DispatchGuestHleHandlers`의 순차 predicate/decode가
상위 HLE latency의 원인인지도 handler 내부 단계 계측 전에는 확정하지 않습니다.

Task 309 recorded guest-EIP counts and TSC latency ticks inside `HandleSingleStepTrace` when
`REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1|on|true`. A 60-second `aot-dbt`, superblock-OFF run
classified all 272,543 steps across 1,132 EIPs with no histogram overflow. The top 32 covered
49.11% of events and 67.21% of measured ticks.

HLE represented 33.60% of events but 84.82% of handler ticks, averaging 186,160 ticks per
event. Ordinary TF re-arm represented 37.18% of events and only 4.71% of ticks, averaging
9,338. The leading sites were segment-register moves and port-I/O HLE, and the top eight
covered 43.09%. This rejects the hypothesis that one guest compute loop owns at least 80% of
handler latency.

The scope does not include kernel #DB entry or work after VEH returns, and preemption can
inflate TSC latency. It is not a pure CPU-cycle or whole-single-step measurement. Whether
sequential predicate/decode work in `DispatchGuestHleHandlers` causes the hot HLE latency
remains unresolved until handler-stage attribution is added.

## Handler 단계별 귀속 / Handler stage attribution

Task 322는 `HandleSingleStepTrace`를 상호 배타적인 5개 단계로 나누어 위 미확정 항목을
해소했습니다. 60초 `aot-dbt` 실행은 표본 53,628개, distinct EIP 717개, overflow 0으로
총 handler tick `32,730,038,317`을 기록했습니다.

| 단계 | count | TSC tick | 전체 tick 비율 | 평균 tick/호출 |
|---|---:|---:|---:|---:|
| `kPrologueTrace` | 53,628 | 433,120,960 | 1.32% | 8,076 |
| `kHleDispatch` | 53,628 | 7,716,478,628 | 23.58% | 143,891 |
| **`kAotResume`** | **39,335** | **24,233,585,450** | **74.05%** | **616,079** |
| `kInterruptInjection` | 14,284 | 12,071,488 | 0.04% | 845 |
| `kNativeEntry` | 14,254 | 245,876,061 | 0.75% | 17,249 |
| `residual` (파생) | — | 88,905,730 | 0.27% | — |

**확인됨:** handler 내부 비용은 emulate 본체가 아니라 `TryResumeAotAfterHandledHle`가
지배합니다. 한 호출의 평균은 `616,079 tick`이며, 이 기기의 TSC 공칭 주파수 2.5GHz
(i5-7200U) 기준으로 약 `246us`입니다. HLE tick 안에서 `kAotResume`은 75.29%입니다.

**정정 (Task 323):** 이 비용의 원인을 "동적 번역"으로 본 최초 해석은 **기각됐습니다.**
`TryResumeAotAfterHandledHle`의 cache miss 경로는 opt-in
`REPIU_AOT_DBT_POST_HLE_TRANSLATE`가 꺼져 있으면 `PostHleTranslationEnabled()`에서
즉시 반환하며, 두 실행 모두 live telemetry가 `posthle=0/0`을 기록했습니다. 즉
`ResolveAotTransferTarget`은 한 번도 호출되지 않았고, `aot-code-cache-emission.md`의
`7,847.2us` cache 생성 비용은 이 수치와 무관합니다.

**추정:** 코드 판독상 남은 후보는 `FindAotCacheAddress`의 `address_map` 선형 스캔,
`IsAotHleBoundaryAddress`의 선형 스캔(최대 64회 반복), 그리고 두 helper가 매 호출
수행하는 `ZydisDecoderInit` 재초기화입니다. Task 323이 이를 4구간으로 분해해
확정합니다.

**확인됨:** 상시 진단 계측(`kPrologueTrace`)은 1.32%로, 이것이 hot path 비용을
지배한다는 가설은 기각됩니다. Task 312의 opcode-directed dispatcher 이후에도
`kHleDispatch` 평균은 `143,891 tick`으로 크지만 `kAotResume`보다 작습니다.

**미확정:** `kAotResume` 내부의 cache lookup, quarantine 판정, 동적 번역, worker
publication 비중은 나누지 않았습니다. `kHleDispatch` 평균의 내역도 미확정입니다.

**비교 제한:** 이 절대값은 Task 309와 직접 비교할 수 없습니다. Task 310~312가 segment
read와 port I/O를 AOT fast-path로 옮기고 Task 313~321이 Glide 경로를 바꾸면서
single-step 모집단이 `272,543`에서 `53,628`로 달라졌습니다. 남은 single-step의 평균
비용이 오히려 커진 원인은 규명되지 않았습니다.

```mermaid
flowchart LR
    S["single-step 53,628"] --> P["kPrologueTrace 1.32%"]
    S --> H["kHleDispatch 23.58%"]
    S --> A["kAotResume 74.05%"]
    A --> T["번역 캐시 재진입이 병목<br/>roadmap 1단계 확정"]
```

Task 322 resolved the preceding open question by splitting `HandleSingleStepTrace` into five
mutually exclusive stages. Over a 60-second `aot-dbt` run of 53,628 samples across 717
distinct EIPs with zero overflow and `32,730,038,317` total handler ticks, `kAotResume` held
74.05% of all ticks and 75.29% of HLE ticks at an average `616,079 ticks`, about `246us` at
this machine's 2.5GHz nominal TSC. The bottleneck inside the handler is
`TryResumeAotAfterHandledHle`, not the emulation body.

**Corrected in Task 323:** the initial attribution of that cost to dynamic translation is
rejected. The cache-miss path returns at the opt-in `PostHleTranslationEnabled()` gate and both
runs recorded `posthle=0/0`, so `ResolveAotTransferTarget` was never called and the
`7,847.2us` cache-generation cost is unrelated. The remaining candidates read from the code are
the linear `address_map` scan in `FindAotCacheAddress`, the linear `IsAotHleBoundaryAddress`
scan repeated up to 64 times, and per-call `ZydisDecoderInit` re-initialization in both
helpers. Task 323 decomposes these.

Always-on diagnostics accounted for 1.32%, rejecting the hypothesis that instrumentation
dominates the hot path. `kHleDispatch` averaged `143,891 ticks` even after the Task 312
opcode-directed dispatcher, but at 23.58% it remains smaller than `kAotResume`. The split
inside `kAotResume` and the composition of `kHleDispatch` remain unresolved. These absolute
values are not comparable to Task 309: Tasks 310-312 moved segment reads and port I/O onto AOT
fast paths and Tasks 313-321 changed the Glide path, shifting the single-step population from
272,543 to 53,628, and why the surviving steps became more expensive per event was not
investigated.

## kAotResume 내역 확정과 전체 실행 시간 귀속 / kAotResume composition and whole-run attribution

Task 323은 `TryResumeAotAfterHandledHle`를 4구간으로 나누고 guest thread wall-clock을
bucket으로 나누어 두 미지수를 동시에 해소했습니다.

`kAotResume` 총 `12,987,145,872 tick` 중 내역은 다음과 같습니다.

| 하위 단계 | count | TSC tick | 비율 |
|---|---:|---:|---:|
| `kSegmentWriteProbe` | 21,547 | 249,595,268 | 1.92% |
| `kQuarantineCheck` | 10,876 | 156,083,004 | 1.20% |
| **`kCacheLookup`** | 10,876 | **11,395,704,478** | **87.75%** |
| `kSpanSafety` | 10,876 | 675,746,920 | 5.20% |
| residual | — | 510,016,202 | 3.93% |

**확인됨:** 원인은 `FindAotCacheAddress`의 `placement.address_map` 선형 탐색입니다.
호출당 평균 `1,047,784 tick`(2.5GHz 기준 약 419us)입니다. Zydis decode
(`kSegmentWriteProbe` 1.92%, `kSpanSafety` 5.20%)와 quarantine 판정은 부차적입니다.

guest thread wall-clock 분모 `162,848,392,105 tick`(약 65.1초) 기준 귀속은 다음과
같습니다.

| bucket | TSC tick | 비율 |
|---|---:|---:|
| VEH handler 본문 — AOT boundary 경로 | 120,110,679,227 | **73.76%** |
| VEH handler 본문 — single-step handler | 20,559,155,309 | 12.62% |
| AOT cache 내 guest 실행 (추정) | 약 20.2e9 | 약 12.4% |
| Glide gate | 2,104,393,724 | 1.29% |
| kernel 예외 전이 (추정) | 약 1.95e9 | **1.20%** |
| DOS service | 236,072,055 | 0.14% |
| port I/O device | 24,285,813 | 0.01% |

**확인됨:** 예외 전이 비용은 전체의 1.20%입니다. 합성 교정 probe는 이 기기에서
`INT3` 왕복 `32,635 tick`, TF single-step 왕복 `34,015 tick`을 측정했고, 실행의 VEH
진입은 59,175회였습니다. 따라서 TF와 `INT3`를 전부 제거해도 상한은 약 1.012배이며,
"예외 왕복 비용이 지배적"이라는 TF/VEH 제거 로드맵의 전제는 기각됩니다.

**확인됨:** 병목은 예외 메커니즘이 아니라 handler 본문 안에서 반복되는 O(n) 선형
탐색입니다.

**미확정:** VEH 내부이면서 single-step handler 밖인 73.76%의 세부 귀속은 아직
없습니다. `ResolveAotTransferTarget`이 같은 `FindAotCacheAddress`를 호출하므로 같은
원인일 가능성이 높지만, 해시 맵 교체 A/B로 직접 검증해야 확정됩니다.

**한계:** Debug 빌드 측정입니다. MSVC Debug의 iterator debug check가 `std::vector`
순회를 크게 늦추므로 Release에서는 선형 탐색의 비중이 줄어듭니다. O(n)이라는 점근
성질 자체는 빌드 구성과 무관합니다.

Task 323 decomposed `TryResumeAotAfterHandledHle` and attributed guest-thread wall clock,
resolving both open unknowns. Within `kAotResume`, the linear `placement.address_map` scan in
`FindAotCacheAddress` holds 87.75% at an average `1,047,784` ticks (about 419us) per call,
while Zydis decoding and quarantine checks are secondary.

Against a `162,848,392,105` tick denominator, 73.76% of wall clock sits in the VEH handler body
outside the single-step handler on the AOT boundary path, 12.62% in the single-step handler,
roughly 12.4% in guest execution inside the AOT cache, 1.29% in the Glide gate, and 1.20% in
kernel exception transition. The calibration probe priced an `INT3` round trip at `32,635`
ticks and a TF single step at `34,015` on this machine across 59,175 VEH entries. Removing
every TF and `INT3` exception therefore bounds improvement at roughly 1.012x, rejecting the
premise that exception round-trip cost dominates. The bottleneck is the repeated O(n) scan
inside the handler body, not the exception mechanism.

The 73.76% remains unattributed at sub-stage granularity; `ResolveAotTransferTarget` calls the
same lookup, so a shared cause is likely but requires the hash-map A/B to confirm. The Debug
build inflates `std::vector` traversal through iterator debug checks, so the scan's share will
shrink in Release, though its O(n) behavior is build-independent.

## 해시 색인 교체 결과 / Hash index replacement result

Task 324는 `FindAotCacheAddress`의 `address_map` 선형 탐색을 버킷 체인 해시 색인으로
교체했습니다. 체인을 최신 head로 연결해 "retired 세대가 있으면 최신 active, 아니면
최초 삽입"이라는 두 규칙을 한 순회로 재현합니다. 색인은 캐시이며
`indexed_entry_count != address_map.size()`이면 기존 선형 탐색으로 fail-safe합니다.

| 지표 | 교체 전 | 교체 후 | 변화 |
|---|---:|---:|---:|
| `kCacheLookup` 호출당 | 1,047,784 tick | 6,866 tick | **-99.3%** |
| `kCacheLookup` 총합 | 11,395,704,478 | 148,029,142 | -98.7% |
| `kAotResume` 총합 | 12,987,145,872 | 2,220,773,822 | -82.9% |
| 60초 heartbeat | 79,640 | 331,913 | **+317%** |
| 60초 progress | 8,199 | 21,843 | **+166%** |

**확인됨:** 조회 의미는 보존됩니다. 차등 probe가 교체 이전 구현을 oracle로 유지하고
retired 목록이 빈 경우, retired/비retired 혼재, 최신 inactive + 이전 active, 전 세대
inactive, 강제 해시 충돌, 동적 append 300회와 버킷 성장, 색인 무효 fallback, 훅을
거치지 않은 placement의 8개 경계 조건에서 완전 일치를 확인했습니다.

**기각됨:** AOT boundary 경로가 같은 선형 탐색 때문에 느리다는 Task 323의 가설.
VEH 내부이면서 single-step handler 밖인 구간은 73.76%에서 74.34%로 **줄지 않았습니다.**
`ResolveAotTransferTarget`이 같은 함수를 호출하는 것은 사실이지만, 그 구간 비용의
지배 원인은 아니었습니다.

**해석 주의:** 실행이 빨라지면서 guest가 이전에 도달하지 못한 texture 로딩 구간까지
진행했고 phase 표시도 달라졌습니다. 따라서 전후의 stage 구성비는 동일 작업량 비교가
아닙니다. 다만 progress·heartbeat·single-step이 2.7~5.2배로 함께 올랐으므로 처리량
개선 자체는 phase 이동의 부산물이 아닙니다.

**한계:** Debug 빌드 수치입니다. MSVC Debug의 iterator debug check가 선형 탐색을 크게
부풀리므로 Release에서 상대 이득은 더 작습니다. O(n) → O(1)이라는 점근 개선만 빌드
구성과 무관합니다.

Task 324 replaced the linear `address_map` scan with a bucket-chain hash index whose chains
link newest-first, reproducing both lookup rules -- newest active entry when the guest address
has a retired generation, oldest entry otherwise -- in a single traversal. The index is a cache
that falls back to the original scan when stale. Per-call cost fell from `1,047,784` to `6,866`
ticks (-99.3%), 60-second heartbeat rose 4.17x, and progress rose 2.66x.

Semantic equivalence is verified by a differential probe that keeps the pre-change
implementation as an oracle across eight boundary conditions including forced hash collisions,
a newest-inactive entry behind an older active one, dynamic appends with bucket growth, and an
invalidated index.

The A/B rejected Task 323's hypothesis that the AOT boundary path was slow for the same reason:
the share inside the VEH but outside the single-step handler held at 74.34% rather than falling.
`ResolveAotTransferTarget` does call the same function, but that is not what dominates its cost.

Faster execution carried the guest into content it had not reached before, so before/after stage
shares are not a comparison over identical work, though the simultaneous 2.7x to 5.2x rise in
progress, heartbeat, and single-step counts shows the throughput gain is real. These are
Debug-build figures, where iterator debug checks inflate the linear scan, so the relative gain is
smaller in Release; only the O(n) to O(1) change is build-independent.
