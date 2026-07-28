# 20260728-334 작업 지시: AOT reentry 핸들러 재분해 / Work order

설계: [20260728-334-aot-reentry-decomposition.md](../design/20260728-334-aot-reentry-decomposition.md)

## 한국어

### 목표

Release 전체 wall-clock의 46.26%인 `HandleAotReentry`에서 function 축이 설명하지 못하는
약 92%를 여섯 구간으로 귀속하고, gate G1이 성립하면 같은 작업에서 색인을 도입한다.

### 범위

**포함**

* `ExecutionTimeBucket`에 reentry 하위 6개 추가와 `HandleAotReentry` 내부 계측.
* 로더 종료 summary에 reentry 축 cycles/count/share와 residual 출력.
* G1 성립 시: cache→guest 색인 도입, 차등 검증 probe, 60초 A/B.

**제외**

* 다른 handler(`kAotReturn` 등) 재분해.
* boundary 진단 자체의 제거(G2 성립 시 별도 Task).

### 구현 지침

* 여섯 구간은 서로 배타적이어야 하며 residual을 감추지 않는다.
* 새 환경변수를 만들지 않는다. `REPIU_EXECUTION_TIME_PROFILE`을 쓴다.
* 색인은 Task 324와 같은 정책을 따른다. 색인이 없거나 낡으면 기존 선형 탐색으로
  degrade하며 **의미는 동일**해야 한다.
* probe는 기존 구현을 oracle로 두는 차등 검증이며 통과 조건은 결정론적 사실만 쓴다.

### 검증 절차

1. Debug/Release 전체 빌드 통과.
2. `repiu_aot_probe` 두 구성 exit 0.
3. Release 60초 `pumpit1` 실행으로 gate 판정.
4. G1 성립 시 색인 A/B: progress·프레임 개선, malformed 0, fatal 0, Glide 공백 0.

---

## English

### Goal

Attribute the roughly 92% of `HandleAotReentry` — itself 46.26% of Release wall clock — that the
function axis does not explain, across six intervals, and introduce an index in the same task if
gate G1 holds.

### Scope

In scope: six reentry sub-buckets in `ExecutionTimeBucket` with instrumentation inside
`HandleAotReentry`, a reentry axis in the loader's exit summary with cycles, counts, shares, and a
residual, and — if G1 holds — a cache-to-guest index with a differential probe and a 60-second A/B.
Out of scope: decomposing other handlers such as `kAotReturn`, and removing the boundary diagnostic
itself, which is a separate task if G2 holds.

### Implementation notes

The six intervals must be mutually exclusive and must not hide the residual. No new environment
variable: `REPIU_EXECUTION_TIME_PROFILE` is reused. Any index follows Task 324's policy, degrading
to the existing linear scan when absent or stale while returning identical results. The probe is a
differential check against the existing implementation as oracle, with deterministic pass
conditions only.

### Verification

Full Debug and Release builds, `repiu_aot_probe` exiting 0 in both configurations, a 60-second
Release `pumpit1` run to judge the gates, and — if G1 holds — an A/B showing improved progress and
frames with malformed, fatal, and Glide-gap counts at zero.
