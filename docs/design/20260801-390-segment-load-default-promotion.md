# 20260801-390 Segment Load 기본 승격 / Segment Load Default Promotion

## 한국어

### 근거

Task 389의 opt-in Music Select 캡처를 Task 388의 같은 수동 경로 캡처와 비교합니다. 기준/활성 실행의 `_GRBUFFERSWAP@4` 횟수는 각각 3,957/3,914로 1.1% 이내이며, 둘 다 SDL 종료 요청으로 끝났고 Glide direct dispatch miss/terminal은 0입니다.

| frame 정규화 지표 | 기준 | 활성 | 변화 |
|---|---:|---:|---:|
| 전체 예외/frame | 46.9366 | 35.2752 | -24.85% |
| breakpoint/frame | 19.6581 | 10.8623 | -44.74% |
| AOT boundary/frame | 16.4109 | 8.0738 | -50.80% |
| effective `8E`/frame | 7.5054 | 0.4854 | -93.53% |

guarded segment-load는 24,102회 성공하고 1,617회 기존 INT3/VEH 경로로 안전 복귀했습니다(성공 93.71%, 복귀 6.29%). 장시간 실행에서 terminal failure 없이 반복 mismatch가 기존 의미 경로에 위임되었으므로 기본 승격 조건을 충족합니다.

### 정책

`aot-dbt`에서 `REPIU_AOT_GUARDED_SEGMENT_LOAD`가 없으면 guarded segment-load를 활성화합니다. `0|off|false` 및 알 수 없는 값은 호환성 진단과 회귀 bisect를 위한 fail-closed opt-out입니다. 다른 backend는 계속 비활성화하며, Task 389의 physical/source/shadow equality guard와 fallback은 변경하지 않습니다.

이 정책은 PIU 주소를 식별하지 않고 지원되는 `MOV Sreg,r16` 형식에 적용됩니다. 다만 실전 기본 승격 근거는 현재 PIU 캡처이므로 다른 guest에서 mismatch 비율과 fallback 동작을 계속 관찰합니다.

## English

### Evidence

Compare Task 389's opt-in Music Select capture with Task 388's matching manual-path capture. `_GRBUFFERSWAP@4` counts were 3,957 and 3,914, within 1.1%; both ended by SDL exit request with zero Glide direct-dispatch misses or terminal failures.

The frame-normalized results are shown above: total exceptions fell 24.85%, breakpoints 44.74%, AOT boundaries 50.80%, and effective `8E` boundaries 93.53%.

The guarded segment-load path succeeded 24,102 times and safely returned to the existing INT3/VEH path 1,617 times (93.71% success, 6.29% fallback). Repeated mismatches delegated to the established semantic path without a terminal failure, satisfying the default-promotion criteria.

### Policy

Enable guarded segment-load for `aot-dbt` when `REPIU_AOT_GUARDED_SEGMENT_LOAD` is unset. Treat `0|off|false` and unknown values as fail-closed compatibility-diagnostic and regression-bisect opt-outs. Other backends remain disabled. Do not change Task 389's physical/source/shadow equality guard or fallback.

The policy recognizes supported `MOV Sreg,r16` forms rather than PIU addresses. Its live default-promotion evidence is currently PIU-only, so mismatch ratios and fallback behavior remain observable for other guests.
