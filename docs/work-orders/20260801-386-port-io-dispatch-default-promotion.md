# 20260801-386 Port-I/O Dispatch 기본 활성화 작업 지시 / Work Order

## 한국어

1. Music Select 수동 캡처를 이전 guarded segment-read 캡처와 frame 기준으로 비교합니다.
2. `aot-dbt`에서 `REPIU_AOT_DBT_PORT_IO_DISPATCH` 미설정 기본값을 ON으로 승격합니다.
3. `0|off|false` 및 알 수 없는 값이 opt-out으로 남도록 합니다.
4. Release Win32 loader와 전체 AOT probe를 빌드·검증합니다.
5. 격리 EEPROM 스모크로 기본/opt-out 상태와 정상 실행을 확인합니다.
6. 분석 문서와 작업 로그에 측정 및 검증 결과를 반영합니다.

## English

1. Compare the manual Music Select capture with the guarded-segment-read capture on a per-frame basis.
2. Promote the unset `REPIU_AOT_DBT_PORT_IO_DISPATCH` default to ON for `aot-dbt`.
3. Retain `0|off|false` and unknown values as opt-outs.
4. Build and verify the Release Win32 loader and full AOT probe.
5. Use isolated-EEPROM smokes to confirm default/opt-out state and normal execution.
6. Record measurement and verification results in the analysis and work log.
