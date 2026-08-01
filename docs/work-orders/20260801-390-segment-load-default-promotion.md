# 20260801-390 작업 지시: Segment Load 기본 승격 / Work Order: Segment Load Default Promotion

설계: [20260801-390-segment-load-default-promotion.md](../design/20260801-390-segment-load-default-promotion.md)

## 한국어

1. Task 388/389 Music Select 캡처를 frame 기준으로 정규화해 분석 문서에 기록합니다.
2. `aot-dbt`에서 guarded segment-load의 미지정 기본값을 ON으로 변경합니다.
3. `0|off|false` 및 알 수 없는 값은 opt-out으로 유지하고 다른 backend는 비활성화합니다.
4. Release Win32 loader와 두 PIU 실행 파일 구성의 전체 AOT probe를 검증합니다.
5. 기본값 실행과 명시적 opt-out 실행에서 enable 상태와 정상 종료를 확인합니다.
6. 아키텍처, 작업 로그를 갱신하고 작업 단위 커밋을 남깁니다.

## English

1. Record a frame-normalized analysis of the Task 388/389 Music Select captures.
2. Change the unset guarded segment-load default to ON for `aot-dbt`.
3. Keep `0|off|false` and unknown values as opt-outs; keep other backends disabled.
4. Verify the Release Win32 loader and full AOT probes for both PIU executable layouts.
5. Confirm enablement and clean termination in default and explicit-opt-out runs.
6. Update architecture and the work log, then leave a task-unit commit.
