# 20260728-337 작업 로그: 예외 census / Work log: Exception census

작업 지시: [20260728-337-exception-census.md](../work-orders/20260728-337-exception-census.md)

## 한국어

### 결론 요약

**예외의 79.24%는 TF single-step이고 19.59%가 `INT3`입니다.** 그런데 더 중요한 것은
그 single-step들이 **HLE 지점마다 1회씩 나는 것이 아니라는 사실**입니다. 구간 길이가
**이봉분포(bimodal)** 이고 긴 꼬리가 있습니다.

따라서 "HLE를 예외 없이 만든다"(Task 308의 slice)는 **지배 인구를 겨냥하지
않습니다.** 그것이 겨냥하는 1-step 구간은 전체 single-step의 약 12%뿐입니다.

### 배타 census (Release 60초)

| 종류 | 횟수 | 비중 |
|---|---:|---:|
| **TF single-step** | 735,886 | **79.24%** |
| `INT3` breakpoint | 181,947 | 19.59% |
| access violation | 10,881 | 1.17% |
| 그 외 | 1 | 0.00% |
| **합계** | **928,715** | 100% |

**확인됨: 합계 928,715는 같은 실행의 VEH 진입 횟수 928,715와 정확히 일치합니다.**
배타성이 구조적으로 성립합니다.

전이 가격(Task 336)을 곱하면 이 실행의 커널 전이 비용은
`735,886 × 37,885 + 181,947 × 34,521 + 10,881 × 34,521 ≈ 34.5e9 tick`,
전체의 약 **21.2%** 입니다. (Task 336이 인용한 27.7~30.4%는 progress 143,818인 더
빠른 실행 기준입니다. **예외 비용의 비중은 실행 속도에 비례해 움직입니다.**)

### 연속 single-step 구간 길이 — 이봉분포

경계와 경계 사이에 TF로 걷는 명령 수입니다. 구간 160,489개, 평균 4, 최대 337.

| 길이 | 구간 수 | 구간 비중 | 소비한 single-step(추정) | step 비중 |
|---|---:|---:|---:|---:|
| **1** | 91,580 | 57.1% | 91,580 | 12.4% |
| 2 | 1,954 | 1.2% | 3,908 | 0.5% |
| 3 | 140 | 0.1% | 420 | 0.1% |
| 4 | 143 | 0.1% | 572 | 0.1% |
| **5–8** | **61,528** | **38.3%** | 약 400,000 | **약 54%** |
| 9–16 | 2,833 | 1.8% | 약 34,000 | 4.6% |
| 17–32 | 289 | 0.2% | 약 7,000 | 1.0% |
| **33+** | **2,022** | 1.3% | 약 198,000 | **약 27%** |

**확인됨:** 구간 수로는 1-step이 다수(57.1%)지만, **비용을 내는 것은 5–8 구간(약
54%)과 33+ 꼬리(약 27%)** 입니다. 33+ 구간은 2,022개뿐인데 평균 약 98개를 걷습니다.

### single-step이 무엇을 하는가 (hotspot profile ON인 별도 실행)

| 결과 | 횟수 | 비중 | cycle 비중 |
|---|---:|---:|---:|
| `HLE` | 206,354 | 21.9% | 66.8% |
| `native` | 375,881 | 39.9% | 24.1% |
| `TF` (아무 핸들러도 걸리지 않음) | 359,518 | 38.2% | 8.7% |
| `timer` | 407 | 0.0% | 0.3% |

**확인됨:** single-step의 38.2%는 **어떤 핸들러도 걸리지 않고 그냥 한 명령을 걸어간
것**입니다. HLE 때문이 아니라 **번역된 코드로 돌아가지 못해서** 걷습니다.

같은 실행에서 동적 번역은 60초에 **240회**뿐이고 `posthle=0/0`입니다. 즉 HLE 이후
연속 코드를 번역하는 opt-in(`REPIU_AOT_DBT_POST_HLE_TRANSLATE`)은 꺼져 있습니다.

### 그래서 다음 대상이 정해집니다

* Task 308의 exception-free HLE slice는 **1-step 구간(single-step의 약 12%)** 을
  겨냥합니다. 지배 인구가 아닙니다.
* 지배 인구인 5–8 구간과 33+ 꼬리는 **번역 커버리지 문제**입니다. guest가 캐시 밖
  코드를 TF로 걷고 있습니다.
* 기존 opt-in 두 개가 각각을 겨냥합니다:
  `REPIU_AOT_DBT_POST_HLE_TRANSLATE`(커버리지)와 `REPIU_AOT_DBT_SUPERBLOCK`(HLE).
  둘 다 Release에서 재판정한 적이 없습니다.

### 검증 결과

1. Release 전체 빌드 통과.
2. census 합계 = VEH 진입 횟수(928,715) — 배타성 확인.
3. 60초 정상 timeout, malformed 0, fatal 0, Glide 공백 0.

### 확인됨 / Confirmed

* 예외 구성: TF 79.24% / `INT3` 19.59% / AV 1.17%, 배타적으로 확인.
* 구간 길이는 이봉분포이며 비용의 다수는 5–8과 33+ 구간입니다.
* single-step의 38.2%는 핸들러가 걸리지 않는 순수 walk입니다.

### 미확정 / Unresolved

* 5–8 구간이 **무엇인지** 이름 붙이지 않았습니다. EIP 단위 hotspot(이미 존재)으로
  다음에 확인합니다.
* AV 10,881회의 정체(대부분 guest code write watch로 추정)도 세지 않았습니다.
* census는 상시 계측이지만 그 자체 비용은 따로 재지 않았습니다.

---

## English

### Summary

Exceptions are 79.24% TF single-steps and 19.59% `INT3`, but the more consequential finding is that
those single-steps are not one per HLE site. The run-length distribution is bimodal with a heavy
tail, so Task 308's exception-free HLE slice does not target the dominant population: the one-step
runs it addresses are only about 12% of single-steps.

### The exclusive census

Over 60 seconds in Release: 735,886 single-steps (79.24%), 181,947 breakpoints (19.59%), 10,881
access violations (1.17%), and one other. The total of 928,715 equals the run's VEH entry count
exactly, so exclusivity is structural. Priced with Task 336's per-transition figures that is about
`34.5e9` ticks, roughly 21.2% of this run's wall clock; Task 336 quoted 27.7-30.4% from a faster
run at progress 143,818, which is the same phenomenon it described — the share of a fixed cost
tracks how fast everything else is.

### Run lengths are bimodal

Across 160,489 runs, mean 4 and max 337: 91,580 runs of one step (57.1% of runs but only 12.4% of
steps), 61,528 runs of five to eight (38.3% of runs and about 54% of steps), 2,833 of nine to
sixteen, 289 of seventeen to thirty-two, and 2,022 of thirty-three or more which average about 98
steps each and carry about 27% of all steps. Counting runs says one-step dominates; counting cost
says the five-to-eight mode and the long tail do.

### What the single-steps do

With the hotspot profile enabled in a separate run, outcomes split into HLE at 21.9% of steps and
66.8% of cycles, native at 39.9% and 24.1%, plain TF continuation at 38.2% and 8.7%, and timer at
0.0%. So 38.2% of single-steps had no handler fire at all: the guest is walking because it cannot
get back into translated code, not because of HLE. The same run performed only 240 dynamic
translations in 60 seconds with `posthle=0/0`, meaning the opt-in that translates the continuation
after an HLE site is off.

### Which fixes the target

Task 308's exception-free HLE targets the one-step runs, about 12% of single-steps, so it is not
the dominant population. The five-to-eight mode and the long tail are a translation-coverage
problem — the guest walking code the cache does not contain. Two existing opt-ins address them
separately, `REPIU_AOT_DBT_POST_HLE_TRANSLATE` for coverage and `REPIU_AOT_DBT_SUPERBLOCK` for HLE,
and neither has been judged in Release.

### Verification and unresolved

The Release build passed, the census total equals the VEH entry count, and the run reached its
60-second timeout with zero malformed dispatch, no fatal halt, and no Glide gap. Unresolved: what
the five-to-eight runs actually are, which the existing per-EIP hotspot profile can name; what the
10,881 access violations are, presumed guest code write watches; and the census's own cost, which
was not measured separately.
