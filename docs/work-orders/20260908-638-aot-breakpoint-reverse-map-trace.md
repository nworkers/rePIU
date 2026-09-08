# Task 638 작업 지시: AOT breakpoint 역매핑 추적

설계: [20260908-638](../design/20260908-638-aot-breakpoint-reverse-map-trace.md)

## 한국어

1. `REPIU_AOT_FAULT_TRACE`가 AOT cache breakpoint도 선택하도록 확장합니다.
2. exact 및 previous cache 주소의 guest mapping, provenance와 cache tail 거리를 출력합니다.
3. `REPIU_AOT_FAULT_TRACE_ADDRESS`로 한 cache 주소를 선택할 수 있게 합니다.
4. Linux x64 Debug 빌드와 core probe를 실행합니다.
5. 실제 `pumpit2a`에서 새 breakpoint를 재현하고 결과를 분석 및 작업 로그에 남깁니다.
6. 실행 제어 변경이나 breakpoint 자동 복구는 하지 않습니다.

## English

1. Extend `REPIU_AOT_FAULT_TRACE` to select AOT-cache breakpoints.
2. Print guest mapping, provenance, and cache-tail distance for exact and
   previous cache addresses.
3. Add `REPIU_AOT_FAULT_TRACE_ADDRESS` to select one cache address.
4. Build Linux x64 Debug and run the core probe.
5. Reproduce the new breakpoint with real `pumpit2a` and record the result in
   the analysis and work log.
6. Do not change execution control or automatically recover the breakpoint.
