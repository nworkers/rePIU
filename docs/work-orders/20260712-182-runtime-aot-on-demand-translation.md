# Runtime AOT on-demand translation 작업 지시

1. arbitrary-entry AOT planning API를 추가합니다.
2. Win32 cache placement에 예약 용량과 append API를 추가합니다.
3. live runtime arena snapshot에서 동적 CFG/image를 생성합니다.
4. 기존 map/fixup과 병합하고 W^X 및 decode 검증을 유지합니다.
5. 미매핑 target re-entry 전에 동적 변환을 시도합니다.
6. 동적 변환/성공/실패/추가 byte telemetry를 기록합니다.
7. legacy, static AOT, dynamic AOT를 동일 시간으로 비교합니다.
8. 문서와 작업 로그를 갱신하고 커밋합니다.

# Runtime AOT On-Demand Translation Work Order

Add arbitrary-entry planning, reserved appendable Win32 cache placement, live arena snapshot translation and map merging, on-demand re-entry, telemetry, same-duration comparison, documentation, verification, and a commit while retaining all legacy paths.
