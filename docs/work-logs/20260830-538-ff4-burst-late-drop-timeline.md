# 20260830-538 FF4 급증과 후반 성능 하락 시간축 작업 로그

## 한국어

### 결과 요약

1초 단위 live profile로 `pumpipx3`의 FF4 급증과 5 fps 절벽의 선후관계를 확인했습니다.
FF4/AOT boundary burst가 먼저 나타나고 다음 보고창에서 지속적인 collapse가 시작되지만,
이 작업은 인과관계를 증명하지 않습니다. `pumpit1`에서는 작은 FF4 변화가 일시적 저하
후 회복으로 끝났습니다.

### 수행 내용

- Task 538 설계 문서와 작업지시서를 먼저 작성했습니다.
- 기존 Linux i386 Release 실행 파일과 guest code를 변경하지 않았습니다.
- `REPIU_LIVE_PROFILE_INTERVAL_MS=1000`으로 두 title을 각각 60초 실행했습니다.
- 누적 `FF4 samples`, `AOT boundary`, `CD` 및 창별 frames, `cycles_per_frame`를 차분했습니다.
- 첫 시도는 WSL `/tmp` 임시 파일이 세션 종료 때 사라져 증거로 사용할 수 없었습니다.
  동일 조건을 다시 실행하여 결과를 Git-ignored `build/task538_*.err`에 보존했습니다.

### 재현 조건

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=1000
REPIU_AOT_FF_TARGET_TIMING=1
```

실행 명령은 다음과 같습니다.

```text
wsl.exe -d Ubuntu-24.04 -- bash -c 'cd /mnt/e/MYWORK/Projects/rePIU && env REPIU_STALL_TIMEOUT_MS=0 REPIU_EXECUTION_TIMEOUT_MS=60000 REPIU_GLIDE_SWAP_INTERVAL=0 REPIU_GLIDE_FRAME_RATE_LOG=1 REPIU_EXECUTION_TIME_PROFILE=1 REPIU_LIVE_PROFILE_INTERVAL_MS=1000 REPIU_AOT_FF_TARGET_TIMING=1 ./build/linux_i386/repiu pumpit1 > /mnt/e/MYWORK/Projects/rePIU/build/task538_pumpit1.err 2>&1'
wsl.exe -d Ubuntu-24.04 -- bash -c 'cd /mnt/e/MYWORK/Projects/rePIU && env REPIU_STALL_TIMEOUT_MS=0 REPIU_EXECUTION_TIMEOUT_MS=60000 REPIU_GLIDE_SWAP_INTERVAL=0 REPIU_GLIDE_FRAME_RATE_LOG=1 REPIU_EXECUTION_TIME_PROFILE=1 REPIU_LIVE_PROFILE_INTERVAL_MS=1000 REPIU_AOT_FF_TARGET_TIMING=1 ./build/linux_i386/repiu pumpipx3 > /mnt/e/MYWORK/Projects/rePIU/build/task538_pumpipx3.err 2>&1'
```

### 측정 결과

| title | final frames/span | final FF4 | final AOT boundary | shutdown |
| --- | ---: | ---: | ---: | --- |
| `pumpit1` | 2,063 / 57,589 ms | 91 | 29,611 | timeout, answered=1, recovered=0, stopped=0 |
| `pumpipx3` | 1,468 / 56,489 ms | 15,809 | 460,278 | timeout, answered=1, recovered=0, stopped=0 |

`pumpipx3`의 핵심 창은 다음과 같습니다.

| window | frames | cycles/frame | FF4 delta | boundary delta | CD delta |
| ---: | ---: | ---: | ---: | ---: | ---: |
| #29 | 30 | 127,014,013 | 0 | 60 | 60 |
| #30 | 52 | 71,508,353 | +777 | +934 | +115 |
| #37 | 48 | 77,453,031 | 0 | +96 | +96 |
| #38 | 42 | 105,428,704 | +12,183 | +13,073 | +261 |
| #39 | 5 | 811,557,496 | 0 | +202 | +12 |
| #40 | 5 | 806,691,182 | 0 | +190 | +10 |

Frame-rate log는 약 38.696초에 34.6 fps, 약 39.779초에 4.6 fps를 기록했으며 이후
약 4.5~4.8 fps가 유지되었습니다. 따라서 #38 burst 뒤 #39에서 collapse가 시작됩니다.

`pumpit1`은 #7에서 FF4 +12, #33에서 +32를 기록했습니다. #33은 19 frames와
262,783,428 cycles/frame이었지만 #34에서 47 frames와 82,093,010 cycles/frame으로
회복되었습니다. 지속적인 5 fps 상태는 관측되지 않았습니다.

### 판정

**확인됨**

- `pumpipx3`의 FF4/AOT burst는 지속적인 5 fps 구간보다 한 보고창 먼저 발생했습니다.
- burst 창의 boundary 증가 13,073회 중 FF4 증가가 12,183회였고 CD 증가는 261회였습니다.
- collapse 직전 새로운 큰 CD 증가나 FF4 추가 증가는 없었습니다.
- `pumpit1`의 FF4 변화는 작고 일시적이며 회복되었습니다.

**추정**

- FF4 burst를 포함한 guest/AOT 상태 전환이 후속 AOT boundary churn을 유발했을 가능성이
  있습니다.

**미확정**

- FF4 target interval 자체가 원인인지 여부
- burst를 발생시킨 guest scene/state와 EDX producer/writer
- burst 이후 `oth` boundary를 반복시키는 정확한 exception/cache 경로

### 산출물

- 설계: `docs/design/20260830-538-ff4-burst-late-drop-timeline.md`
- 작업지시: `docs/work-orders/20260830-538-ff4-burst-late-drop-timeline.md`
- 누적 분석: `docs/analysis/current-execution-frontier.md`,
  `docs/analysis/runtime-aot-dynamic-translation.md`
- 원자료: `build/task538_pumpit1.err`, `build/task538_pumpipx3.err` (Git-ignored)

---

# 20260830-538 Work Log: FF4 Burst and Late-Drop Timeline

## English

### Summary

One-second live profiles established the ordering between the `pumpipx3` FF4 burst and the
5 fps cliff. The FF4/AOT-boundary burst appears first and persistent collapse starts in the
next report window, but this task does not prove causality. Pumpit1's smaller FF4 changes were
followed by recovery rather than a sustained cliff.

### Work performed

- Wrote the Task 538 design and work-order before measurement.
- Kept the existing Linux i386 Release binary and guest code unchanged.
- Ran both titles for 60 seconds with `REPIU_LIVE_PROFILE_INTERVAL_MS=1000`.
- Differenced cumulative FF4 samples, AOT boundaries, CD samples, frames, and cycles/frame.
- The first attempt used WSL `/tmp`, which disappeared when the session ended; it was rerun
  under the same conditions with logs preserved in Git-ignored `build/task538_*.err` files.

### Conditions

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=1000
REPIU_AOT_FF_TARGET_TIMING=1
```

### Measurements

| title | final frames/span | final FF4 | final AOT boundary | shutdown |
| --- | ---: | ---: | ---: | --- |
| `pumpit1` | 2,063 / 57,589 ms | 91 | 29,611 | timeout, answered=1, recovered=0, stopped=0 |
| `pumpipx3` | 1,468 / 56,489 ms | 15,809 | 460,278 | timeout, answered=1, recovered=0, stopped=0 |

Pumpipx3's decisive windows were #38 (`+12,183` FF4, `+13,073` boundaries, `+261` CD,
42 frames, 105,428,704 cycles/frame) and #39 (0 FF4, +202 boundaries, +12 CD, 5 frames,
811,557,496 cycles/frame). The frame-rate log reported 34.6 fps at approximately 38.696
seconds and 4.6 fps at approximately 39.779 seconds, followed by approximately 4.5–4.8 fps.

Pumpit1 recorded +12 FF4 at #7 and +32 at #33. Its #33 transient reached 19 frames and
262,783,428 cycles/frame, then recovered at #34 to 47 frames and 82,093,010 cycles/frame.
It did not enter the sustained 5 fps state.

### Assessment

**Confirmed:** The pumpipx3 FF4/AOT burst precedes the persistent 5 fps state by one report
window; the burst is mostly boundary growth rather than a new CD surge; and pumpit1's small
FF4 changes recover. **Inferred:** a guest/AOT state transition involving the FF4 burst may
lead to the later boundary churn. **Unresolved:** FF4 target-interval causality, the guest
scene/state and EDX producer/writer, and the exact exception/cache path that repeats `oth`
boundaries.
