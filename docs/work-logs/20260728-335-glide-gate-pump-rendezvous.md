# 20260728-335 작업 로그: gate 진입 pump rendezvous 제거 / Work log

설계: [20260728-335-glide-gate-pump-rendezvous.md](../design/20260728-335-glide-gate-pump-rendezvous.md)

작업 지시: [20260728-335-glide-gate-pump-rendezvous.md](../work-orders/20260728-335-glide-gate-pump-rendezvous.md)

## 한국어

### 결론 요약

**gate G1과 G2가 성립하고 G3은 작게 성립했습니다.** gate 진입당 rendezvous가
`1.92 → 0.92`로 떨어지고, Glide gate의 wall-clock 비중은 중앙값
`17.00% → 13.47%`(-3.53%p), 프레임 중앙값은 `1,891 → 1,995`(**+5.5%**),
progress 중앙값은 `118,426 → 121,654`(+2.7%)입니다.

**예측은 빗나갔습니다.** 설계는 약 8.5%p 절감을 예측했지만 실측 중앙값은 3.53%p였습니다.
guest가 빨라지면 gate 호출 자체가 늘어나므로 비중이 예측만큼 떨어지지 않습니다.

### 사전 등록 gate 판정

| gate | 조건 | 관측(중앙값) | 판정 |
|---|---|---:|---|
| **G1** | gate 진입당 rendezvous <= 1.05 | **0.92** | **성립** |
| **G2** | Glide gate 비중 3%p 이상 감소 | **-3.53%p** | **성립(경계)** |
| **G3** | 프레임 증가 | **+5.5%** | **성립(작음)** |
| G4 | G1 성립·G2 기각 | — | 해당 없음 |

### 측정 값 — 표본을 늘려야 했습니다

**단일 표본이었다면 결론이 반대였습니다.** 첫 OFF 실행(프레임 1,611)은 첫 ON
실행(1,597)과 사실상 같았고, 두 번째 ON 실행은 1,891로 OFF보다 높았습니다.
같은 설정의 실행 간 편차가 18%에 달하므로 표본을 늘려 중앙값으로 판정했습니다.

| 설정 | 실행별 프레임 | 중앙값 | 실행별 progress | 중앙값 |
|---|---|---:|---|---:|
| ON | 1,597 / 1,891 / 1,901 / 1,883 | 1,891 | 109,158 / 118,827 / 118,426 / 118,100 | 118,426 |
| OFF | 1,611 / 2,055 / 1,995 | 1,995 | 108,443 / 127,502 / 121,654 | 121,654 |

각 설정의 첫 실행이 가장 느립니다(1,597과 1,611). 워밍업 성격으로 보이나 원인은
확인하지 않았습니다.

| 항목 | ON | OFF |
|---|---:|---:|
| gate 진입 | 79,964 | 67,739 |
| rendezvous | 153,452 | 62,246 |
| **진입당 rendezvous** | **1.92** | **0.92** |
| Glide gate 비중(중앙값) | 17.00% | 13.47% |
| VEH 비중 | 33.51~35.10% | 29.12~31.48% |
| AOT 캐시 내 guest 실행 | 64.90~66.49% | 68.52~70.88% |

**진입당 0.92는 1.00이 아닙니다.** 일부 gate 진입은 backend를 건드리지 않아
rendezvous 없이 끝나기 때문입니다.

**rendezvous 1회당 `work`가 커진 것은 비용 증가가 아닙니다.** `57,558 → 112,143 tick`
으로 보이지만, 이는 작업이 거의 없던 pump rendezvous가 평균에서 빠졌기 때문입니다.
`work` 총량은 `8.83e9 → 6.98e9`로 오히려 줄었습니다.

### 확인됨 / Confirmed

* gate 경로의 `PumpEvents`는 gate당 정확히 1회의 host rendezvous였고 제거했습니다.
* 동등성 유지: 모든 실행에서 60초 정상 timeout, `malformed 0`,
  `original fatal halt reached: false`, `Glide implementation issues 0/0/0/0/0/0`.
* 창 응답과 종료 요청은 host poll loop의 pump로 그대로 동작합니다.

### 미확정 / Unresolved

* **처리량 개선이 비용 절감에 비해 작습니다.** wall-clock 비중은 3.53%p 줄었는데
  프레임은 5.5%만 늘었습니다. 실행이 이제 무엇에 의해 pacing되는지(guest 내부
  타이머 대기인지, `grBufferSwap`의 vsync인지) 측정하지 않았습니다. **다음 Task의
  질문입니다.**
* 실행 간 편차 18%의 원인(첫 실행이 항상 가장 느림).
* VEH residual 11.19%는 여전히 이름이 없습니다.
* handler 축의 중첩(`return` 484.73%)도 그대로입니다.

---

## English

### Summary

G1 and G2 hold and G3 holds weakly: rendezvous per gate entry fell from 1.92 to 0.92, the Glide
gate's median share of wall clock from 17.00% to 13.47% (-3.53 points), the median frame count from
1,891 to 1,995 (+5.5%), and median progress from 118,426 to 121,654 (+2.7%). The design's
prediction of roughly 8.5 points was wrong: as the guest speeds up it issues more gate calls, so the
share does not fall proportionally.

### Sampling had to be widened

A single sample would have inverted the conclusion. The first OFF run reached 1,611 frames against
the first ON run's 1,597, while a second ON run reached 1,891 — above OFF. Run-to-run spread within
one setting is 18%, so the judgement uses medians over four ON and three OFF runs: frames 1,597 /
1,891 / 1,901 / 1,883 against 1,611 / 2,055 / 1,995, and progress 109,158 / 118,827 / 118,426 /
118,100 against 108,443 / 127,502 / 121,654. The first run of each setting is the slowest, which
looks like warm-up but was not investigated.

Per-run structure: 79,964 gate entries and 153,452 rendezvous with the pump against 67,739 and
62,246 without, so 1.92 against 0.92 per entry — not 1.00, because some gate entries never touch
the backend. The VEH fell from 33.5-35.1% to 29.1-31.5% of wall clock while AOT cache execution rose
from 64.9-66.5% to 68.5-70.9%. The mean `work` per rendezvous rose from 57,558 to 112,143 ticks,
which is not a cost increase: the near-zero-work pump rendezvous left the average, and total work
fell from 8.83e9 to 6.98e9.

### Confirmed and unresolved

The gate-path `PumpEvents` was exactly one host rendezvous per gate entry and is gone, with
equivalence holding in every run — normal 60-second timeout, zero malformed dispatch, no fatal
halt, no Glide gap — and window responsiveness and the exit request still served by the host poll
loop's own pump. Unresolved: throughput improved less than the cost removed, so what now paces the
run — a guest-side timer wait or `grBufferSwap` vsync — is unmeasured and is the next task's
question; the 18% run-to-run spread with a consistently slowest first run is unexplained; the
11.19% VEH residual is still unnamed; and the handler axis still overlaps past 100%.
