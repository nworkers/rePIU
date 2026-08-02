# 20260802-403 JAMMA 입력 스냅샷 작업 로그 / JAMMA Input Snapshot Work Log

## 한국어

### 작업 요약

Task 402가 지목한 port I/O 비용의 근인이 `GetAsyncKeyState`임을 계측으로 확정하고,
입력 스냅샷을 도입해 호출을 **14.2배** 줄였습니다. 비용 제거는 확정됐지만
**프레임 개선은 확정하지 못했습니다.**

### 1. 분해 계측 결과

| 항목 | 값 |
|---|---:|
| port I/O 핸들러 본체 | wall의 30.90% |
| └ JAMMA scan 루프 | port-io 본체의 **99.21%** |
| scan당 key query | 8.98 |
| key query당 cycle | 3,044 |
| `9 × 3,044` vs 실측 27,337 | **100.2%** |

디코드·`RecordPortIo`·문자열 생성은 측정 오차 안입니다. **근인은 `GetAsyncKeyState`
하나입니다.**

### 2. 구현

`ReadJammaPort8`을 둘로 나눴습니다.

- `ScanJammaPort8(port)`: 실제 `GetAsyncKeyState` 조회 (기존 로직 그대로)
- `ReadJammaPort8(port)`: `QueryPerformanceCounter`로 경과를 보고 임계값 이상일 때만
  JAMMA 창(`0x02A0`~`0x02AF`) 전체를 갱신, 그 외에는 스냅샷 바이트 반환

`REPIU_JAMMA_SNAPSHOT`(기본 on, `0`이면 매 읽기 조회), `REPIU_JAMMA_SNAPSHOT_US`(기본 500).

Task 327 제약은 그대로입니다. 게스트 `IN`은 매번 트랩하고 EIP도 매번 전진하며 NOP 패치도
없습니다. 바뀐 것은 트랩 **안에서** 호스트 키보드를 전수 조회하는지 여부뿐입니다.
EEPROM·사운드 포트는 스냅샷 경로 앞에서 분기하므로 영향받지 않습니다.

### 3. A/B 결과 (pumpit3, 45초, 각 6회, EEPROM 격리)

| | 렌더 도달 | 프레임(정렬) | 렌더런 중앙값 | port-io 비중 중앙값 | key query/s 중앙값 |
|---|---|---|---:|---:|---:|
| off | 4/6 | 0, 0, 867, 867, 867, 1106 | 867 | 22.40% | 285,425 |
| **on** | 3/6 | 0, 0, 0, 867, 1340, 1389 | **1340** | **2.75%** | **20,080** |

**확정된 것: 비용은 제거됐습니다.** key query는 **14.2배** 줄었고 port-io wall 비중은
22.40% → 2.75%입니다. 설계가 의도한 그대로입니다.

**확정되지 않은 것: 프레임 개선.** 렌더링에 도달한 실행만 보면 중앙값 867 → 1,340이지만,
근거로 삼기에는 약합니다.

- 표본이 각 6회이고 결과가 0 아니면 약 1,300으로 **양극화**되어 있습니다.
- **867이라는 값이 off 4회 중 3회와 on 1회에 정확히 같은 값으로 반복**됩니다. 이는
  처리량 상한이 아니라 **특정 장면 경계**의 프레임 수로 보입니다. 그렇다면 프레임 수는
  throughput 지표가 아니라 "45초 안에 어느 장면까지 갔는가" 지표입니다.
- Task 365가 "비용을 줄여도 프레임이 늘지 않았다"를 이미 확인한 전례가 있습니다.

### 4. 새로 드러난 문제: 렌더링 도달이 비결정적

**두 arm 모두에서 6회 중 2~3회가 45초 안에 렌더링에 도달하지 못했습니다**(off 2회,
on 3회). 스냅샷과 무관한 기존 문제입니다. 도달하지 못한 실행도 크래시 없이 정상
timeout했고 `progress`는 계속 증가했습니다.

이것이 해결되기 전에는 프레임 기반 성능 판정을 신뢰할 수 없습니다.

### 5. 측정 절차에서 배운 것 (중요)

**첫 A/B는 무효였습니다.** `eeprom.dat`가 git에 추적되지 않는 영속 상태인데 격리하지
않고 6회를 연속 실행했습니다. 기존 `scripts/benchmark_*.ps1`은 이미 매 실행마다 고정
fixture를 복사해 `REPIU_EEPROM_PATH`로 격리하고 있었는데, 그 관행을 따르지 않았습니다.
격리 후 재실행에서야 신호가 나왔습니다.

### 6. 회귀 확인

| 타겟 | 프레임 | key query/s | 종료 |
|---|---:|---:|---|
| pumpit1 | 2,251 (이전 2,222) | 1,712 | 정상 timeout |
| pumpit2 | 2,204 (이전 1,985) | 2,377 | 정상 timeout |

회귀 없습니다.

### 7. 미확정

- **프레임 개선 여부.** 위 4번의 비결정성을 먼저 해소하고, 프레임이 아니라 장면 진행도
  같은 지표로 다시 재야 합니다.
- **입력 정확도의 실측 검증.** 정확도 논증은 분석적입니다(게스트 폴링 4.8ms 대 스냅샷
  0.5ms). 실제 키 입력으로 press/release가 게스트에 그대로 전달되는지는 사람이 플레이해
  확인해야 합니다.
- 갱신 주기 500µs는 게스트 폴링의 1/10로 잡은 보수적 값이며 최적화하지 않았습니다.

---

## English

### Summary

Confirmed by measurement that `GetAsyncKeyState` is the root of the port I/O cost Task 402
identified, and introduced an input snapshot that cuts the calls **14.2x**. The cost removal
is established; **the frame improvement is not.**

### 1. Decomposition

The JAMMA scan loop is **99.21%** of the port I/O handler body, at 8.98 key queries per scan
and 3,044 cycles per query. `9 × 3,044 = 27,396` against a measured 27,337 per scan —
**100.2%** — leaving decode, `RecordPortIo`, and string construction inside measurement noise.
`GetAsyncKeyState` is the entire cost.

### 2. Implementation

`ReadJammaPort8` split into `ScanJammaPort8` (the real query, logic unchanged) and a snapshot
lookup that refreshes the whole JAMMA window (`0x02A0`-`0x02AF`) only when
`QueryPerformanceCounter` shows the threshold has elapsed. `REPIU_JAMMA_SNAPSHOT` (default on,
`0` queries every read) and `REPIU_JAMMA_SNAPSHOT_US` (default 500).

The Task 327 constraint is untouched: the guest `IN` still traps, EIP still advances, and
nothing is NOP-patched — only the host keyboard rescan *inside* the trap is bounded. EEPROM and
sound ports branch before the snapshot path and are unaffected.

### 3. A/B (pumpit3, 45 s, six runs per arm, isolated EEPROM)

| | Reached rendering | Frames (sorted) | Median of rendering runs | Port-io share median | Key queries/s median |
|---|---|---|---:|---:|---:|
| off | 4/6 | 0, 0, 867, 867, 867, 1106 | 867 | 22.40% | 285,425 |
| **on** | 3/6 | 0, 0, 0, 867, 1340, 1389 | **1340** | **2.75%** | **20,080** |

**Established: the cost is gone.** Key queries fell **14.2x** and the port-io wall share went
from 22.40% to 2.75%, exactly as designed.

**Not established: a frame improvement.** Among runs that rendered the median moves 867 to
1,340, but that is weak evidence: six runs per arm, outcomes polarised at 0 or ~1,300, and the
value **867 recurs exactly** in three of four off runs and one on run — which looks like a
scene boundary rather than a throughput ceiling, making frames a "how far did it get in 45 s"
measure rather than throughput. Task 365 already found one case where removing cost did not
add frames.

### 4. Newly surfaced problem: reaching rendering is nondeterministic

**In both arms, two to three of six runs never reached rendering within 45 seconds** (off two,
on three). This is independent of the snapshot. Those runs still timed out cleanly with
`progress` advancing. Until it is understood, frame-based performance judgements cannot be
trusted.

### 5. Process lesson

**The first A/B was invalid.** `eeprom.dat` is untracked persistent state and I ran six runs
back to back without isolating it, even though `scripts/benchmark_*.ps1` already copy a fixed
fixture per run and point `REPIU_EEPROM_PATH` at it. Only the re-run with isolation produced a
usable signal.

### 6. Regression check

pumpit1 rendered 2,251 frames (2,222 before) and pumpit2 2,204 (1,985 before), both ending in a
clean timeout. No regression.

### 7. Unresolved

- **Whether frames improve.** The nondeterminism in section 4 must be resolved first, and the
  metric should be scene progress rather than raw frame count.
- **Empirical input-accuracy check.** The accuracy argument is analytic (4.8 ms guest poll
  versus 0.5 ms snapshot); whether press/release still reach the guest needs a human playing.
- The 500 µs interval was chosen conservatively as a tenth of the guest poll period and was not
  tuned.
