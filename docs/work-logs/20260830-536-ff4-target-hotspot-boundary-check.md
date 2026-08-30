# 20260830-536 FF /4 target hotspot timing 경계 확인 작업 로그

## 작업 목적

Task 535에서 확인한 resolved FF /4 target 주소가 기존 `SingleStepHotspotProfile` timing
경계에 나타나는지 확인했습니다. 이번 작업은 코드 및 guest 실행 경로를 변경하지 않는
관측 범위 판정입니다.

## 실행

Task 535와 동일한 runtime 조건에 다음 두 설정을 추가했습니다.

```text
REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1
REPIU_SINGLE_STEP_HOTSPOT_DUMP=/mnt/c/Users/nworkers/AppData/Local/Temp/repiu_task536_<title>_hotspot.txt
```

각 title 실행은 60초 timeout을 사용했습니다. dump는 teardown 중 생성되었고, 두 실행 모두
process exit code는 1이었지만 dump와 live FF 자료는 남았습니다.

## 측정 결과

| title | dump summary | FF site entry | target candidate entries |
| --- | --- | --- | --- |
| `pumpipx3` | `381761 samples`, `114 distinct`, `4251287558 cycles` | `0x010EF6DE`: `3600`, `11281020`, max `312909` | `0x010EF6E6` absent; `0x010EF8E9` absent |
| `pumpit1` | `26363 samples`, `106 distinct`, `8945029040 cycles` | `0x010F1DD7`: `90`, `1885705`, max `74000` | `0x010F1CFD` absent; `0x010F1CF4` absent |

FF live data는 두 실행 모두 target resolved 상태였습니다. `pumpipx3`의 FF live sample #5는
`resolved=3628`, site `0x010EF6DE:3600`, `tx=27`, `ts=27`, `to=0`이었고, `pumpit1`은
`resolved=90`, site `0x010F1DD7:90`, `tx=13`, `ts=13`, `to=0`이었습니다.

## 경계 판정

기존 hotspot dump에서 FF site는 관측되지만 resolved target guest 주소는 관측되지 않았습니다.
따라서 site entry의 cycle 값은 `HandleSingleStepTrace` 전체 handler window이며, target
instruction 자체의 pure cycle로 사용할 수 없습니다. Dispatcher 코드 순서상 FF 실행 후
target은 `HandleAotReentry`에서 먼저 AOT cache target으로 재진입하고, 별도
`HandleSingleStepTrace` hotspot scope는 target 주소에서 열리지 않는 동작과 일치합니다.

두 실행의 shutdown은 모두 `reason=timeout`, `answered=1`, `recovered=0`, `stopped=0`,
`failure=0`이었으며, 각각 `frames=1361`, `span_ms=56061` 및 `frames=1680`,
`span_ms=57227`이었습니다. 이는 dump/cleanup 한계이며 target resolution 실패가 아닙니다.

## 결론과 다음 작업

기존 계측만으로는 resolved target cycle 비용을 측정할 수 없습니다. 다음 작업은 원본 guest
instruction을 수정하지 않고 AOT address map의 target entry와 다음 boundary 사이를 연결하는
전용 timing boundary를 설계해야 합니다. Index별 target 비용, 실제 AOT target 실행 비용,
late-drop causality는 그 이후에도 별도 검증 대상으로 유지합니다.

## English summary

Task 536 enabled the existing full single-step hotspot dump without changing code or guest
execution. Both title dumps were created. The dump recorded the FF sites but none of the four
resolved target candidates: pumpipx3 `0x010EF6E6`/`0x010EF8E9` and pumpit1
`0x010F1CFD`/`0x010F1CF4`.

Pumpipx3's site entry was 3,600 samples and 11,281,020 cycles; pumpit1's was 90 samples and
1,885,705 cycles. These are `HandleSingleStepTrace` handler-window values, not pure target
cycles. Both runs timed out with `recovered=0, stopped=0`, but FF attribution and dumps were valid.
The existing boundary is unsuitable for target-cost measurement; a dedicated AOT target timing
boundary remains required.
