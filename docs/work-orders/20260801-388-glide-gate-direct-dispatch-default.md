# 20260801-388 Glide Gate 직접 Dispatch 기본 활성화 작업 지시 / Work Order

## 한국어

1. 수동 Music Select 캡처의 실행시간, buffer swap, direct dispatch 성공/실패, 예외율을 검토합니다.
2. `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH` 미설정 기본값을 ON으로 승격합니다.
3. `0|off|false`와 알 수 없는 값은 fail-closed opt-out으로 유지합니다.
4. synthetic probe에 기본값과 opt-out 정책 검증을 추가합니다.
5. Release probe/loader 빌드와 전체 probe를 수행합니다.
6. 환경 변수 미설정 및 `0` smoke를 수행해 direct 경로와 기존 복구 경로를 확인합니다.
7. 아키텍처, 누적 분석, 작업 로그를 갱신하고 작업 단위를 커밋합니다.

## English

1. Review runtime, buffer swaps, direct-dispatch success/failure, and exception rate in the manual Music Select capture.
2. Promote the unset `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH` default to ON.
3. Preserve `0|off|false` and unknown values as fail-closed opt-outs.
4. Add default and opt-out policy checks to the synthetic probe.
5. Build the Release probe/loader and run the full probe suite.
6. Run smokes with the variable unset and set to `0` to verify the direct path and legacy recovery path.
7. Update architecture, cumulative analysis, and the work log, then commit the task unit.
