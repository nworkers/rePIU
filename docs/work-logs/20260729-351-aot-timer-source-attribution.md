# 20260729-351 AOT timer source 귀속 작업 로그 / Work log

* 설계: [20260729-351-aot-timer-source-attribution.md](../design/20260729-351-aot-timer-source-attribution.md)
* 작업 지시: [20260729-351-aot-timer-source-attribution.md](../work-orders/20260729-351-aot-timer-source-attribution.md)
* 측정 산출물: `build/benchmarks/aot-timer-source/20260729-163659/` (Git 제외)

## 한국어

### 결과

기존 240Hz AOT timer safe point에서 실제 소비한 PIT tick을 원본 guest back-edge
source별로 귀속하는 opt-in profile을 추가했습니다. 원본 executable, ISR, `IRETD`,
tick 값과 cadence는 변경하지 않았습니다.

Release 60초 세 번에서 정적 tick 의존성이 확인된 pacing source는
`0x0303DE89` 하나였습니다. 귀속 tick 중앙값 1,416개는 divisor 4,972에서
5.900초, wall-clock의 **9.83%**입니다. interval 일부가 loop 진입 전 유효 작업일 수
있으므로 pacing 상한으로 해석합니다. Task 347의 guest 실행 유도값 60.72%에서
차감하면 active/unresolved guest 잔여는 보수적으로 **50.89%p**입니다.

### 기각한 suspend sampler

처음에는 `SuspendThread`/`GetThreadContext`로 guest EIP를 7ms/19ms 간격에
표본화했습니다. 세 10초 dry run은 다음과 같았습니다.

| 방식 | cycle 기반 cache 유도 | cache EIP 표본 | capture 정지 |
|---|---:|---:|---:|
| host poll, 7ms | 49.90% | 15.53% | 0.833% |
| 독립 thread, 7ms | 56.04% | 22.37% | 1.385% |
| 독립 thread, 19ms | 55.04% | 21.58% | 0.604% |

독립 thread의 host EIP 절반이 정확히 `ntdll!NtWaitForSingleObject+0xC`였고 syscall
stub에 표본이 집중됐습니다. 프레임 처리량도 크게 흔들렸습니다. kernel 경계에 편향된
정지 표본을 instruction residency로 사용할 수 없으므로 prototype 코드는 전부
폐기하고 실패 근거만 설계 문서에 남겼습니다.

### 구현

* `breakpoint_offset -> guest_source` map을 initial placement와 dynamic append에서
  유지합니다.
* host scheduler가 만료 PIT tick을 별도 atomic due ledger에 saturating-add합니다.
* 공용 `InjectPendingInterrupts`가 모든 성공 주입에서 ledger를 소비하고 tick 수를
  반환합니다. natural VEH 주입은 이를 source에 귀속하지 않아 stale tick을 막습니다.
* safe-point handler는 자기 주입 호출이 소비한 tick만 해당 source에 기록하고,
  deferred trap은 0 tick을 기록합니다.
* VEH hot path는 allocation/lock 없는 고정 1,024-entry profile을 사용합니다.
* 종료 attempt에 profile을 복사하고 모든 entry를 tick 순으로 출력합니다.
* 합성 AOT probe와 `scripts/task351_aot_timer_source_attribution.ps1`을 추가했습니다.
  wrapper는 Task 347의 동등성 검증을 재사용하고 profile completeness와 overflow를
  추가 검증해 전체 source CSV/집계를 생성합니다.

### 측정과 분류

| run | frames | source entry | 전체 귀속 tick | `0x0303DE89` tick | guest 유도 |
|---|---:|---:|---:|---:|---:|
| 1 | 1,114 | 99 | 5,658 | 1,414 | 61.08% |
| 2 | 1,123 | 94 | 5,674 | 1,416 | 61.13% |
| 3 | 1,111 | 106 | 5,638 | 1,430 | 60.89% |

전체 safe-point source의 tick 주기 환산 중앙값은 23.575초(39.29%)지만, 이는
interrupt delivery context의 합입니다. 상위 source 대부분은 렌더, 메모리 이동,
일반 계산 back edge였고 tick 의존성이 없으므로 pacing으로 분류하지 않았습니다.

확정 source의 원본 흐름은 다음과 같습니다.

```text
0x0303DE81  call 0x0304318F
0x0303DE86  cmp eax, 2
0x0303DE89  jl  0x0303DE81
```

`0x0304318F`는 critical-section helper 뒤 `0x032D9C80`을 읽고, 원본 INT 8 ISR은
`0x03042F3C`에서 이 값을 증가시킵니다. Task 348의
`0x0302FA10`/`0x032D9C84` wait는 이번 정상 route의 129개 source 합집합에
나타나지 않았습니다.

profile-on 프레임 중앙값 1,114는 Task 347의 1,124보다 0.89% 낮고, 기존
1,112~1,141 범위 안입니다. guest 실행 유도값도 61.08%로 Task 347의 60.72%와
0.36%p 차이여서 observer impact는 작다고 판정했습니다.

### 검증

* 합성 profile probe: `aot_timer_source_profile_all=true`
* Win32 x86 Release `repiu_aot_probe` 및 `repiu_loader_win32` 빌드: 성공
* 10초 smoke: 63 source, overflow 0, stale-ledger 과대 귀속 없음
* Release direct-loader 60초 3회: 정상 timeout 및 Task 347 semantic invariant 통과
* PIT divisor 4,972 / 240Hz: 세 실행 모두 유지
* source row count 및 tick 합: profile summary와 세 실행 모두 정확히 일치
* malformed/fatal/Glide issue: 세 실행 모두 0

### 다음 작업

확정 timer pacing은 guest 축의 지배 원인이 아닙니다. 다음 작업은 정지 기반 표본화가
아닌 비침습적 방법으로 남은 active/unresolved guest 50.89%p를 주소와 phase별로
귀속해야 합니다. Glide gate 21.73%는 그 다음 축으로 유지합니다.

---

## English

### Result

Added an opt-in profile that attributes PIT expirations actually consumed at
existing 240 Hz AOT timer safe points to exact original guest back-edge
sources. It changes neither the original executable, ISR, `IRETD`, tick
values, nor cadence.

Across three 60-second Release runs, only `0x0303DE89` had a statically proven
tick dependency. Its median 1,416 ticks equal 5.900 seconds at divisor 4,972,
or a conservative **9.83%** pacing upper bound. Subtracting that upper bound
from Task 347's derived 60.72% guest share leaves **50.89 percentage points**
as a conservative active/unresolved guest remainder.

### Rejected suspend sampler

The first prototype sampled guest EIP with
`SuspendThread`/`GetThreadContext`. Three ten-second dry runs derived
49.90--56.04% cache time but observed only 15.53--22.37% cache EIPs and paused
the target for 0.604--1.385% of wall time. Half of independent-thread host
samples landed exactly at `ntdll!NtWaitForSingleObject+0xC`, other syscall
stubs recurred, and frame throughput was materially disturbed. The
kernel-boundary-biased prototype was removed; only its rejection evidence
remains in the design.

### Implementation

Initial placement and dynamic append maintain an exact
`breakpoint_offset -> guest_source` map. The scheduler saturating-adds expired
PIT ticks to a separate atomic due ledger. Common `InjectPendingInterrupts`
consumes the ledger on every successful injection and returns the count.
Natural VEH delivery clears it without source attribution, preventing stale
ticks; only a safe-point handler attributes ticks consumed by its own call.
Deferred traps consume none.

The VEH hot path uses a fixed, allocation-free, lock-free 1,024-entry profile.
The final attempt copies it and prints every entry in tick order. A synthetic
probe and `scripts/task351_aot_timer_source_attribution.ps1` were added; the
wrapper reuses Task 347 equivalence checks and validates complete source rows,
tick totals, and zero overflow before exporting CSV/JSON.

### Measurement and classification

The runs recorded 1,114/1,123/1,111 frames, 99/94/106 source entries, and
5,658/5,674/5,638 total attributed ticks, with zero overflow. The period
conversion of all delivery contexts had a 23.575-second median, but was not
treated as pacing. Most leading sources were render, memory, or ordinary
calculation back edges without a tick dependency.

The confirmed loop calls `0x0304318F`, compares the returned value with two,
and branches from `0x0303DE89` back to the call. The reader returns
`0x032D9C80`, which the original INT 8 ISR increments at `0x03042F3C`.
Task 348's former `0x0302FA10`/`0x032D9C84` wait did not appear in the
129-source union of this normal route.

Median profile-on frames were 1,114, only 0.89% below Task 347's 1,124 and
inside its 1,112--1,141 range. The derived guest median was 61.08%, 0.36
percentage points from Task 347. Observer impact is therefore small.

### Verification and next work

The synthetic probe reported `aot_timer_source_profile_all=true`; Win32 x86
Release probe and loader builds passed; a ten-second smoke retained 63 sources,
zero overflow, and no stale-ledger over-attribution; and all three 60-second
runs passed the existing semantic invariants at divisor 4,972/240 Hz with zero
malformed, fatal, or Glide issues. Every run's source count and tick sum
exactly matched its profile summary.

Confirmed timer pacing does not dominate the guest axis. The next task should
attribute the remaining 50.89 percentage points by address and phase with a
non-suspending observation method. The 21.73% Glide gate remains the following
axis.
