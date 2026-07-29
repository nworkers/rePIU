# 20260729-352 협력형 AOT back-edge 표본화 작업 지시 / Work order

## 한국어

### 목표

Task 351 뒤 남은 active/unresolved guest 50.89%p의 주소·phase anchor를 guest thread
정지 없이 관측합니다. 독립적으로 jitter된 host 요청을 기존 AOT back-edge guard로
전달하고 guest VEH가 source와 arm-to-hit latency를 기록합니다.

### 구현

1. 고정 1,024-entry `aot_backedge_sample_profile` 공용 정책/집계 모듈을 추가합니다.
2. `ThreadContext`에 idle/arming/pending handshake, arm timestamp/count,
   coalesced count를 추가합니다.
3. host poll loop에 1초 warm-up과 7~23ms xorshift jitter scheduler를 추가합니다.
4. 기존 safe-point handler가 sample 요청을 소비하고 정확한 guest source를 기록하되
   sample-only trap을 timer 계수에서 제외합니다.
5. execution attempt와 loader summary에 전체 profile을 출력합니다.
6. 합성 probe와 Release 3회 측정 wrapper를 추가합니다.

### 완료 조건

- 설계의 S1~S4 gate와 해석 한계를 코드·측정 문서가 같은 의미로 표현합니다.
- 합성 probe가 정책, jitter, 집계, 정렬, overflow, latency bucket을 통과합니다.
- Win32 x86 Release loader와 AOT probe가 빌드됩니다.
- 10초 smoke에서 요청 손실과 timer 계수 오염이 없습니다.
- Release 60초 3회가 legacy suspend sampler 없이 기존 semantic invariant를 통과합니다.
- 상위 source를 원본 디스어셈블리로 phase 분류하고 observer impact와 안정성을
  분석 문서에 반영합니다.
- 아키텍처, 분석, 작업 로그를 갱신하고 하나의 Git 커밋으로 마무리합니다.

### 종료 판정

프로토타입은 요청/지연 gate를 통과했지만 bounded `memset` alignment loop를 세 실행
모두 1위로 표본화해 topology 편향이 확인됐습니다. active hotspot 계측으로 기각하며
prototype 코드는 유지하지 않습니다. 실패 증거와 후속 우선순위만 문서화합니다.

---

## English

### Objective and implementation

Observe address/phase anchors inside the remaining 50.89 percentage points of
active/unresolved guest execution without suspending the guest thread. Post
independently jittered host requests through the existing AOT back-edge guard,
then let guest VEH record the exact source and arm-to-hit latency.

Add a fixed 1,024-entry profile, idle/arming/pending handshake and counters,
a one-second warm-up plus 7--23 ms xorshift scheduler, sample handling that
does not contaminate timer accounting, final attempt reporting, a synthetic
probe, and a three-run Release wrapper.

### Completion

The implementation must preserve the S1--S4 gates and interpretation limits,
pass synthetic policy/jitter/aggregation/ranking/overflow/latency coverage,
build Win32 x86 Release targets, retain loss-free and timer-clean behavior in
a ten-second smoke, and complete three semantically equivalent 60-second runs
with the legacy suspend sampler disabled. Classify leading original sources,
update architecture/analysis/work-log documents, and finish with one Git
commit.

### Final disposition

The prototype passed request and latency gates but ranked a bounded `memset`
alignment loop first in every run, confirming topology bias. It is rejected
for active-hotspot attribution and no prototype code is retained. Preserve
only the failure evidence and the resulting next priority.
