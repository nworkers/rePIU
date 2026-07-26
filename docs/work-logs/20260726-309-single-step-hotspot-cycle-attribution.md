# 20260726-309 작업 로그: single-step hotspot cycle 귀속 / Work log: single-step hotspot cycle attribution

설계: [20260726-309-single-step-hotspot-cycle-attribution.md](../design/20260726-309-single-step-hotspot-cycle-attribution.md)

작업 지시: [20260726-309-single-step-hotspot-cycle-attribution.md](../work-orders/20260726-309-single-step-hotspot-cycle-attribution.md)

## 한국어

### 구현

`REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1|on|true`일 때만 생성되는 8,192-slot
guest-EIP histogram을 추가했습니다. 테이블은 `ThreadContext` stack에 넣지 않고
계측을 켰을 때만 heap에 할당합니다. guest hot path에서는 allocation과 lock 없이
주소별 count, total/max TSC tick과 HLE/timer/native/TF outcome별 count/tick을
기록합니다.

`HandleSingleStepTrace` 전체를 RAII scope로 감싸 모든 조기 반환을 포함했습니다.
종료 snapshot은 count 상위 32개와 TSC tick 상위 32개를 따로 정렬하고 각 coverage를
출력합니다. TSC는 handler 안에서 경과한 latency tick이며, kernel의 #DB 진입 전과
VEH 복귀 이후는 포함하지 않습니다. thread가 중간에 preempt되면 해당 sample의
latency도 증가하므로 이를 순수 CPU cycle로 해석하지 않습니다.

합성 프로브는 설정 parser, count와 cycle 정렬의 차이, outcome 합계, 8,192개
slot 포화와 overflow를 검증합니다.

### 60초 profile 결과

조건은 `aot-dbt`, superblock OFF, timeout 60초입니다.

| 실행 | progress | single-step | AOT boundary | profile sample |
|---|---:|---:|---:|---:|
| control OFF | 44,291 | 272,855 | 65,789 | 0 |
| profile ON | 44,307 | 272,543 | 65,644 | 272,543 |

OFF/ON progress 차이는 `+0.04%`, single-step 차이는 `-0.11%`로 실행 변동 범위입니다.
OFF snapshot은 enabled=false이고 count/cycle/outcome이 모두 0이었습니다. 두 실행 모두
정상 timeout, AOT legacy fallback 0, malformed dispatch 0과 동일 EEPROM hash를
유지했습니다.

- 정상 timeout, AOT legacy fallback 0, malformed dispatch 0
- progress 44,307, single-step 272,543, AOT boundary 65,644
- histogram sample 272,543, distinct EIP 1,132, overflow 0
- count 상위 32 coverage 49.11%, cycle 상위 32 coverage 67.21%
- EEPROM SHA-256
  `A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`

| outcome | count | count 비율 | TSC tick | tick 비율 | 평균 tick/event |
|---|---:|---:|---:|---:|---:|
| HLE | 91,568 | 33.60% | 17,046,293,980 | 84.82% | 186,160 |
| timer | 189 | 0.07% | 20,142,800 | 0.10% | 106,576 |
| native 진입 | 79,445 | 29.15% | 2,084,544,953 | 10.37% | 26,239 |
| 일반 TF 재설정 | 101,341 | 37.18% | 946,285,220 | 4.71% | 9,338 |

cycle 상위 주소는 다음과 같습니다. runtime address에서 `0x02000000`을 뺀 주소를
`repiu_aot_probe`에 질의해 원본 명령을 확인했습니다.

| 순위 | runtime EIP | 명령 | count | TSC tick | 전체 tick 비율 |
|---:|---:|---|---:|---:|---:|
| 1 | `0x030F940E` | `mov edx, ds` | 3,512 | 2,231,241,185 | 11.10% |
| 2 | `0x030F536A` | `mov eax, ds` | 4,837 | 1,873,425,081 | 9.32% |
| 3 | `0x0303BDAA` | `out dx, ax` | 1,024 | 815,914,489 | 4.06% |
| 4 | `0x0303C795` | `in ax, dx` | 1,024 | 765,639,543 | 3.81% |
| 5 | `0x0303C758` | `out dx, ax` | 1,024 | 756,527,868 | 3.76% |
| 6 | `0x0303C779` | `out dx, ax` | 1,024 | 755,635,019 | 3.76% |
| 7 | `0x0303BDC3` | `out dx, ax` | 1,024 | 745,670,435 | 3.71% |
| 8 | `0x0303BDF0` | `out dx, ax` | 981 | 715,595,510 | 3.56% |

상위 8개 합계는 43.09%입니다. 반면 count 상위 1~6은 모두 8,216회 반복된
`0x03034CD4..0x03034D2B` 묶음이지만 cycle 순위는 16위 이하입니다. 따라서
single-step 발생 횟수만으로 최적화 대상을 정하면 실제 handler 비용 순위를
잘못 판단합니다.

### 결론

**확인됨:** 남아 있는 single-step handler 내부 비용은 균등하지 않으며, HLE
outcome이 event의 33.60%로 TSC tick의 84.82%를 차지합니다. cycle 상위권은 하나의
순수 계산 loop가 아니라 segment register와 port-I/O HLE 지점의 집합입니다.
상위 32개도 67.21%이므로 한 guest loop가 80% 이상을 독점한다는 Task 309의
exception-free generation 조건은 충족하지 않습니다.

**추정:** `DispatchGuestHleHandlers`가 여러 후보 handler를 순차 검사하고 일부
handler가 반복 decode/probe를 수행하는 구조가 segment/I/O 지점의 높은 latency에
기여할 가능성이 큽니다. 이번 계측은 handler별 내부 단계를 분리하지 않았으므로
원인으로 확정하지 않습니다.

다음 단계도 single-step 병목에 집중하되, 새 superblock을 바로 확장하지 않습니다.
먼저 상위 segment/I/O EIP에서 opcode-directed dispatch와 decode 횟수를 계측해
HLE-associated single-step의 handler 비용을 줄일 수 있는지를 검증해야 합니다.
동시에 이번 TSC 범위 밖의 kernel/VEH 전환 비용은 별도 대조 실험으로 분리해야
single-step 제거의 whole-run 상한을 계산할 수 있습니다.

사용자 소유 `repiu_log.txt`는 수정하거나 staging하지 않았습니다.

## English

Added an opt-in 8,192-slot guest-EIP histogram enabled by
`REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1|on|true`. The table is allocated on the heap only when
profiling is enabled. The guest hot path performs no allocation or locking while recording
per-address count, total/max TSC ticks, and HLE/timer/native/TF outcomes.

An RAII scope covers all early returns from `HandleSingleStepTrace`. The final snapshot sorts
the top 32 addresses independently by count and by TSC ticks and reports both coverages. The
measurement is handler latency: it excludes the kernel's #DB entry and the path after VEH
returns, and a thread preemption can inflate a sample. It is therefore not labeled as pure CPU
cycles.

The Win32 x86 Debug build and all AOT probes passed. A controlled 60-second `aot-dbt`,
superblock-OFF profile completed with no AOT legacy fallback or malformed dispatch and with a
matching EEPROM hash. All 272,543 single steps were recorded across 1,132 EIPs with no
histogram overflow.

A same-build 60-second control run with profiling disabled reported zero profile samples and
progress 44,291 versus 44,307 with profiling enabled, a +0.04% difference within run
variation. Both runs had zero legacy fallback and malformed dispatch and matching EEPROMs.

HLE accounted for 33.60% of events but 84.82% of measured handler ticks. Ordinary TF re-arm
accounted for 37.18% of events and only 4.71% of ticks. The top cycle addresses were segment
register moves and port-I/O instructions. The top eight addresses covered 43.09% of ticks and
the top 32 covered 67.21%, so no single loop meets the 80% exception-free generation gate.

The next task should remain focused on single-step overhead, but should first attribute decode
and handler-chain work at the hot segment/I/O EIPs and test opcode-directed dispatch. Kernel
and VEH transition cost, which this scope cannot see, also needs a separate control experiment
before estimating the whole-run ceiling of eliminating single steps.
