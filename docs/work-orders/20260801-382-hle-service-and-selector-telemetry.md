# 20260801-382 작업 지시: HLE 서비스 및 selector telemetry / Work Order: HLE service and selector telemetry

설계: [20260801-382-hle-service-and-selector-telemetry.md](../design/20260801-382-hle-service-and-selector-telemetry.md)

## 한국어

1. ThreadContext와 실행 결과 snapshot에 고정 크기 histogram 및 합계 필드를 추가합니다.
2. 기존 DOS, segment, Port I/O 기록 지점에서 카운터를 증가시킵니다.
3. 종료 로그에 상위 여덟 항목과 합계를 출력합니다.
4. x86 Debug 빌드와 AOT probe를 실행합니다.

## English

1. Add fixed-size histograms and totals to ThreadContext and the execution-result snapshot.
2. Increment them at existing DOS, segment, and Port-I/O record sites.
3. Print the top eight entries and totals at shutdown.
4. Run the x86 Debug build and AOT probe.
