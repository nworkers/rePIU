# 20260801-384 Segment Read 기본 승격 / Segment Read Default Promotion

## 한국어

### 근거

Task 383의 opt-in Music Select capture는 기준/활성 실행에서 고정 경로 지표가 일치했습니다. `0x030F536C`는 양쪽 모두 5,471회, DOS `AH=3Bh`는 양쪽 모두 580회였습니다. 활성 실행은 54개 guarded read site를 사용했고 `0x030F536A` segment-read boundary를 제거했습니다.

실행 시간이 달라 raw total은 직접 비교하지 않고 frame으로 정규화합니다.

| 지표 | 기준 | 활성 | 변화 |
|---|---:|---:|---:|
| 평균 FPS | 21.64 | 31.00 | +43.25% |
| CPU cycles/frame | 170.39M | 119.03M | -30.14% |
| exceptions/frame | 1,223.41 | 860.05 | -29.70% |
| HLE outcomes/frame | 105.58 | 51.18 | -51.52% |
| segment-store HLE/frame | 32.17 | 1.85 | -94.26% |

캡처 길이 차이 때문에 평균 FPS 증가율 자체는 보수적으로 해석해야 하지만, 동일 고정 경로와 여러 독립적인 frame-normalized 지표가 같은 방향으로 움직이므로 기본 승격 조건은 충족합니다.

### 정책

`aot-dbt`에서 환경 변수가 없으면 guarded segment-read를 활성화합니다. `REPIU_AOT_GUARDED_SEGMENT_READ=0|off|false` 및 알 수 없는 값은 진단과 회귀 bisect를 위한 fail-closed opt-out입니다. 다른 backend는 계속 비활성화합니다. Task 383의 physical/shadow equality guard와 HLE fallback은 변경하지 않습니다.

## English

### Evidence

Task 383's opt-in Music Select capture followed the same fixed path in baseline and enabled runs: `0x030F536C` occurred 5,471 times and DOS `AH=3Bh` occurred 580 times in both. The enabled run used 54 guarded-read sites and removed the `0x030F536A` segment-read boundary.

Because capture durations differ, the comparison uses frame-normalized metrics shown above. The enabled run reduced cycles per frame by 30.14%, exceptions per frame by 29.70%, HLE outcomes per frame by 51.52%, and segment-store HLE per frame by 94.26%. The raw average-FPS gain should be interpreted conservatively due to duration differences, but the matching fixed path and independent normalized metrics satisfy promotion criteria.

### Policy

Enable guarded segment-read for `aot-dbt` when the environment variable is unset. Treat `REPIU_AOT_GUARDED_SEGMENT_READ=0|off|false` and unknown values as fail-closed diagnostic/regression-bisect opt-outs. Other backends remain disabled. Do not change Task 383's physical/shadow equality guard or HLE fallback.
