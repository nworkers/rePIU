# 20260729-352 협력형 AOT back-edge 표본화 작업 로그 / Work log

* 설계: [20260729-352-cooperative-aot-backedge-sampling.md](../design/20260729-352-cooperative-aot-backedge-sampling.md)
* 작업 지시: [20260729-352-cooperative-aot-backedge-sampling.md](../work-orders/20260729-352-cooperative-aot-backedge-sampling.md)
* 최종 측정: `build/benchmarks/aot-backedge-sample-final/20260729-181611/` (Git 제외)
* 동일 바이너리 control: `build/benchmarks/aot-backedge-sample-control/20260729-180545/` (Git 제외)

## 한국어

### 결과

guest thread를 정지하지 않고 임의 시점의 요청을 다음 AOT back edge에서 회수하는
협력형 표본화를 시험했습니다. request 손실, timer 계수 혼입, 긴 rendezvous 지연은
제거했지만, 측정값이 요청 시점의 실행 위치가 아니라 host/guest 복귀 뒤 처음 만나는
eligible back edge의 topology와 호출 빈도를 나타낸다는 사실이 확인됐습니다.

따라서 이 방법은 active guest instruction residency 귀속에 사용할 수 없습니다.
프로토타입 코드, probe, 실행 wrapper는 전부 제거했고 설계·측정·기각 근거만
남겼습니다.

### 프로토타입 반복

첫 구현은 timer와 sample이 같은 request word를 공유하고 host poll마다 sample guard를
재무장했습니다. latency는 짧았지만 timer 소비와 sample 소비 사이에 stale request가
남을 수 있어 요청 비트를 분리했습니다.

poll마다 재무장하지 않도록 줄인 중간 구현은 10초 smoke
`build/benchmarks/aot-backedge-sample-final-smoke/20260729-181314/`에서 arm/hit
364/364를 유지했지만 latency histogram이
`332/0/0/0/0/23/4/5`였고 p95가 9~16ms bucket으로 밀렸습니다. rendezvous가 임의
시점 표본으로 쓰기에 충분히 가깝지 않아 폐기했습니다.

최종 구현은 timer와 sample을 서로 다른 bit로 분리하고 pending 동안 sample bit만
재무장했습니다. request state의 `idle/arming/pending` publication과 guest-side CAS로
부분 게시를 막았고, sample-only trap을 timer safe-point 계수에서 제외했습니다.
고정 1,024-entry profile과 latency histogram을 사용해 VEH hot path의 allocation과
lock도 피했습니다. 이 구현 역시 결론 확인 뒤 제거했습니다.

### 최종 측정

legacy suspend sampler를 끄고 Release direct-loader를 60초씩 세 번 실행했습니다.

| 항목 | 결과 |
|---|---:|
| frames | 중앙값 1,221 (1,154~1,413) |
| arms / hits | 중앙값 2,599 / 2,599 |
| profile overflow | 0 |
| arm-to-hit p95 | 0ms |
| coalesced | 중앙값 12 |
| timer safe-point trap | 약 93.28회/초 |

수치상 S1~S4 gate는 통과했습니다. 그러나 세 실행 모두 1위인
`0x030F5F41`의 평균 표본 비중은 20.19%였고, 원본 코드는 다음과 같습니다.

```text
0x030F5F36  test al, 3
0x030F5F38  jz   0x030F5F43
0x030F5F3A  mov  [eax], dl
0x030F5F3C  inc  eax
0x030F5F3D  ror  edx, 8
0x030F5F40  dec  ecx
0x030F5F41  jnz  0x030F5F36
```

이는 `memset`의 4-byte alignment prefix로 최대 세 번만 반복됩니다. active
instruction residency의 20%를 차지할 수 없으므로 표본 비중을 시간 비중으로 해석할
수 없습니다. 그 뒤 순위는 `0x0305C227` 16.48%, `0x030CE1B1` 10.74%,
`0x0303DE89` 10.65%였지만 같은 이유로 시간 비중으로 사용하지 않았습니다.

timer/sample bit를 분리하기 전에는 `0x0305C227`이 1위였습니다. request를 지우는
시점만 달라져 순위가 바뀐 것도 이 분포가 요청 시점 EIP보다 rendezvous topology에
민감하다는 별도 증거입니다.

### 관찰자 영향과 기각

같은 최종 바이너리에서 profile만 끈 control의 60초 세 번 프레임은 중앙값
1,159(1,156~1,160)였습니다. profile-on 중앙값 1,221은 control보다 5.35% 높아,
추가 rendezvous가 게임 phase를 관찰 가능한 수준으로 움직였습니다.

기존 S1~S4는 요청 보존, 짧은 지연, 처리량, 실행 간 반복성만 확인합니다. 여기에
“상위 source의 bounded work가 표본 비중과 실행 시간 관점에서 양립하는가”라는 정적
타당성 S5가 필요하며 이번 결과는 이를 실패했습니다. 짧은 latency와 안정된 순위만으로
first-backedge 표본을 instruction residency로 바꿀 수 없습니다.

### 검증

프로토타입이 존재할 때 다음 검증을 통과했습니다.

* 합성 probe: enable 정책, jitter 범위, profile 집계·정렬·overflow·latency bucket
* Win32 x86 Release `repiu_aot_probe` 및 `repiu_loader_win32` 빌드
* 10초 smoke의 arm/hit 보존과 sample-only timer 계수 분리
* Release direct-loader 60초 세 번의 기존 semantic invariant
* malformed/fatal/Glide issue 0

기각 뒤 프로토타입 소스와 빌드 연결을 제거했습니다. 최종 저장소는 Task 351 런타임
구현과 동일합니다. 삭제 후 Release loader와 AOT probe를 다시 빌드했고 전체 probe가
exit 0, `aot_timer_source_profile_all=true`로 통과해 복구 상태를 확인했습니다.

### 다음 작업

Task 351의 240Hz pacing 상한은 wall-clock 9.83%이고, 정지 표본과 협력형
first-backedge 표본은 모두 active guest 주소 귀속에 부적합합니다. 원본 guest 계산과
geometry를 다시 구현하지 않는다는 프로젝트 원칙도 유지해야 합니다.

따라서 다음 실행 가능한 HLE 축은 Glide gate입니다. Task 347에서는 21.73%였고 이번
동일 바이너리 control에서는 22.74%였습니다. guest active 축을 다시 조사하려면
실행된 back edge 자체의 정확한 counter/instruction-count 계측이나 외부 PMU처럼
first-eligible topology를 표본화하지 않는 방법이 먼저 필요합니다.

---

## English

### Result

The prototype posted independently timed requests without suspending the guest
thread and collected each request at the next AOT back edge. It eliminated
request loss, timer-account contamination, and long rendezvous latency, but
proved to measure the topology and call frequency of the first eligible back
edge after host/guest return rather than the execution location at request
time.

It is therefore invalid for active guest instruction-residency attribution.
All prototype code, probes, and wrappers were removed; only the design,
measurements, and rejection evidence remain.

### Prototype iterations

The first implementation shared one request word with timer work and rearmed
the sample guard every host poll. Latency was short, but timer and sample
consumption could leave a stale request, so the bits were separated.

An intermediate reduced-rearm version retained 364/364 arms/hits in its
ten-second smoke, but its latency buckets were `332/0/0/0/0/23/4/5`, putting
p95 in the 9--16 ms bucket. The final version separated timer and sample bits,
rearmed only the sample bit while pending, used an
`idle/arming/pending` publication handshake, excluded sample-only traps from
timer accounting, and kept a fixed 1,024-entry allocation-free profile. This
implementation was also removed after establishing the conclusion.

### Final measurement

Three 60-second Release runs with the legacy suspend sampler disabled produced
median 1,221 frames (1,154--1,413), 2,599/2,599 arms/hits, zero overflow, 0 ms
p95 arm-to-hit latency, median 12 coalesced requests, and about 93.28 timer
safe-point traps per second.

Although the numeric S1--S4 gates passed, `0x030F5F41` ranked first in all
runs at a mean 20.19% sample share. It is the back edge of a `memset` 4-byte
alignment prefix that repeats at most three times, so it cannot plausibly own
20% of active instruction residency. Later ranks (`0x0305C227` 16.48%,
`0x030CE1B1` 10.74%, and `0x0303DE89` 10.65%) were not treated as time shares
either. Before timer/sample bit separation, `0x0305C227` ranked first, further
showing that request-clear timing changes the topology distribution.

### Observer impact and rejection

The same final binary with the profile disabled produced median 1,159 frames
(1,156--1,160). The profile-on median was 5.35% higher, so the added
rendezvous measurably shifted game phase.

S1--S4 cover request preservation, latency, throughput, and repeatability.
They need a fifth static plausibility gate: bounded work at a leading source
must be compatible with its claimed execution-time share. This result fails
that gate. Short latency and stable rankings cannot turn a first-backedge
sample into instruction residency.

The prototype passed its synthetic policy/jitter/aggregation/ranking/
overflow/latency checks, Win32 x86 Release builds, ten-second handshake and
timer-separation smoke, and three-run semantic invariants before removal.
After removal, the final Task 351 runtime source was rebuilt successfully;
the full probe exited zero and reported `aot_timer_source_profile_all=true`.

### Next work

Task 351 bounded 240 Hz pacing at 9.83% of wall time, while both suspension
sampling and cooperative first-backedge sampling are invalid for active guest
address attribution. Reimplementing original guest calculation or geometry
would also violate the project direction.

The next actionable HLE axis is therefore the Glide gate: 21.73% in Task 347
and 22.74% in this same-binary control. Any future guest-active investigation
must use exact executed-backedge/instruction-count instrumentation or an
external PMU-class method that does not sample first-eligible topology.
